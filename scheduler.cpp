#include "scheduler.h"

// #include <iostream>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>

#include "config.h"
#include "io_api.h"
#include "os_specific.h"
#include "task_manager.h"
#include "util.h"
#include "worker.h"

// Creates new Scheduler for each bit available in available CPUs mask.
//
// clang-format off
Schedulers::Schedulers() noexcept try
    : m_system_cpus_count{os::cpus_count()}
    , m_avail_cpus_mask{Cpu_Mask{os::cpus_avail_mask()} & CFG_allowed_cpus}
{
    for (uint64_t cpu_id = 0; cpu_id < m_avail_cpus_mask.size(); ++cpu_id)
        if (m_avail_cpus_mask.test(cpu_id))
            m_schedulers.emplace_back(std::make_unique<Scheduler>(*this, cpu_id));
}
catch (...) {
    std::terminate();
}

// clang-format on

Scheduler& Schedulers::min_load_scheduler() const noexcept
{
    // NOLINTNEXTLINE(readability-suspicious-call-argument)
    const auto cmp = [](const auto& left, const auto& right) {
        return left->load() < right->load();
    };

    return **std::min_element(m_schedulers.begin(), m_schedulers.end(), cmp);
}

[[nodiscard]] uint32_t Schedulers::workers_count() const noexcept
{
    uint32_t c = 0;
    for (const auto& scheduler : m_schedulers)
        c += scheduler->workers_count();

    return c;
}

[[nodiscard]] uint32_t Schedulers::cpus_count() const noexcept
{
    return m_schedulers.size();
}

// Returns whether all schedulers are idle.
// Every time scheduler gets idle, it increases m_idle_schedulers, so we just check whether it
// matches shedulers size.
// There is a tricky case that we must handle: If we have only 1 running scheduler (others are idle)
// and user executes async task which gets scheduled on a sleeping scheduler, it might happen that
// all schedulers becomes idle (because waking up scheduler takes time), so we are not allowed to
// exit. Instead, we must lock all schedulers and check their state and tasks before exiting, to be
// sure that there is no more work to do. Note that locking schedulers 1 by 1 and checking
// their tasks would not solve the problem, because we might miss execution of async task which can
// execute another async task etc.
//
bool Schedulers::all_idle() noexcept
{
    if (m_idle_schedulers.load(std::memory_order_relaxed) < m_schedulers.size())
        return false;

    using namespace std::ranges;

    for_each(m_schedulers, [](auto& s) { s->m_mtx.lock(); });
    const bool r = all_of(m_schedulers, [&](auto& s) { return s->idle() && !s->has_tasks(); });
    for_each(m_schedulers, [](auto& s) { s->m_mtx.unlock(); });

    return r;
}

// Increases number of idle schedulers.
// If all schedulers are idle, it means that there is no more work to do and we can exit all
// schedulers.
// Note that we are not locking Schedulers::m_mtx before notify, because we might enter a deadlock:
//        First thread                                  Second thread
// 1. lock(Schedulers::mtx) in wait_exit()
//                                            2. lock(Scheduler::mtx) in sleep();
// 3. lock(Scheduler::mtx) in all_idle()
//                                            4. lock(Schedulers::mtx) in signal_idle()
// Both threads are waiting for a mutex which is owned by other thread, so we are deadlocked.
// We should be safe this way without locking, because this function is called under
// lock in sleep().
//
void Schedulers::signal_idle() noexcept
{
    const uint32_t idle_count = m_idle_schedulers.fetch_add(1, std::memory_order_relaxed) + 1;

    if (idle_count == m_schedulers.size())
        m_cv.notify_one();
}

void Schedulers::signal_running() noexcept
{
    m_idle_schedulers.fetch_sub(1, std::memory_order_relaxed);
}

void Schedulers::wait_exit()
{
    std::unique_lock<std::mutex> lock{m_mtx};
    m_cv.wait(lock, [&] { return all_idle(); });
}

// Creates scheduler and workers for provided CPU.
// After workers are created, starts single worker from idle queue
// and waits until worker (scheduler) goes to sleep.
//
Scheduler::Scheduler(Schedulers& schedulers, uint64_t cpu_id)
    : m_schedulers{schedulers}
    , m_cpu{cpu_id}
{
    for (uint32_t i = 0; i < CFG_workers_per_cpu; ++i)
        m_workers.push_back(std::make_unique<Worker>(i, *this));

    {
        const std::unique_lock<std::mutex> lock{m_workers_mtx};
        m_worker = m_idle_queue.front();
        m_worker->notify(lock);
    }

    std::unique_lock<std::mutex> lock{m_mtx};
    m_cv.wait(lock, [&] { return !m_running; });
}

Scheduler::~Scheduler() noexcept
{
    signal_exit();
    notify();
}

bool Scheduler::has_idle_workers() const noexcept
{
    return !m_idle_queue.empty();
}

bool Scheduler::has_runnable_workers() const noexcept
{
    return !m_runnable_queue.empty();
}

bool Scheduler::has_waiting_workers() const noexcept
{
    return !m_waiting_queue.empty();
}

bool Scheduler::has_pending_io_workers() const noexcept
{
    return !m_pending_io_queue.empty();
}

void Scheduler::save_runnable(Worker* worker)
{
    m_runnable_queue.push_back(worker);
    worker->set_state(Worker::State::runnable);
}

template<bool back>
void Scheduler::save_idle(Worker* worker)
{
    if constexpr (back)
        m_idle_queue.push_back(worker);
    else
        m_idle_queue.push_front(worker);

    worker->set_state(Worker::State::idle);
}

void Scheduler::save_waiting(Worker* worker)
{
    m_waiting_queue.push_back(worker);
    worker->set_state(Worker::State::waiting);
}

void Scheduler::save_pending_io(Worker* worker)
{
    m_pending_io_queue.push_back(worker);
    worker->set_state(Worker::State::pending_io);
}

void Scheduler::prepare_next_worker() noexcept
{
    m_worker = m_runnable_queue.front();
    m_runnable_queue.pop_front();

    m_worker->set_state(Worker::State::running);
}

void Scheduler::enqueue_task(const std::shared_ptr<Task>& task)
{
    m_tasks.enque(task);
    notify();
}

std::shared_ptr<Task> Scheduler::next_task() noexcept
{
    return m_tasks.deque();
}

bool Scheduler::has_tasks() const noexcept
{
    return !m_tasks.empty();
}

void Scheduler::schedule_idle_worker()
{
    Worker* worker = m_idle_queue.front();
    m_idle_queue.pop_front();

    worker->m_task = next_task();
    save_runnable(worker);
}

// Moves workers from pending_io to runnable queue if worker's I/O is completed.
//
void Scheduler::schedule_io_workers()
{
    auto io_completed = [](Worker* worker) {
        worker->m_io_request->update();
        return !worker->m_io_request->pending();
    };

    auto begin = m_pending_io_queue.begin();
    auto end = m_pending_io_queue.end();

    for (auto it = std::find_if(begin, end, io_completed); it != end;
         it = std::find_if(it, end, io_completed)) {
        save_runnable(*it);
        it = m_pending_io_queue.erase(it);
    }
}

// Moves workers from waiting to runnable queue if worker's wait is done.
//
void Scheduler::schedule_waiting_workers()
{
    auto checker = [](Worker* worker) { return worker->check_wait_info(); };

    auto begin = m_waiting_queue.begin();
    auto end = m_waiting_queue.end();

    for (auto it = std::find_if(begin, end, checker); it != end;
         it = std::find_if(it, end, checker)) {
        save_runnable(*it);
        it = m_waiting_queue.erase(it);
    }
}

// Schedules single idle worker with the earliest enqued task.
// This is done only if there is no work to do (no previous tasks that got scheduled out due to I/O,
// yield, wait on mutex or condition_variable etc. and are now ready to continue). This way we are
// prioritizing execution of old unfinished tasks, instead of beeing "fair" and giving all new tasks
// the same priority as for the old ones.
//
void Scheduler::schedule_idle_workers()
{
    if (!has_runnable_workers() && has_idle_workers() && has_tasks())
        schedule_idle_worker();
}

void Scheduler::schedule_workers()
{
    schedule_io_workers();
    schedule_waiting_workers();
    schedule_idle_workers();
}

void Scheduler::schedule()
{
    schedule_workers();

    while (!has_runnable_workers() && !should_exit()) {
        sleep();
        schedule_workers();
    }

    if (should_exit()) [[unlikely]]
        exit();
    else [[likely]]
        prepare_next_worker();
}

void Scheduler::wait(std::unique_lock<std::mutex>& lock, Time_point abs_time)
{
    m_running = false;
    m_cv.wait_until(lock, abs_time, [&] { return m_running; });
}

void Scheduler::notify([[maybe_unused]] const std::unique_lock<std::mutex>& lock) noexcept
{
    m_running = true;
    m_cv.notify_one();
}

// Notify API for other components that should wake up scheduler.
//
void Scheduler::notify()
{
    const std::unique_lock<std::mutex> lock{m_mtx};
    notify(lock);
}

// Returns struct which holds info about workers in a waiting queue (whether any condition is
// signaled and earliest wait time point for all waiters).
//
auto Scheduler::waiters_info() const noexcept
{
    struct waiters_info {
        bool m_cond{false};
        Time_point m_earliest_wait{Time_point::max()};
    } result;

    for (const auto& worker : m_waiting_queue) {
        result.m_cond |= worker->check_cond();
        result.m_earliest_wait = std::min(result.m_earliest_wait, worker->sleep_time_point());
    }

    return result;
}

// Sleep if there is no work.
// Our sleep is determined by waiting workers. We have to wait until any waiting worker's condition
// is signaled or earlies sleep time expires for all waiting workers. This function is called after
// initial workers scheduling when there are no runnable workers.
//
// NOTE:
// Since scheduler is going to sleep, we need to signal it every time new task arrives,
// earlies sleep time expires, any condition variable or exit is signaled.
// At any moment new task can come, condition might me signaled etc., so we need to check
// everything under lock before going to sleep and all scheduler notifications must be done under
// lock to avoid race conditions.
// In order to allow precise sleep time (15ms or less) we must include idle_sleep_threshold.
// Since windows clock is not that precise (clock cycle is ~15ms) and OS scheduler can delay
// wake up of any sleeping thread, we choose arbitrary value for idle_sleep_threshold of 20ms.
// Note that all functions trying to notify scheduler are blocked until all of our checks
// are done under lock.
// One potential problem is that OS scheduler might schedule worker out if
// mutex is locked in this function and we are, for example, trying to add new task from another
// worker which must notify scheduler.
//
void Scheduler::sleep() noexcept
{
    // For pending I/O workers, we will just keep scheduling until I/O is done.
    // TODO: Check whether we can aford to sleep for I/O operations.
    //
    if (has_pending_io_workers())
        return;

    std::unique_lock<std::mutex> lock{m_mtx};

    if (has_tasks() && has_idle_workers())
        return;

    // TODO: It should be enough to just check waiting workers, since if there are tasks and there
    // are no waiting workers, I don't know where they are.
    //
    if (exit_signaled() && !has_waiting_workers() && !has_tasks())
        return;

    if (has_waiting_workers()) {
        auto wait_info{waiters_info()};
        wait_info.m_earliest_wait -= CFG_idle_sleep_threshold;

        if (wait_info.m_cond || now() >= wait_info.m_earliest_wait)
            return;

        set_state(State::idle_wait);
        wait(lock, wait_info.m_earliest_wait);
        set_state(State::running);
    }
    else {
        if (initializing()) [[unlikely]]
            notify(lock); // Notify thread (waiting in scheduler constructor) to continue.

        set_state(State::idle_sleep);
        m_schedulers.signal_idle();

        wait(lock);

        m_schedulers.signal_running();
        set_state(State::running);
    }
}

void Scheduler::set_state(State state) noexcept
{
    m_state = state;
}

// NOLINTBEGIN
// clang-format off
class Scheduler_Loads {
public:
    static constexpr int loads_size = int(Worker::State::exiting) + 1;

    constexpr inline Scheduler_Loads()
    {
        m_loads[int(Worker::State::initializing)] =  0;
        m_loads[int(Worker::State::idle)]         =  0;
        m_loads[int(Worker::State::waiting)]      =  1;
        m_loads[int(Worker::State::pending_io)]   =  2;
        m_loads[int(Worker::State::runnable)]     = 10;
        m_loads[int(Worker::State::running)]      = 10;
        m_loads[int(Worker::State::exiting)]      =  0;
    }

    constexpr inline int operator[](const Worker::State state) const { return m_loads[int(state)]; }

private:
    std::array<uint8_t, loads_size> m_loads{};
};

// clang-format on
// NOLINTEND

static constexpr Scheduler_Loads Loads;

// Sets new scheduler load based on previous and new worker state.
//
void Scheduler::manage_load(Worker::State prev_state, Worker::State new_state) noexcept
{
    m_load += Loads[new_state] - Loads[prev_state];
}

// TODO: Make m_load atomic, to avoid race conditions.
//
uint64_t Scheduler::load() const noexcept
{
    return m_load + m_tasks.size() * Loads[Worker::State::runnable];
}

// Switches thread execution context from previous worker to current.
//
// Notes:
// There is a single mutex on scheduler used for workers synchronization and every worker has it's
// own condition variable. In order to atomically suspend single worker thread (go to sleep by
// calling wait) and wake up next, we will take lock on mutex before notifying another thread to
// wake up. Condition_variable::wait function guarantees that it will unlock mutex and go to sleep
// atomically and it also guarantees that it will take lock on mutex when wait is done. So when we
// notify another thread to wake up we are already holding lock on mutex (and notified thread can
// not wake up until we release lock) so mutex will be unlocked only when we call wait on this
// thread, which will release lock and wake another thread.
//
void Scheduler::context_switch(Worker* prev_worker)
{
    std::unique_lock<std::mutex> lock{m_workers_mtx};
    m_worker->notify(lock);
    prev_worker->wait(lock);
}

// Synchronization point for the workers that are beeing initialized.
// We will return whether scheduling is needed or not.
//
// Notes:
// When worker is started for the first time, it will be parked in this function
// waiting on condition variable. We will later decide whether to proceed with scheduling based on
// return value. Since only first started worker on scheduler should enter scheduling code (other
// workers will already be scheduled when it's their turn to run), we will use flag
// m_workers_started to help us do this. Also, we are going to notify scheduler thread to continue
// when we are safely parked, since it is blocked on a condition variable waiting for us.
//
bool Scheduler::sync_init(Worker* worker)
{
    if (!initializing()) [[likely]]
        return true; // proceed with scheduling.

    std::unique_lock<std::mutex> lock{m_workers_mtx};
    worker->notify(lock); // Notify thread (waiting in worker constructor) to continue
    worker->wait(lock);   // and go to sleep.

    return m_workers_started ? false : (m_workers_started = true);
}

// Save worker to proper queue based on synchronization context.
//
template<Sync_context ctx>
void Scheduler::save_worker(Worker* worker)
{
    if constexpr (ctx == Sync_context::main)
        if (initializing()) [[unlikely]]
            save_idle<true>(worker);
        else [[likely]]
            save_idle<false>(worker);
    else if constexpr (ctx == Sync_context::yield)
        save_runnable(worker);
    else if constexpr (ctx == Sync_context::wait_cond_or_sleep)
        save_waiting(worker);
    else if constexpr (ctx == Sync_context::io)
        save_pending_io(worker);
}

// Synchronization point for the workers.
// We will save current worker to proper queue, schedule workers and context switch to
// next worker if needed. For workers initialization, we will enter sync_init function.
//
template<Sync_context ctx>
void Scheduler::sync(Worker* worker)
{
    save_worker<ctx>(worker);

    if (!sync_init(worker)) [[unlikely]]
        return;

    schedule();

    if (m_worker != worker)
        context_switch(worker);
}

// Sets exit state for scheduler and current worker.
// Other workers will set their exit state in worker destructor.
//
void Scheduler::exit()
{
    set_state(State::exiting);
    m_worker->set_state(Worker::State::exiting);
}

bool Scheduler::has_work() const noexcept
{
    return has_runnable_workers() || has_waiting_workers() || has_pending_io_workers() ||
           has_tasks();
}

bool Scheduler::should_exit() const noexcept
{
    return exit_signaled() && !has_work();
}

template void Scheduler::sync<Sync_context::main>(Worker* worker);
template void Scheduler::sync<Sync_context::yield>(Worker* worker);
template void Scheduler::sync<Sync_context::wait_cond_or_sleep>(Worker* worker);
template void Scheduler::sync<Sync_context::io>(Worker* worker);

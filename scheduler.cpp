/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "scheduler.hpp"

// #include <iostream>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "config.hpp"
#include "intrusive_list.hpp"
#include "io.hpp"
#include "mutex.hpp"
#include "options.hpp"
#include "os_specific.hpp"
#include "task.hpp"
#include "types.hpp"
#include "util.hpp"
#include "worker.hpp"

namespace ums {

namespace {

/* Global schedulers */
std::unique_ptr<Schedulers> schedulers; // NOLINT

} // namespace

/**
 * Creates new Scheduler for each bit available in CPUs availability mask.
 */
Schedulers::Schedulers(Options opt) noexcept /* clang-format off */ try
    : m_system_cpus_count{std::min(os::cpus_count(), CFG_max_supported_cpus)}
    , m_cpus_avail_mask{Cpu_Mask{os::cpus_avail_mask()} & Cpu_Mask{CFG_allowed_cpus_mask}}
{
    usize cpu_id = 0;
    u64 sch_created = 0;

    while (cpu_id < m_cpus_avail_mask.size() && sch_created < opt.schedulers_count()) {
        if (m_cpus_avail_mask.test(cpu_id)) {
            m_schedulers.emplace_back(std::make_unique<Scheduler>(this, cpu_id, opt.workers_per_scheduler()));
            ++sch_created;
        }

        ++cpu_id;
    }
}
catch (...) {
    std::terminate();
} /* clang-format on */

/**
 * TODO: Prefer this scheduler if this scheduler has the same load as min scheduler, since we can
 * than execute task instantly. Also, make invokation on the same thread that schedules task
 * possible and check if it makes sense. Also, make scheduling policy: async and concurent and
 * decide based on that.
 */
Scheduler& Schedulers::min_load_scheduler() const noexcept
{
    TZoneScopedC(tracy::Color::DarkGreen);

    const auto cmp = [](const auto& left, const auto& right) noexcept {
        return left->load() < right->load();
    };

    return **std::ranges::min_element(m_schedulers, cmp);
}

[[nodiscard]] u32 Schedulers::workers_per_cpu_count() const noexcept
{
    return m_schedulers.front()->workers_count();
}

[[nodiscard]] u32 Schedulers::workers_count() const noexcept
{
    return m_schedulers.size() * workers_per_cpu_count();
}

[[nodiscard]] u32 Schedulers::cpus_count() const noexcept
{
    return m_schedulers.size();
}

/**
 * Returns whether all schedulers are idle.
 * Every time scheduler gets idle, it increases m_idle_schedulers, so we just check whether it
 * matches shedulers size.
 * There is a tricky case that we must handle: If we have only 1 running scheduler (others are idle)
 * and user executes async task which gets scheduled on a sleeping scheduler, it might happen that
 * all schedulers becomes idle (because waking up scheduler takes time), so we are not allowed to
 * exit. Instead, we must lock all schedulers and check their state and tasks before exiting, to be
 * sure that there is no more work to do. Note that locking schedulers 1 by 1 and checking
 * their tasks would not solve the problem, because we might miss execution of async task which can
 * execute another async task etc.
 */
bool Schedulers::all_idle() noexcept
{
    if (m_idle_schedulers.load(std::memory_order_relaxed) < m_schedulers.size())
        return false;

    using namespace std::ranges;

    if (any_of(m_schedulers, [&](const auto& s) noexcept { return s->has_tasks(); }))
        return false;

    for_each(m_schedulers, [](auto& s) noexcept { s->m_mtx.lock(); });
    const bool r =
        all_of(m_schedulers, [&](const auto& s) noexcept { return s->idle() && !s->has_tasks(); });
    for_each(m_schedulers, [](auto& s) noexcept { s->m_mtx.unlock(); });

    return r;
}

/**
 * Increases the number of idle schedulers and notifies main thread (waiting in wait_exit)
 * if all shcedulers are idle.
 */
void Schedulers::signal_idle() noexcept
{
    const u32 idle_count = m_idle_schedulers.fetch_add(1, std::memory_order_relaxed) + 1;
    if (idle_count < m_schedulers.size())
        return;

    {
        const std::scoped_lock<std::mutex> lock{m_mtx};
        m_check_idle = true;
    }

    m_cv.notify_one();
}

void Schedulers::signal_running() noexcept
{
    m_idle_schedulers.fetch_sub(1, std::memory_order_relaxed);
}

/**
 * Waits for schedulers to exit.
 * Schedulers will notify us only when all of them are idle. After notification, we must check
 * whether all schedulers are really idle (see all_idle for details). In order to prevent deadlock
 * while checking for idle states, we must unlock our mutex before calling all idle, since signal
 * idle would require it.
 * Of course, there are a lot of edge cases that are handled with this implementation, and
 * explaining all of them would be a waste of time.
 */
void Schedulers::wait_exit()
{
    std::unique_lock<std::mutex> lock{m_mtx};
    m_cv.wait(lock, [&] {
        if (!m_check_idle)
            return false;

        m_check_idle = false;

        {
            const Scoped_unlock<std::mutex> unlock{m_mtx};
            return all_idle();
        }
    });
}

std::string Scheduler::state_to_string(State state)
{
    switch (state) { // clang-format off
    case State::initializing: return "initializing";
    case State::running:      return "running";
    case State::idle_wait:    return "idle_wait";
    case State::idle_sleep:   return "idle_sleep";
    case State::exiting:      return "exiting";
    default:                  return "unkown";
    } // clang-format on
}

/**
 * Creates scheduler and workers for provided CPU.
 * After workers are created, starts single worker from idle queue
 * and waits until worker (scheduler) goes to sleep.
 * TODO: Speed this up with parallel workes creation.
 */
Scheduler::Scheduler(Schedulers* schedulers, u64 cpu_id,
                     Options::Workers_per_scheduler workers_count)
    : m_schedulers{schedulers}
    , m_cpu{cpu_id}
    , m_workers{workers_count}
{
    for (u32 i = 0; i < workers_count; ++i)
        m_workers.emplace_back(i, this);

    {
        const std::unique_lock<std::mutex> lock{m_workers_mtx};
        m_worker = &m_idle_queue.front();
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

bool Scheduler::has_yielded_workers() const noexcept
{
    return m_yielded_worker != nullptr;
}

void Scheduler::park_runnable(Worker* worker)
{
    m_runnable_queue.push_back(*worker);
    set_worker_state(worker, Worker::State::runnable);
}

/**
 * Parks worker to idle queue. If worker has completed task, notify user and release task.
 */
template<bool back>
void Scheduler::park_idle(Worker* worker)
{
    if constexpr (back)
        m_idle_queue.push_back(*worker);
    else
        m_idle_queue.push_front(*worker);

    set_worker_state(worker, Worker::State::idle);
}

void Scheduler::park_waiting(Worker* worker)
{
    m_waiting_queue.push_back(*worker);
    set_worker_state(worker, Worker::State::waiting);
}

void Scheduler::park_pending_io(Worker* worker)
{
    m_pending_io_queue.push_back(*worker);
    set_worker_state(worker, Worker::State::pending_io);
}

void Scheduler::park_yielded(Worker* worker)
{
    m_yielded_worker = worker;
    set_worker_state(worker, Worker::State::yielded);
}

void Scheduler::prepare_next_worker() noexcept
{
    m_worker = &m_runnable_queue.front();
    m_runnable_queue.pop_front();

    set_worker_state(m_worker, Worker::State::running);
}

void Scheduler::set_worker_state(Worker* worker, Worker::State state) noexcept
{
    manage_load(worker->state(), state);
    worker->set_state(state);
}

void Scheduler::enqueue_task(std::shared_ptr<TaskBase> task)
{
    m_tasks.enque(std::move(task));
    TTracyMessageLC(tracy_str("Enqued task on sch {}", id()), tracy::Color::Green1);

    notify();
}

std::shared_ptr<TaskBase> Scheduler::next_task() noexcept
{
    TTracyMessageLC(tracy_str("Dequed (maybe) task from sch {}", id()), tracy::Color::Green1);
    return m_tasks.deque();
}

bool Scheduler::has_tasks() const noexcept
{
    return !m_tasks.empty();
}

/**
 * Returns whether this scheduler has stealable work (work to be stolen from other schedulers).
 * If we have more than 1 task, always allow stealing. If we have a single task, allow stealing only
 * if we are already executing task (load > 0).
 * Note that this must be thread safe, because it will be called from other schedulers.
 */
bool Scheduler::has_stealable_work() const noexcept
{
    const u64 tasks_size = m_tasks.size();
    if (tasks_size == 0)
        return false;

    if (tasks_size == 1)
        return state_load() > 0;

    return true;
}

/**
 * Schedules idle worker with provided task or with next task from tasks queue if task is empty.
 * We must check whether task exists even if we are getting task from our queue, because someone
 * might have stolen our task in the meantime.
 */
void Scheduler::schedule_idle_worker(std::shared_ptr<TaskBase> task)
{
    TZoneScopedC(tracy::Color::Blue1);

    if (!task)
        task = next_task();

    if (task) {
        Worker& worker = m_idle_queue.front();
        m_idle_queue.pop_front();

        worker.m_task = std::move(task);
        park_runnable(&worker);
    }
}

/**
 * Moves workers from pending_io to runnable queue if worker's I/O is completed.
 */
void Scheduler::schedule_io_workers()
{
    TZoneScopedC(tracy::Color::Blue1);

    auto io_completed = [](Worker& worker) {
        worker.m_io_request->update();
        return !worker.m_io_request->pending();
    };

    auto begin = m_pending_io_queue.begin();
    auto end = m_pending_io_queue.end();

    for (auto it = std::find_if(begin, end, io_completed); it != end;
         it = std::find_if(it, end, io_completed)) {
        Worker& worker = *it;
        it = m_pending_io_queue.erase(it);
        park_runnable(&worker);
    }
}

/**
 * Moves workers from waiting to runnable queue if worker's wait is done.
 */
void Scheduler::schedule_waiting_workers()
{
    TZoneScopedC(tracy::Color::Blue1);

    auto checker = [](Worker& worker) { return worker.check_wait_info(); };

    auto begin = m_waiting_queue.begin();
    auto end = m_waiting_queue.end();

    for (auto it = std::find_if(begin, end, checker); it != end;
         it = std::find_if(it, end, checker)) {
        Worker& worker = *it;
        it = m_waiting_queue.erase(it);
        park_runnable(&worker);
    }
}

/**
 * Schedules single idle worker with the earliest enqued task.
 * This is done only if there is no work to do (no previous tasks that got scheduled out due to I/O,
 * yield, wait on mutex or condition_variable etc. and are now ready to continue). This way we are
 * prioritizing execution of old unfinished tasks, instead of beeing "fair" and giving all new tasks
 * the same priority as for the old ones.
 */
void Scheduler::schedule_idle_workers()
{
    TZoneScopedC(tracy::Color::Blue1);

    if (!has_runnable_workers() && has_idle_workers() && has_tasks())
        schedule_idle_worker();
}

/**
 * Schedules yielded worker by moving it to the runnable queue.
 */
void Scheduler::schedule_yielded_workers()
{
    TZoneScopedC(tracy::Color::Blue1);

    if (m_yielded_worker != nullptr) {
        park_runnable(m_yielded_worker);
        m_yielded_worker = nullptr;
    }
}

/**
 * Steals work (single task) from other scheduler if there are no runnable workers on this
 * scheduler.
 */
void Scheduler::steal_work()
{
    if constexpr (!FS_work_stealing_allowed)
        return;

    TZoneScopedC(tracy::Color::DarkRed);

    if (has_runnable_workers() || !has_idle_workers())
        return;

    auto others_with_work = [&](const auto& other) noexcept {
        return id() != other->id() && other->has_stealable_work();
    };

    for (const auto& other : m_schedulers->filter(others_with_work)) {
        if (auto task{other->next_task()}) {
            schedule_idle_worker(std::move(task));
            TTracyMessageLC(tracy_str("Stolen work S{} -> S{}", other->id(), id()),
                            tracy::Color::Red1);
            return;
        }
    }
}

void Scheduler::schedule_workers()
{
    TZoneScopedC(tracy::Color::Blue1);

    schedule_io_workers();
    schedule_waiting_workers();
    schedule_idle_workers();
    schedule_yielded_workers();

    steal_work();
}

void Scheduler::prepare_exec() noexcept
{
    invalidate_idle_timer();
    prepare_next_worker();
}

void Scheduler::schedule()
{
    TZoneScopedC(tracy::Color::Blue1);

    while (true) {
        schedule_workers();

        if (has_runnable_workers())
            return prepare_exec();

        sleep();

        if (should_exit()) [[unlikely]]
            return exit();
    }
}

void Scheduler::wait_until(std::unique_lock<std::mutex>& lock, Time_point abs_time)
{
    m_running = false;
    [[maybe_unused]] const bool wait_res =
        m_cv.wait_until(lock, abs_time, [&] { return m_running; });

    assert(m_running || !wait_res); // Either someone notified us or we timed out on wait.
    m_running = true;
}

/**
 * Notify API for other components that should wake up scheduler.
 */
void Scheduler::notify()
{
    TZoneScopedC(tracy::Color::DarkGreen);

    {
        const std::unique_lock<std::mutex> lock{m_mtx};
        m_running = true;
    }

    m_cv.notify_one();
}

/**
 * Returns struct which holds info about workers in a waiting queue (whether any condition is
 * signaled and earliest wait time point for all waiters).
 */
auto Scheduler::waiters_info() const noexcept
{
    struct waiters_info {
        bool m_cond{false};
        Time_point m_earliest_wait{Time_point::max()};
    } result;

    for (const auto& worker : m_waiting_queue) {
        result.m_cond |= worker.check_cond();
        result.m_earliest_wait = std::min(result.m_earliest_wait, worker.sleep_time_point());
    }

    return result;
}

/**
 * Checks whether scheduler should idle spin.
 * Idle spin is necessary for eficient use of scheduler. After task is done, if we go to sleep
 * immediately, we will miss the oportinity to execute next user task which might come right after
 * the previous task is done if user executes tasks sequentially.
 */
bool Scheduler::should_idle_spin() noexcept
{
    if constexpr (!FS_idle_spinning_allowed)
        return false;

    if (initializing()) [[unlikely]]
        return false;

    if (m_idle_start_time == Time_point::max())
        m_idle_start_time = now();

    if (now() > m_idle_start_time + CFG_idle_spin_threshold)
        return false;

    /**
     * TODO: Check whether we should CPU pause here, to lower power consumption.
     */
    TTracyMessageLC(tracy_str("Idle spinning."), tracy::Color::Red1);
    return true;
}

/**
 * Sleep if there is no work.
 * Our sleep is determined by waiting workers. We have to wait until any waiting worker's
 * condition is signaled or earlies sleep time expires for all waiting workers. This function is
 * called after initial workers scheduling when there are no runnable workers.
 *
 * NOTE:
 * Since scheduler is going to sleep, we need to signal it every time new task arrives,
 * earlies sleep time expires, any condition variable or exit is signaled.
 * At any moment new task can come, condition might be signaled etc., so we need to check
 * everything under lock before going to sleep and all scheduler notifications must be done
 * under lock to avoid race conditions. In order to allow precise sleep time (15ms or less) we
 * must include idle_sleep_threshold. Since windows clock is not that precise (clock cycle is
 * ~15ms) and OS scheduler can delay wakeup of any sleeping thread, we choose arbitrary value
 * for idle_sleep_threshold of 20ms. Note that all functions trying to notify scheduler are
 * blocked until all of our checks are done under lock. One potential problem is that OS
 * scheduler might schedule worker out if mutex is locked in this function and we are, for
 * example, trying to add new task from another worker which must notify scheduler.
 * This problem maybe can be solved with global run queue.
 */
void Scheduler::sleep() noexcept
{
    /**
     * For pending I/O workers, we will just keep scheduling until I/O is done.
     * TODO: Check whether we can aford to sleep for I/O operations.
     */
    if (has_pending_io_workers())
        return;

    if (should_idle_spin())
        return;

    TZoneScopedC(tracy::Color::Gray);
    std::unique_lock<std::mutex> lock{m_mtx};

    if (has_tasks() && has_idle_workers())
        return;

    /**
     * TODO: It should be enough to just check waiting workers, since if there are tasks and
     * there are no waiting workers, I don't know where they are.
     */
    if (exit_signaled() && !has_waiting_workers() && !has_tasks())
        return;

    if (has_waiting_workers()) {
        auto wait_info{waiters_info()};
        wait_info.m_earliest_wait -= CFG_idle_sleep_threshold;

        if (wait_info.m_cond || now() >= wait_info.m_earliest_wait)
            return;

        set_state(State::idle_wait);
        wait_until(lock, wait_info.m_earliest_wait);
        set_state(State::running);

        return;
    }

    if (initializing()) [[unlikely]]
        m_cv.notify_one(); // Notify thread (waiting in scheduler constructor) to continue.

    set_state(State::idle_sleep);
    m_schedulers->signal_idle();

    wait(lock, [&] { return has_tasks() || exit_signaled(); });

    m_schedulers->signal_running();
    set_state(State::running);
}

void Scheduler::set_state(State state) noexcept
{
    m_state = state;
}

// NOLINTBEGIN

class Load { /* clang-format off */
public:
    static constexpr i32 task = 10; // Single task load.

    static constexpr i32 loads_size = i32(Worker::State::exiting) + 1;

    constexpr inline Load()
    {
        m_loads[i32(Worker::State::initializing)] =  0;
        m_loads[i32(Worker::State::idle)]         =  0;
        m_loads[i32(Worker::State::waiting)]      =  1;
        m_loads[i32(Worker::State::pending_io)]   =  2;
        m_loads[i32(Worker::State::yielded)]      = 10;
        m_loads[i32(Worker::State::runnable)]     = 10;
        m_loads[i32(Worker::State::running)]      = 10;
        m_loads[i32(Worker::State::exiting)]      =  0;
    }

    constexpr inline i32 operator[](const Worker::State state) const noexcept { return m_loads[i32(state)]; }

private:
    std::array<i32, loads_size> m_loads{};
}; /* clang-format on */

// NOLINTEND

static constexpr Load Loads;

// Sets new scheduler load based on previous and new worker state.
//
void Scheduler::manage_load(Worker::State prev_state, Worker::State new_state) noexcept
{
    m_state_load.fetch_add(Loads[new_state] - Loads[prev_state], std::memory_order_relaxed);
}

i32 Scheduler::tasks_load() const noexcept
{
    return static_cast<i32>(m_tasks.size() * Load::task);
}

i32 Scheduler::state_load() const noexcept
{
    return m_state_load.load(std::memory_order_relaxed);
}

i32 Scheduler::load() const noexcept
{
    return state_load() + tasks_load();
}

/**
 * Switches thread execution context from previous worker to current.
 *
 * Notes:
 * There is a single mutex on scheduler used for workers synchronization and every worker has
 * it's own condition variable. In order to atomically suspend single worker thread (go to sleep
 * by calling wait) and wake up next, we will take lock on mutex before notifying another thread
 * to wake up. Condition_variable::wait function guarantees that it will unlock mutex and go to
 * sleep atomically and it also guarantees that it will take lock on mutex when wait is done. So
 * when we notify another thread to wake up we are already holding lock on mutex (and notified
 * thread can not wake up until we release lock) so mutex will be unlocked only when we call
 * wait on this thread, which will release lock and wake up another thread.
 */
void Scheduler::context_switch(Worker* prev_worker)
{
    TTracyMessageLC(tracy_str("Context switch W{} -> W{}", prev_worker->id(), m_worker->id()),
                    tracy::Color::Green1);

    std::unique_lock<std::mutex> lock{m_workers_mtx};
    m_worker->notify(lock);
    prev_worker->wait(lock);
}

/**
 * Synchronization point for the workers that are beeing initialized.
 * We will return whether scheduling is needed or not.
 *
 * Notes:
 * When worker is started for the first time, it will be parked in this function
 * waiting on condition variable. We will later decide whether to proceed with scheduling based
 * on return value. Since only first started worker on scheduler should enter scheduling code
 * (other workers will already be scheduled when it's their turn to run), we will use flag
 * m_workers_started to help us do this. Also, we are going to notify scheduler thread to
 * continue when we are safely parked, since it is blocked on a condition variable waiting for
 * us.
 */
bool Scheduler::sync_init(Worker* worker)
{
    if (!initializing()) [[likely]]
        return true; // proceed with scheduling.

    std::unique_lock<std::mutex> lock{m_workers_mtx};
    worker->notify(lock); // Notify thread (waiting in worker constructor) to continue
    worker->wait(lock);   // and go to sleep.

    if (exit_signaled()) [[unlikely]]
        return false;

    return m_workers_started ? false : (m_workers_started = true);
}

/**
 * Parks worker to proper queue based on synchronization context.
 */
template<Sync_context ctx>
void Scheduler::park_worker(Worker* worker)
{
    if constexpr (ctx == Sync_context::main)
        if (initializing()) [[unlikely]]
            park_idle<true>(worker);
        else [[likely]]
            park_idle<false>(worker);
    else if constexpr (ctx == Sync_context::yield)
        park_yielded(worker);
    else if constexpr (ctx == Sync_context::wait)
        park_waiting(worker);
    else if constexpr (ctx == Sync_context::io)
        park_pending_io(worker);
}

/**
 * Synchronization point for the workers.
 * We will park current worker to proper queue, schedule workers and context switch to
 * next worker if needed. For workers initialization, we will enter sync_init function.
 */
template<Sync_context ctx>
void Scheduler::sync(Worker* worker)
{
    park_worker<ctx>(worker);

    if (!sync_init(worker)) [[unlikely]]
        return;

    schedule();

    if (m_worker != worker)
        context_switch(worker);
}

/**
 * Sets exit state for scheduler and current worker.
 * Other workers will set their exit state in worker destructor.
 */
void Scheduler::exit()
{
    set_worker_state(m_worker, Worker::State::exiting);
    set_state(State::exiting);
}

bool Scheduler::has_work() const noexcept
{
    return has_runnable_workers() || has_waiting_workers() || has_pending_io_workers() ||
           has_tasks() || has_yielded_workers();
}

bool Scheduler::should_exit() const noexcept
{
    const bool exit = exit_signaled();
    assert(!exit || !has_work());

    return exit;
}

namespace sch {

/**
 * Returns global schedulers.
 */
std::unique_ptr<Schedulers>& get() noexcept
{
    return schedulers;
}

Scheduler& min_load_scheduler() noexcept
{
    return get()->min_load_scheduler();
}

u32 schedulers_count() noexcept
{
    return get()->cpus_count();
}

u32 cpus_count() noexcept
{
    return get()->cpus_count();
}

u32 workers_per_cpu_count() noexcept
{
    return get()->workers_per_cpu_count();
}

u32 workers_count() noexcept
{
    return get()->workers_count();
}

}; // namespace sch

template void Scheduler::sync<Sync_context::main>(Worker* worker);
template void Scheduler::sync<Sync_context::yield>(Worker* worker);
template void Scheduler::sync<Sync_context::wait>(Worker* worker);
template void Scheduler::sync<Sync_context::io>(Worker* worker);

} // namespace ums

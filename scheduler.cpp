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
#include "scheduler.h"

// #include <iostream>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <utility>

#include "config.h"
#include "intrusive_list.hpp"
#include "io_api.h"
#include "options.h"
#include "os_specific.h"
#include "task.h"
#include "util.h"
#include "worker.h"

namespace ums {

// Creates new Scheduler for each bit available in CPUs availability mask.
//
// clang-format off
Schedulers::Schedulers(Options opt) noexcept try
    : m_system_cpus_count{std::min(os::cpus_count(), CFG_max_supported_cpus)}
    , m_cpus_avail_mask{Cpu_Mask{os::cpus_avail_mask()} & Cpu_Mask{CFG_allowed_cpus_mask}}
{
    size_t cpu_id = 0;
    uint64_t sch_created = 0;

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
}

// clang-format on

// TODO: Prefer this scheduler if this scheduler has the same load as min scheduler, since we can
// than execute task instantly. Also, make invokation on the same thread that schedules task
// possible and check if it makes sense. Also, make scheduling policy: async and concurent and
// decide based on that.
Scheduler& Schedulers::min_load_scheduler() const noexcept
{
    const auto cmp = [](const auto& left, const auto& right) {
        return left->load() < right->load();
    };

    return **std::ranges::min_element(m_schedulers, cmp);
}

[[nodiscard]] uint32_t Schedulers::workers_count() const noexcept
{
    return m_schedulers.size() * m_schedulers.front()->workers_count();
}

[[nodiscard]] uint32_t Schedulers::cpus_count() const noexcept
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

    if (any_of(m_schedulers, [&](const auto& s) { return s->has_tasks(); }))
        return false;

    for_each(m_schedulers, [](auto& s) { s->m_mtx.lock(); });
    const bool r =
        all_of(m_schedulers, [&](const auto& s) { return s->idle() && !s->has_tasks(); });
    for_each(m_schedulers, [](auto& s) { s->m_mtx.unlock(); });

    return r;
}

/**
 * Increases the number of idle schedulers and notifies main thread (waiting in wait_exit)
 * if all shcedulers are idle.
 */
void Schedulers::signal_idle() noexcept
{
    const uint32_t idle_count = m_idle_schedulers.fetch_add(1, std::memory_order_relaxed) + 1;
    if (idle_count < m_schedulers.size())
        return;

    {
        std::scoped_lock<std::mutex> lock{m_mtx};
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
            Scoped_unlock<std::mutex> unlock{m_mtx};
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

// Creates scheduler and workers for provided CPU.
// After workers are created, starts single worker from idle queue
// and waits until worker (scheduler) goes to sleep.
// TODO: Speed this up with parallel workes creation.
Scheduler::Scheduler(Schedulers* schedulers, uint64_t cpu_id,
                     Options::Workers_per_scheduler workers_count)
    : m_schedulers{schedulers}
    , m_cpu{cpu_id}
{
    for (uint32_t i = 0; i < workers_count; ++i)
        m_workers.push_back(std::make_unique<Worker>(i, this));

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

// Parks worker to idle queue. If worker has completed task, notify user and release task.
//
template<bool back>
void Scheduler::park_idle(Worker* worker)
{
    if constexpr (back)
        m_idle_queue.push_back(*worker);
    else
        m_idle_queue.push_front(*worker);

    set_worker_state(worker, Worker::State::idle);

    if (worker->m_task) [[likely]] {
        worker->m_task->notify();
        worker->m_task.reset();
    }
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
    notify();
}

std::shared_ptr<TaskBase> Scheduler::next_task() noexcept
{
    return m_tasks.deque();
}

bool Scheduler::has_tasks() const noexcept
{
    return !m_tasks.empty();
}

// Schedules idle worker with provided task or with next task from tasks queue if task is empty.
// We must check whether task exists even if we are getting task from our queue, because someone
// might have stolen our task in the meantime.
//
void Scheduler::schedule_idle_worker(std::shared_ptr<TaskBase> task)
{
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

// Schedules yielded worker by moving it to the runnable queue.
//
void Scheduler::schedule_yielded_workers()
{
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

    if (has_runnable_workers() || !has_idle_workers())
        return;

    auto other_with_tasks = [&](auto& other) { return other->id() != id() && other->has_tasks(); };

    for (const auto& other : m_schedulers->filter(other_with_tasks)) {
        if (auto task{other->next_task()}) {
            schedule_idle_worker(std::move(task));
            return;
        }
    }
}

void Scheduler::schedule_workers()
{
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
    [[maybe_unused]] bool wait_res = m_cv.wait_until(lock, abs_time, [&] { return m_running; });

    assert(m_running || !wait_res); // Either someone notified us or we timed out on wait.
    m_running = true;
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
    notify(lock); // TODO: Unlock before notify to avoid pessimization!
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
        result.m_cond |= worker.check_cond();
        result.m_earliest_wait = std::min(result.m_earliest_wait, worker.sleep_time_point());
    }

    return result;
}

// Checks whether scheduler should idle spin.
// Idle spin is necessary for eficient use of scheduler. After task is done, if we go to sleep
// immediately, we will miss the oportinity to execute next user task which might come right after
// the previous task is done if user executes tasks sequentially.
//
bool Scheduler::should_idle_spin() noexcept
{
    if constexpr (!FS_idle_spinning_allowed)
        return false;

    if (initializing()) [[unlikely]]
        return false;

    if (m_idle_start_time == Time_point::max())
        m_idle_start_time = now();

    return now() <= m_idle_start_time + CFG_idle_spin_threshold;
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
        notify(lock); // Notify thread (waiting in scheduler constructor) to continue.

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
// clang-format off
class Scheduler_loads {
public:
    static constexpr int loads_size = int(Worker::State::exiting) + 1;

    constexpr inline Scheduler_loads()
    {
        m_loads[int(Worker::State::initializing)] =  0;
        m_loads[int(Worker::State::idle)]         =  0;
        m_loads[int(Worker::State::waiting)]      =  1;
        m_loads[int(Worker::State::pending_io)]   =  2;
        m_loads[int(Worker::State::yielded)]      = 10;
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

static constexpr Scheduler_loads Loads;

// Sets new scheduler load based on previous and new worker state.
//
void Scheduler::manage_load(Worker::State prev_state, Worker::State new_state) noexcept
{
    m_load.fetch_add(Loads[new_state] - Loads[prev_state], std::memory_order_relaxed);
}

uint64_t Scheduler::load() const noexcept
{
    const uint64_t tasks_load = m_tasks.size() * Loads[Worker::State::runnable];
    return m_load.load(std::memory_order_relaxed) + tasks_load;
}

// Switches thread execution context from previous worker to current.
//
// Notes:
// There is a single mutex on scheduler used for workers synchronization and every worker has
// it's own condition variable. In order to atomically suspend single worker thread (go to sleep
// by calling wait) and wake up next, we will take lock on mutex before notifying another thread
// to wake up. Condition_variable::wait function guarantees that it will unlock mutex and go to
// sleep atomically and it also guarantees that it will take lock on mutex when wait is done. So
// when we notify another thread to wake up we are already holding lock on mutex (and notified
// thread can not wake up until we release lock) so mutex will be unlocked only when we call
// wait on this thread, which will release lock and wake up another thread.
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
// waiting on condition variable. We will later decide whether to proceed with scheduling based
// on return value. Since only first started worker on scheduler should enter scheduling code
// (other workers will already be scheduled when it's their turn to run), we will use flag
// m_workers_started to help us do this. Also, we are going to notify scheduler thread to
// continue when we are safely parked, since it is blocked on a condition variable waiting for
// us.
//
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

// Parks worker to proper queue based on synchronization context.
//
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

// Synchronization point for the workers.
// We will park current worker to proper queue, schedule workers and context switch to
// next worker if needed. For workers initialization, we will enter sync_init function.
//
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

// Sets exit state for scheduler and current worker.
// Other workers will set their exit state in worker destructor.
//
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
    bool exit = exit_signaled();
    assert(!exit || !has_work());

    return exit;
}

template void Scheduler::sync<Sync_context::main>(Worker* worker);
template void Scheduler::sync<Sync_context::yield>(Worker* worker);
template void Scheduler::sync<Sync_context::wait>(Worker* worker);
template void Scheduler::sync<Sync_context::io>(Worker* worker);

} // namespace ums
#include "scheduler.h"

// #include <iostream>
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <mutex>

#include "config.h"
#include "cpu.h"
#include "io_api.h"
#include "sync_api.h"
#include "task_manager.h"
#include "worker.h"

// Creates scheduler and workers for provided CPU.
// After workers are created, starts single worker from idle queue.
//
Scheduler::Scheduler(const CPU& cpu)
    : m_cpu{cpu}
    , m_worker{nullptr}
    , m_state{State::initializing}
    , m_workers_started{false}
    , m_load{0}
{
    for (uint32_t i = 0; i < CFG_workers_per_cpu; ++i)
        m_workers.push_back(std::make_unique<Worker>(i, *this));

    std::unique_lock<std::mutex> lock{m_workers_mtx};

    set_state(State::running);
    m_worker = m_idle_queue.front();
    m_worker->notify(lock);
}

Scheduler::~Scheduler()
{
    set_state(State::exiting);
}

bool Scheduler::has_idle_workers() const
{
    return !m_idle_queue.empty();
}

bool Scheduler::has_runnable_workers() const
{
    return !m_runnable_queue.empty();
}

bool Scheduler::has_waiting_workers() const
{
    return !m_waiting_queue.empty();
}

bool Scheduler::has_pending_io_workers() const
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

void Scheduler::save_sleeping(Worker* worker)
{
    m_sleeping_queue.push_back(worker);
    worker->set_state(Worker::State::sleeping);
}

void Scheduler::save_pending_io(Worker* worker)
{
    m_pending_io_queue.push_back(worker);
    worker->set_state(Worker::State::pending_io);
}

void Scheduler::prepare_next_worker()
{
    m_worker = m_runnable_queue.front();
    m_runnable_queue.pop_front();

    m_worker->set_state(Worker::State::running);
}

void Scheduler::enqueue_task(const std::shared_ptr<Task>& task)
{
    const std::scoped_lock<std::mutex> lock{m_tasks_mtx};
    m_tasks.push_back(task);
}

std::shared_ptr<Task> Scheduler::next_task()
{
    const std::scoped_lock<std::mutex> lock{m_tasks_mtx};
    std::shared_ptr<Task> t = m_tasks.front();
    m_tasks.pop_front();

    return t;
}

bool Scheduler::has_tasks()
{
    const std::scoped_lock<std::mutex> lock{m_tasks_mtx};
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
        return worker->m_io_request->completed();
    };

    auto begin = m_pending_io_queue.begin();
    auto end = m_pending_io_queue.end();

    for (auto it = std::find_if(begin, end, io_completed); it != end;
         it = std::find_if(it, end, io_completed)) {
        save_runnable(*it);
        it = m_pending_io_queue.erase(it);
    }
}

void Scheduler::schedule_idle_workers()
{
    while (has_tasks() && has_idle_workers())
        schedule_idle_worker();
}

// Moves workers from waiting to runnable queue if worker's event is signaled.
//
void Scheduler::schedule_waiting_workers()
{
    auto signaled = [](Worker* worker) { return worker->m_cond_event->check(); };

    auto begin = m_waiting_queue.begin();
    auto end = m_waiting_queue.end();

    for (auto it = std::find_if(begin, end, signaled); it != end;
         it = std::find_if(it, end, signaled)) {
        save_runnable(*it);
        it = m_waiting_queue.erase(it);
    }
}

void Scheduler::schedule_sleeping_workers()
{
    auto signaled = [](Worker* worker) { return worker->m_timed_event->check(); };

    auto begin = m_sleeping_queue.begin();
    auto end = m_sleeping_queue.end();

    for (auto it = std::find_if(begin, end, signaled); it != end;
         it = std::find_if(it, end, signaled)) {
        save_runnable(*it);
        it = m_sleeping_queue.erase(it);
    }
}

void Scheduler::schedule_workers()
{
    schedule_io_workers();
    schedule_waiting_workers();
    schedule_sleeping_workers();
    schedule_idle_workers();
}

void Scheduler::schedule()
{
    schedule_workers();

    // Idle loop if there is no work.
    //
    while (!has_runnable_workers() && !should_exit()) {
        idle_sleep();
        schedule_workers();
    }

    if (should_exit()) [[unlikely]]
        exit_workers();
    else [[likely]]
        prepare_next_worker();
}

void Scheduler::idle_sleep()
{
    // std::cout << "Idle sleep.\n";

    // TODO: Check what should be done here.
    // TODO: Release CPU (sleep) only when our time slice expires.
    // TODO: Check why there is a problem with sync tasks execution
    // if there is a sleep after task is done.
    //

    // tatic int c = 0;
    // onstexpr int sleep_cycle = 1 << 20;
    //
    // f ((++c & (sleep_cycle-1)) == 0)
    //
    // std::cout << "Zzzz...\n";
    // std::this_thread::sleep_for(CFG_idle_sleep);
    // c = 0;
    //

    // auto start = std::chrono::high_resolution_clock::now();
    //
    // std::this_thread::sleep_for(CFG_idle_sleep);
    //
    // auto end = std::chrono::high_resolution_clock::now();
    //
    // std::chrono::duration<double, std::milli> duration = end - start;
    // std::cout << "Sleep duration: " << duration.count() << "ms.\n";

    // auto start = std::chrono::high_resolution_clock::now();
    //
    // std::unique_lock lock{ m_workers_mtx };
    // m_worker->wait(lock);
    //
    // auto end = std::chrono::high_resolution_clock::now();
    //
    // std::chrono::duration<double, std::milli> duration = end - start;
    // std::cout << "Sleep duration: " << duration.count() << "ms.\n";
}

void Scheduler::set_state(State state)
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
        m_loads[int(Worker::State::sleeping)]     =  1;
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
void Scheduler::manage_load(Worker::State prev_state, Worker::State new_state)
{
    m_load += Loads[new_state] - Loads[prev_state];
}

uint64_t Scheduler::load() const
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
    worker->notify(lock); // Notify scheduler thread that created us to continue
    worker->wait(lock);   // and go to sleep.

    return m_workers_started ? false : (m_workers_started = true);
}

// Save worker to proper queue based on synchronization context.
//
template<SyncCtx ctx>
void Scheduler::save_worker(Worker* worker)
{
    if constexpr (ctx == SyncCtx::main)
        if (initializing()) [[unlikely]]
            save_idle<true>(worker);
        else [[likely]]
            save_idle<false>(worker);
    else if constexpr (ctx == SyncCtx::yield)
        save_runnable(worker);
    else if constexpr (ctx == SyncCtx::wait_event)
        save_waiting(worker);
    else if constexpr (ctx == SyncCtx::wait_sleep)
        save_sleeping(worker);
    else if constexpr (ctx == SyncCtx::io)
        save_pending_io(worker);
}

// Synchronization point for the workers.
// We will save current worker to proper queue, schedule workers and context switch to
// next worker if needed. For workers initialization, we will enter sync_init function.
//
template<SyncCtx ctx>
void Scheduler::sync(Worker* worker)
{
    save_worker<ctx>(worker);

    if (!sync_init(worker)) [[unlikely]]
        return;

    schedule();

    if (m_worker != worker)
        context_switch(worker);
}

void Scheduler::exit_workers()
{
    std::unique_lock<std::mutex> lock{m_workers_mtx};
    for (Worker* worker : m_idle_queue) {
        worker->set_state(Worker::State::exiting);
        worker->notify(lock);
    }
}

bool Scheduler::should_exit()
{
    return exiting() && !has_runnable_workers() && !has_waiting_workers() &&
           !has_pending_io_workers() && !has_tasks();
}

template void Scheduler::sync<SyncCtx::main>(Worker* worker);
template void Scheduler::sync<SyncCtx::yield>(Worker* worker);
template void Scheduler::sync<SyncCtx::wait_event>(Worker* worker);
template void Scheduler::sync<SyncCtx::io>(Worker* worker);
template void Scheduler::sync<SyncCtx::wait_sleep>(Worker* worker);

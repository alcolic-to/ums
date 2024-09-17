#pragma once

#ifndef COS_SCHEDULER_H
#define COS_SCHEDULER_H

#include <atomic>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "task_manager.h"
#include "worker.h" // This can be moved and class Worker can be forward declared if we dispose Worker::State.

class CPUs;
class CPU;

// Synchronization context for scheduler.
//
enum class SyncCtx : int { main, yield, wait_event, io, wait_sleep };

class Scheduler final {
public:
    enum class State : int { initializing, running, exiting };

    explicit Scheduler(const CPU& cpu);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&&) noexcept = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    bool has_idle_workers() const;
    bool has_runnable_workers() const;
    bool has_waiting_workers() const;
    bool has_pending_io_workers() const;

    void save_runnable(Worker* worker);
    void save_waiting(Worker* worker);
    void save_sleeping(Worker* worker);
    void save_pending_io(Worker* worker);

    template<bool back>
    void save_idle(Worker* worker);

    void prepare_next_worker();

    void enqueue_task(const std::shared_ptr<Task>& task);
    std::shared_ptr<Task> next_task();
    bool has_tasks();

    void schedule_idle_worker();

    void schedule_io_workers();
    void schedule_idle_workers();
    void schedule_waiting_workers();
    void schedule_sleeping_workers();
    void schedule_workers();

    void schedule();

    void idle_sleep();

    [[nodiscard]] bool exiting() const { return m_state == State::exiting; }

    [[nodiscard]] bool initializing() const { return m_state == State::initializing; }

    [[nodiscard]] Worker* worker() const { return m_worker; }

    void set_state(State state);

    void manage_load(Worker::State prev_state, Worker::State new_state);
    void inc_load();
    void dec_load();
    uint64_t load() const;

    void context_switch(Worker* prev_worker);

    template<SyncCtx ctx>
    void save_worker(Worker* worker);

    bool sync_init(Worker* worker);

    template<SyncCtx ctx>
    void sync(Worker* worker);

    void exit_workers();
    bool should_exit();

    const CPU& m_cpu;

    Worker* m_worker;
    std::deque<Worker*> m_runnable_queue;
    std::deque<Worker*> m_idle_queue;
    std::list<Worker*> m_waiting_queue;
    std::list<Worker*> m_pending_io_queue;
    std::list<Worker*> m_sleeping_queue;

    std::mutex m_workers_mtx;
    std::mutex m_tasks_mtx;
    std::deque<std::shared_ptr<Task>> m_tasks;
    std::atomic<State> m_state;
    bool m_workers_started;
    uint64_t m_load;

    // TODO: Create workers in place next to each other.
    //
    std::vector<std::unique_ptr<Worker>> m_workers;
};

#endif // COS_SCHEDULER_H

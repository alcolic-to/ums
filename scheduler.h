#pragma once

#ifndef COS_SCHEDULER_H
#define COS_SCHEDULER_H

#include <memory>
#include <vector>
#include <deque>
#include <list>
#include <mutex>
#include <atomic>

#include "worker.h" // This can be moved and class Worker can be forward declared if we dispose Worker::State.
#include "task_manager.h"

class CPUs;
class CPU;

// Synchronization context for scheduler.
//
enum class SyncCtx : int { main, yield, wait_event, io, wait_sleep };

class Scheduler final
{
public:
    enum class State : int { initializing, running, exiting };

    Scheduler(const CPU& cpu);
    ~Scheduler();

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

    void enqueue_task(std::shared_ptr<Task> task);
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

    bool exiting() const;
    bool initializing() const;
    Worker* worker() const;

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

public:
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

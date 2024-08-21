#pragma once

#ifndef COS_WORKER_H
#define COS_WORKER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "task_manager.h"
#include "os_specific.h"

class Scheduler;
class TimedEvent;
class ConditionalEvent;

// TODO: Check whether worker should be placed on std::hardware_constructive_interference_size alignment,
// to avoid false sharing.
//
class Worker final
{
public:
    enum class State : int { initializing, idle, waiting, pending_io, runnable, sleeping , running, exiting };

    // Create worker object and start worker thread on a provided CPU.
    //
    Worker(uint64_t id, Scheduler& scheduler);

    ~Worker();

    Worker(const Worker& other) = delete;
    Worker(const Worker&& other) = delete;

    Worker& operator=(const Worker& other) = delete;

    void entry_point();

    void main_loop();

    void yield();
    void read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);
    void write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);
    void wait_event(ConditionalEvent* event);
    void wait_sleep(TimedEvent* event);

    void set_state(State state);

    void notify(std::unique_lock<std::mutex>& lock);
    void wait(std::unique_lock<std::mutex>& lock);

    constexpr uint64_t id() const { return m_id; }
    constexpr State state() const { return m_state; }

    bool exit() const;

public:

    // TODO: reorganize data members for quick access.
    //
    std::condition_variable m_cv;
    uint64_t m_id;
    State m_state;
    bool m_running; // Flag used for spurious wakeup check.
    ConditionalEvent* m_cond_event;
    TimedEvent* m_timed_event;
    std::shared_ptr<Task> m_task;
    std::unique_ptr<IO_Request> m_io_request;
    Scheduler& m_scheduler;
    std::thread m_thread;
};

extern thread_local Worker* tls_worker;

#endif // COS_WORKER_H

#include "worker.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>

#include "cpu.h"
#include "io_api.h"
#include "os_specific.h"
#include "scheduler.h"
#include "sync_api.h"

// Creates worker object and starts worker thread on a provided CPU.
// We will wait for a signal from created thread, so we can continue when it is ready.
//
Worker::Worker(uint64_t id, Scheduler& scheduler)
    : m_id{id}
    , m_state{State::initializing}
    , m_running{true}
    , m_cond_event{nullptr}
    , m_timed_event{nullptr}
    , m_scheduler{scheduler}
    , m_thread{&Worker::entry_point, this}
{
    std::unique_lock<std::mutex> lock{m_scheduler.m_workers_mtx};
    m_cv.wait(lock, [&] { return !m_running; });
}

Worker::~Worker()
{
    if (m_thread.joinable())
        m_thread.join();
}

void Worker::set_state(Worker::State state)
{
    m_scheduler.manage_load(m_state, state);
    m_state = state;
}

bool Worker::exit() const
{
    return m_state == State::exiting;
}

// Yields current worker and wakes up next worker for execution.
//
void Worker::yield()
{
    m_scheduler.sync<SyncCtx::yield>(this);
}

// Wait on a event if it is not signaled.
//
void Worker::wait_event(ConditionalEvent* event)
{
    if (!event->check()) {
        m_cond_event = event;
        m_scheduler.sync<SyncCtx::wait_event>(this);
    }
}

void Worker::wait_sleep(TimedEvent* event)
{
    m_timed_event = event;
    m_scheduler.sync<SyncCtx::wait_sleep>(this);
}

void Worker::read_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    m_io_request =
        std::make_unique<IO_Request>(file_handle, buffer, offset, IO_Request::Type::read);

    if (m_io_request->m_state == IO_Request::State::pending)
        m_scheduler.sync<SyncCtx::io>(this);
}

void Worker::write_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    m_io_request =
        std::make_unique<IO_Request>(file_handle, buffer, offset, IO_Request::Type::write);

    if (m_io_request->m_state == IO_Request::State::pending)
        m_scheduler.sync<SyncCtx::io>(this);
}

void Worker::notify([[maybe_unused]] std::unique_lock<std::mutex>& lock)
{
    m_running = true;
    m_cv.notify_one();
}

void Worker::wait(std::unique_lock<std::mutex>& lock)
{
    m_running = false;
    m_cv.wait(lock, [&] { return m_running; });
}

void Worker::entry_point()
{
    bind_thread(m_scheduler.m_cpu.m_mask);

    std::cout << "Started thread: " << id() << " on CPU " << m_scheduler.m_cpu.m_id << "\n";

    main_loop();

    std::cout << "Ended thread: " << id() << " on CPU " << m_scheduler.m_cpu.m_id << "\n";
}

// Main worker loop.
//
void Worker::main_loop()
{
    tls_worker = this;

    while (true) {
        m_scheduler.sync<SyncCtx::main>(this);

        if (exit())
            return;

        try {
            m_task->m_func();
        }
        catch (const std::exception& ex) {
            std::cout << ex.what() << "\n";
        }

        std::cout << "CPU " << m_scheduler.m_cpu.m_id << ": worker id " << id() << " task done.\n";

        m_task->notify();
        m_task.reset();
    }
}

// Thread local worker pointer.
//
thread_local Worker* tls_worker;

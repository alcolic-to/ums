#include "worker.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>

#include "io_api.h"
#include "os_specific.h"
#include "scheduler.h"
#include "util.h"

// Creates worker object and starts worker thread on a provided CPU.
// We will wait for a signal from created thread, so we can continue when it is ready.
//
Worker::Worker(uint64_t id, Scheduler& scheduler)
    : m_id{id}
    , m_scheduler{scheduler}
    , m_thread{&Worker::entry_point, this}
{
    std::unique_lock<std::mutex> lock{m_scheduler.m_workers_mtx};
    m_cv.wait(lock, [&] { return !m_running; });
}

Worker::~Worker()
{
    {
        const std::unique_lock<std::mutex> lock{m_scheduler.m_workers_mtx};
        set_state(Worker::State::exiting);
        notify(lock);
    }

    if (m_thread.joinable())
        m_thread.join();
}

void Worker::set_state(Worker::State state) noexcept
{
    m_scheduler.manage_load(m_state, state);
    m_state = state;
}

bool Worker::exit() const noexcept
{
    return m_state == State::exiting;
}

// Yields current worker and wakes up next worker for execution.
//
void Worker::yield()
{
    m_scheduler.sync<Sync_context::yield>(this);
}

void Worker::wait_cond_or_sleep()
{
    if (!check_wait_info())
        m_scheduler.sync<Sync_context::wait_cond_or_sleep>(this);
}

void Worker::sleep_until_internal(const Time_point& time_point)
{
    set_wait_info(false, time_point);
    wait_cond_or_sleep();
}

void Worker::read_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    m_io_request =
        std::make_unique<IO_Request>(file_handle, buffer, offset, IO_Request::Type::read);

    if (m_io_request->m_state == IO_Request::State::pending)
        m_scheduler.sync<Sync_context::io>(this);
}

void Worker::write_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    m_io_request =
        std::make_unique<IO_Request>(file_handle, buffer, offset, IO_Request::Type::write);

    if (m_io_request->m_state == IO_Request::State::pending)
        m_scheduler.sync<Sync_context::io>(this);
}

void Worker::wait(std::unique_lock<std::mutex>& lock)
{
    m_running = false;
    m_cv.wait(lock, [&] { return m_running; });
}

void Worker::notify([[maybe_unused]] const std::unique_lock<std::mutex>& lock) noexcept
{
    m_running = true;
    m_cv.notify_one();
}

// We must notify scheduler that our condition is set, because it might be sleeping.
//
void Worker::notify_waiter() noexcept
{
    set_cond();
    m_scheduler.notify();
}

void Worker::entry_point()
{
    tls_worker = this;
    os::bind_thread(m_scheduler.m_cpu.m_mask.to_ullong());

    // std::cout << "Started thread: " << id() << " on CPU " << m_scheduler.m_cpu.m_id << "\n";

    main_loop();

    // std::cout << "Ended thread: " << id() << " on CPU " << m_scheduler.m_cpu.m_id << "\n";
}

// Main worker loop.
//
void Worker::main_loop()
{
    while (true) {
        m_scheduler.sync<Sync_context::main>(this);

        if (exit())
            return;

        try {
            m_task->m_func();
        }
        catch (const std::exception& ex) {
            std::cout << "Unhandled user exception: " << ex.what() << "\n";
        }

        // std::cout << "CPU " << m_scheduler.m_cpu.m_id << ": worker id " << id() << " task done.\n
        // ";

        m_task->notify();
        m_task.reset();
    }
}

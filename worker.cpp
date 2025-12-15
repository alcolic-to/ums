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
#include "worker.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <mutex>

#include "config.hpp"
#include "io.hpp"
#include "os_specific.hpp"
#include "scheduler.hpp"
#include "types.hpp"
#include "util.hpp"

namespace ums {

namespace {

/* Thread local worker */
thread_local Worker* this_worker; // NOLINT

} // namespace

/**
 * Creates worker object and starts worker thread on a provided CPU.
 * We will wait for a signal from created thread, so we can continue when it is ready.
 */
Worker::Worker(u64 id, Scheduler* scheduler)
    : m_id{id}
    , m_scheduler{scheduler}
    , m_thread{&Worker::entry_point, this}
{
    std::unique_lock<std::mutex> lock{m_scheduler->m_workers_mtx};
    m_cv.wait(lock, [&] { return !m_running; });
}

Worker::~Worker()
{
    {
        const std::unique_lock<std::mutex> lock{m_scheduler->m_workers_mtx};
        set_state(Worker::State::exiting);
        notify(lock);
    }

    if (m_thread.joinable())
        m_thread.join();
}

void Worker::set_state(State state) noexcept
{
    m_state = state;
}

bool Worker::exit() const noexcept
{
    return m_state == State::exiting;
}

/**
 * Yields current worker and wakes up next worker for execution.
 */
void Worker::yield()
{
    TZoneScopedC(tracy::Color::Gray);
    m_scheduler->sync<Sync_context::yield>(this);
}

/**
 * Waits for event or timer expiration.
 */
void Worker::wait_cond_or_sleep()
{
    TZoneScopedC(tracy::Color::Gray);
    if (!check_wait_info())
        m_scheduler->sync<Sync_context::wait>(this);
}

void Worker::sleep_until_internal(const Time_point& time_point)
{
    set_wait_info(false, time_point);
    wait_cond_or_sleep();
}

void Worker::read_file(void* file_handle, IO_Buffer buffer, u64 offset)
{
    m_io_request =
        std::make_unique<IO_Request>(file_handle, buffer, offset, IO_Request::Type::read);

    if (m_io_request->m_state == IO_Request::State::pending)
        m_scheduler->sync<Sync_context::io>(this);
}

void Worker::write_file(void* file_handle, IO_Buffer buffer, u64 offset)
{
    m_io_request =
        std::make_unique<IO_Request>(file_handle, buffer, offset, IO_Request::Type::write);

    if (m_io_request->m_state == IO_Request::State::pending)
        m_scheduler->sync<Sync_context::io>(this);
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

/**
 * We must notify scheduler that our condition is set, because it might be sleeping.
 */
void Worker::notify_waiter() noexcept
{
    set_cond();
    m_scheduler->notify();
}

void Worker::entry_point()
{
    set_tracy_worker(id(), m_scheduler->id());

    this_worker = this;

    if constexpr (FS_thread_binding_allowed)
        os::bind_thread(m_scheduler->m_cpu.m_mask.to_ullong());

    // std::cout << "Started thread: " << id() << " on CPU " << m_scheduler.m_cpu.m_id << "\n";

    main_loop();

    // std::cout << "Ended thread: " << id() << " on CPU " << m_scheduler.m_cpu.m_id << "\n";
}

/**
 * Main worker loop.
 */
void Worker::main_loop()
{
    while (true) {
        m_scheduler->sync<Sync_context::main>(this);

        if (exit()) [[unlikely]]
            return;

        TZoneScopedC(tracy::Color::Green1);
        TTracyMessageLC(tracy_msg("Task started."), tracy::Color::Green1);

        m_task->invoke_noexcept();

        TTracyMessageLC(tracy_msg("Task done."), tracy::Color::Green1);

        m_task->notify();
        m_task.reset();

        // std::cout << "CPU " << m_scheduler.m_cpu.m_id << ": worker id " << id() << " task done.\n
        // ";
    }
}

/**
 * API similar to std::thread::
 */
namespace worker {

/**
 * Returns current (this) worker.
 */
Worker* get() noexcept
{
    return this_worker;
}

/**
 * Returns worker id.l
 */
u64 get_id() noexcept
{
    return this_worker->id();
}

/**
 * Yield this worker.
 */
void yield() noexcept
{
    this_worker->yield();
}

}; // namespace worker

} // namespace ums

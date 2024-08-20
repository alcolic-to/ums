#include <iostream>
#include <cassert>
#include <liburing.h>
#include <locale>
#include <string.h>

#include "worker.h"
#include "cpu.h"
#include "os_specific.h"
#include "scheduler.h"
#include "sync_api.h"
#include "io_api.h"

// Creates worker object and starts worker thread on a provided CPU.
// We will wait for a signal from created thread, so we can continue when it is ready.
//
Worker::Worker(uint64_t id, Scheduler& scheduler)
	: m_id{ id }
	, m_state{ State::initializing }
    , m_io_req_id { 0 }
    , m_io_completed { false }
    , m_io_bytes { 0 }
	, m_cond_event{ nullptr }
	, m_timed_event{ nullptr }
	, m_scheduler{ scheduler }
	, m_thread{ &Worker::entry_point, this }

{
    int ret = io_uring_queue_init(1, &m_uring, 0 /* flags */);
    if (ret < 0)
    {
        char buff[100];
        sprintf(buff, "io_uring_queue_init failed with %s\n", strerror(-ret));
        assert(false && buff);
    }
	std::unique_lock<std::mutex> lock{ m_scheduler.m_workers_mtx };
	m_cv.wait(lock);
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
	if (!event->check())
	{
		m_cond_event = event;
		m_scheduler.sync<SyncCtx::wait_event>(this);
	}
}

void Worker::wait_sleep(TimedEvent* event)
{
    m_timed_event = event;
    m_scheduler.sync<SyncCtx::wait_sleep>(this);
}

void Worker::update_io()
{
    io_uring_cqe* cqe;
    int ret = io_uring_peek_cqe(&m_uring, &cqe);
    if (ret == 0 && cqe != nullptr)
    {
        std::uint64_t id = io_uring_cqe_get_data64(cqe);
        assert(id == m_io_req_id && "Req ids don't match");
        assert(cqe->res == m_io_bytes && "Bytes written don't match");
        io_uring_cqe_seen(&m_uring, cqe);
        m_io_completed = true;
    }
}

bool Worker::get_io_status() const
{
    return m_io_completed;
}

void Worker::read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset)
{
	m_io_request = std::make_unique<IO_Request>(file_handle, buffer, nbytes, offset, IO_Request::Type::read);

	if (m_io_request->m_state == IO_Request::State::pending)
		m_scheduler.sync<SyncCtx::io>(this);
}

void Worker::write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset)
{
    m_io_req_id++;
    m_io_completed = false;
	// m_io_request = std::make_unique<IO_Request>(file_handle, buffer, nbytes, offset, IO_Request::Type::write);
    int fd = *reinterpret_cast<int*>(file_handle);
    io_uring_sqe* sqe = io_uring_get_sqe(&m_uring);
    assert(sqe && "io_uring_get_sqe is null");
    iovec io;
    io.iov_base = buffer;
    io.iov_len = nbytes;
    io_uring_prep_writev(sqe, fd, &io, 1, offset);
    io_uring_sqe_set_data64(sqe, m_io_req_id);
    m_io_bytes = nbytes;
    int ret = io_uring_submit(&m_uring);
    if (ret != 1)
    {
        assert(!"Failed to submit io");
    }

    m_scheduler.sync<SyncCtx::io>(this);
}

void Worker::notify()
{
	m_cv.notify_one();
}

void Worker::wait(std::unique_lock<std::mutex>& lock)
{
	m_cv.wait(lock);
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

	while (true)
	{
		m_scheduler.sync<SyncCtx::main>(this);

		if (exit())
			return;

		try
		{
			m_task->m_func();
		}
		catch (const std::exception& ex)
		{
			std::cout << ex.what() << "\n";
		}

		// std::cout << "CPU " << m_scheduler.m_cpu.m_id << ": worker id " << id() << " task done.\n";

		m_task->notify();
		m_task.reset();
	}
}

// Thread local worker pointer.
//
thread_local Worker* tls_worker;

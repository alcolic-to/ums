#include <iostream>

#include "worker.h"
#include "cpu.h"
#include "os_specific.h"
#include "scheduler.h"

Event::Event() : m_cond{ false } {};
void Event::signal() { m_cond = true; }
void Event::wait() { tls_worker->wait_event(*this); }

// Creates worker object and starts worker thread on a provided CPU.
// We will wait for a signal from created thread, so we can continue when it is ready.
//
Worker::Worker(uint64_t id, Scheduler& scheduler)
	: m_id{ id }
	, m_state{ State::initializing }
	, m_cv{}
	, m_task{}
	, m_event{ nullptr }
	, m_scheduler{ scheduler }
	, m_thread{ &Worker::entry_point, this }
{
	std::unique_lock<std::mutex> lock{ m_scheduler.m_workers_mtx };
	m_cv.wait(lock);
}

Worker::~Worker()
{
	m_scheduler.set_state(Scheduler::State::exiting);

	if (m_thread.joinable())
		m_thread.join();
}

void Worker::set_state(Worker::State state)
{
	m_scheduler.manage_load(m_state, state);
	m_state = state;
}

bool Worker::state_idle(State state) { return state == State::idle || state == State::waiting; }
bool Worker::state_runnable(State state) { return state == State::runnable || state == State::running; }

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
void Worker::wait_event(Event& event)
{
	if (!event.m_cond)
	{
		m_event = &event;
		m_scheduler.sync<SyncCtx::wait_event>(this);
	}
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

		// std::cout << "CPU " << m_cpu.m_id << ": worker id " << id() << " task done.\n";

		m_task->notify();
		m_task.reset();
	}
}

// Thread local worker pointer.
//
thread_local Worker* tls_worker;

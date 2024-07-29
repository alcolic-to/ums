#pragma once

#ifndef COS_WORKER_H
#define COS_WORKER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "task_manager.h"

class Scheduler;

class Event final
{
public:
	Event();
	void signal();
	void wait();

public:
	bool m_cond;
};

// TODO: Check whether worker should be placed on std::hardware_constructive_interference_size alignment,
// to avoid false sharing.
//
class Worker final
{
public:
	enum class State : int { initializing, idle, waiting, runnable, running, exiting };

	static bool state_idle(State state);
	static bool state_runnable(State state);

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
	void wait_event(Event& event);

	void set_state(State state);

	void notify();
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

	Event* m_event;
	std::shared_ptr<Task> m_task;
	Scheduler& m_scheduler;
	std::thread m_thread;
};

extern thread_local Worker* tls_worker;

#endif // COS_WORKER_H

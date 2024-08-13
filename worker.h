#pragma once

#ifndef COS_WORKER_H
#define COS_WORKER_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

#include "task_manager.h"
#include "os_specific.h"

class Scheduler;

class Event final
{
public:
	Event();
	void signal();
	void wait();

public:
	std::atomic<bool> m_cond;
};

// TODO: Check whether worker should be placed on std::hardware_constructive_interference_size alignment,
// to avoid false sharing.
//
class Worker final
{
public:
	enum class State : int { initializing, idle, waiting, pending_io, runnable, running, exiting };

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
	void read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);
	void write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);

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
	std::unique_ptr<IO_Request> m_io_request;
	Scheduler& m_scheduler;
	std::thread m_thread;
};

extern thread_local Worker* tls_worker;

#endif // COS_WORKER_H

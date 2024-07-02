#pragma once

#ifndef COS_H
#define COS_H

#include <cstdint>
#include <vector>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <list>
#include <functional>

class Scheduler;
class CPU;
class Worker;

class Task
{
public:

	Task() = default;
	Task(const std::function<void()> function)
		: m_function{ function } {}

	void operator()() { m_function(); }

	std::function<void()> m_function;
};

class CPUs final
{
public:
	CPUs();

	CPU& min_load_cpu() const;

private:
	uint32_t m_system_cpus_count;
	uint64_t m_avail_cpus_mask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

class Event final
{
public:
	void signal() { m_cond = true; }
	void wait();
	bool m_cond = false;
};

// Synchronization context for scheduler.
//
enum class SyncCtx : int { main, yield, wait_event };

class Scheduler final
{
public:
	enum class State : int { initializing, running, exiting };

	Scheduler(const CPU& cpu);

	void start();

	void enqueue_task(const Task& task);

	bool has_idle_workers() const;
	bool has_runnable_workers() const;
	bool has_waiting_workers() const;

	void save_runnable(Worker* worker);
	void save_idle(Worker* worker);
	void save_waiting(Worker* worker);

	void prepare_next_worker();

	bool has_tasks() const;
	Task next_task();

	void schedule_idle_worker();

	void schedule_idle_workers();
	void schedule_waiting_workers();

	void schedule_workers();

	bool exiting() const;
	bool initializing() const;
	Worker* worker() const;

	void set_state(State state);

	void context_switch(Worker* prevWorker);

	bool sync_main(Worker* worker);
	bool sync_yield(Worker* worker);
	bool sync_wait_event(Worker* worker);

	template<SyncCtx ctx>
	void sync(Worker* worker);

	void exit_workers() const;
	bool exit() const;

public:
	const CPU& m_cpu;
	std::mutex m_workers_mtx;
	std::deque<Worker*> m_runnable_queue;
	std::deque<Worker*> m_idle_queue;
	std::list<Worker*> m_waiting_queue;
	Worker* m_worker;
	std::mutex m_tasks_mtx;
	std::deque<Task> m_tasks;
	State m_state;
	bool m_workers_started;
};

class CPU final
{
public:
	CPU(uint64_t cpu_id, uint64_t cpu_mask);

public:

	void worker_entry_point(Worker& worker) const;
	void bind_thread() const;
	void execute_task(const Task& task);
	void inc_load();
	void dec_load();
	uint64_t load() const;

public:
	uint64_t m_id;
	uint64_t m_mask;
	uint64_t m_load;

	Scheduler m_scheduler;

	// TODO: Create workers in place next to each other.
	//
	std::vector<std::unique_ptr<Worker>> m_workers;
};

// TODO: Check whether worker should be placed on std::hardware_constructive_interference_size alignment,
// to avoid false sharing.
//
class Worker final
{
public:
	enum class State : int { initializing, idle, waiting, runnable, running, exiting };

	static bool state_idle(State state) { return state == State::idle || state == State::waiting; }
	static bool state_runnable(State state) { return state == State::runnable || state == State::running; }

	// Create worker object and start worker thread on a provided CPU.
	//
	Worker(uint64_t id, CPU& cpu);

	~Worker();

	Worker(const Worker& other) = delete;
	Worker(const Worker&& other) = delete;

	Worker& operator=(const Worker& other) = delete;

	void main_loop();

	void yield();
	void wait_event(Event& event);

	void set_state(State state);

	void notify();
	void wait(std::unique_lock<std::mutex>& lock);

	constexpr uint64_t id() const { return m_id; }
	constexpr CPU& cpu() const { return m_cpu; }
	constexpr State state() const { return m_state; }

	bool exit() const;

public:

	// TODO: reorganize data members for quick access.
	//
	std::condition_variable m_cv;
	std::thread m_thread;

	uint64_t m_id;
	State m_state;

	Event* m_event;
	Task m_task;
	CPU& m_cpu;
	Scheduler& m_scheduler;
};

class Task_manager final
{
public:
	Task_manager(const CPUs& cpus)
		: m_cpus{ cpus }
	{
	}

	void execute_task(Task task);

	const CPUs& m_cpus;
};

extern CPUs cpus;
extern Task_manager task_manager;
extern thread_local Worker* tls_worker;

#endif

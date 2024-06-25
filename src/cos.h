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

	CPU& MinLoadCPU() const;

private:
	uint32_t m_system_cpus_count;
	uint64_t m_avail_cpus_mask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

// Synchronization context for scheduler.
//
enum SyncCtx : int { MAIN, YIELD };

class Scheduler final
{
public:
	enum State : int { INITIALIZING, RUNNING, EXITING };

	Scheduler(const CPU& cpu);

	void start();

	void enqueue_task(const Task& task);

	bool has_idle_workers() const;
	bool has_runnable_workers() const;

	void save_runnable();
	void save_idle();

	void prepare_worker();
	void idle_looping();

	bool has_tasks() const;
	Task next_task();

	void schedule();
	void schedule_idle_worker();

	void exit_workers() const;
	bool exiting() const;
	bool initializing() const;
	Worker* worker() const;

	void set_state(State state);

	bool sync_main(Worker& worker);
	bool sync_yield(Worker& worker);

	template<SyncCtx ctx>
	void sync(Worker& worker);

public:
	const CPU& m_cpu;
	std::mutex m_workers_mtx;
	std::deque<Worker*> m_runnable_queue;
	std::deque<Worker*> m_idle_queue;
	Worker* m_worker;
	std::mutex m_tasks_mtx;
	std::deque<Task> m_tasks;
	State m_state;
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
	enum State : int { INITIALIZING, IDLE, IDLE_LOOPING, RUNNABLE, RUNNING, EXITING };

	static bool StateIdle(State state) { return state == IDLE || state == IDLE_LOOPING; }
	static bool StateRunnable(State state) { return state == RUNNABLE || state == RUNNING; }

	// Create worker object and start worker thread on a provided CPU.
	//
	Worker(uint64_t id, CPU& cpu);

	~Worker();

	Worker(const Worker& other) = delete;
	Worker(const Worker&& other) = delete;

	Worker& operator=(const Worker& other) = delete;

	void main_loop();

	void yield();

	bool idle_loop() const;
	void set_state(State state);
	bool exiting() const;

	constexpr uint64_t id() const { return m_id; }
	constexpr CPU& cpu() const { return m_cpu; }
	constexpr State state() const { return m_state; }

public:

	// TODO: reorganize data members for quick access.
	//
	std::condition_variable m_cv;
	std::thread m_thread;

	uint64_t m_id;
	State m_state;

	Task m_task;
	CPU& m_cpu;
	Scheduler& m_scheduler;
};

class TaskManager final
{
public:
	TaskManager(const CPUs& cpus)
		: m_cpus{ cpus }
	{
	}

	void ExecuteTask(Task task);

	const CPUs& m_cpus;
};

extern CPUs cpus;
extern TaskManager task_manager;
extern thread_local Worker* tls_worker;

#endif

#pragma once

#ifndef COS_H
#define COS_H

//class B;
//
//class A
//{
//public:
//	A(B& b)
//		: m_bref{ b }
//	{
//	
//	}
//
//	B& m_bref;
//};
//
//class B
//{
//public:
//	B() = default;
//	int a;
//};

#include <cstdint>
#include <vector>
#include <windows.h>
#include <bitset>
#include <iostream>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <list>
#include <queue>
#include <utility>
#include <functional>

class Scheduler;
class CPU;
class Worker;
class Task;

extern thread_local Worker* tls_worker;
extern std::deque<Task> g_tasksQueue;

class CPUs final
{
public:
	CPUs();

	// Start workers on every available CPU.
	//
	void Init();

	void Execute();

private:
	uint32_t m_systemCpusCount;
	uint64_t m_availCpusMask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

class Task
{
public:

	Task() = default;
	Task(const std::function<void()> function)
		: m_function{ function } {}

	void operator()() { m_function();}

	std::function<void()> m_function;
};

class Scheduler final
{
public:
	enum StateE : int { IDLE, RUNNING };

	Scheduler(const CPU& cpu)
		: m_cpu{ cpu }
		, m_worker{ nullptr }
		, m_state{ IDLE }
	{}

	void ExecuteTask();

	bool HasIdleWorkers();
	bool HasRunnableWorkers();

	void WakeUpNext();
	void WakeUpNextIdle();

	void SaveRunnable();
	void SaveIdle();

	void ContinueRunnable();

	Worker* NextRunnableWorker();

	Worker* NextFreeWorker();
	bool HasTasks();
	void ScheduleWorker(Worker& worker);
	Worker* NextRunWorker();
	Task NextTask();

	void Schedule();
	void ScheduleNextIdle();

public:
	const CPU& m_cpu;
	std::deque<Worker*> m_runnableQueue;
	std::deque<Worker*> m_idleQueue;
	Worker* m_worker;
	StateE m_state;
	// std::vector<Worker*> m_vec;
};

class CPU final
{
public:
	CPU(uint64_t cpu_id, uint64_t cpu_mask)
		: m_id{ cpu_id }
		, m_mask{ cpu_mask }
		, m_scheduler{ *this }
		, m_workers{}
	{
	}

public:

	void InitWorkers();

	void WorkerEntryPoint(Worker& worker);
	void BindThread();
	void ExecuteTasks();

public:
	uint64_t m_id;
	uint64_t m_mask;

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

	enum StateE : int { IDLE, RUNNABLE, RUNNING };

	// Create worker object and start worker thread on a provided CPU.
	//
	Worker(uint64_t id, CPU& cpu)
		: m_id{ id }
		, m_state{ IDLE }
		, m_mtx{}
		, m_sync{}
		, m_cpu{ cpu }
		, m_task{}
		, m_scheduler{ cpu.m_scheduler }
		, m_thread{ &CPU::WorkerEntryPoint, &cpu, std::ref(*this) }
	{
		// Wait for a signal from created thread, so we can continue when it is ready.
		//
		std::unique_lock<std::mutex> lock{ m_mtx };
		m_sync.wait(lock);
	}

	~Worker()
	{
		if (m_thread.joinable())
			m_thread.join();
	};

	Worker(const Worker& other) = delete;
	Worker(const Worker&& other) = delete;

	Worker& operator=(const Worker& other) = delete;

	void Main();

	void WaitForTask();
	void ExecuteTask(Task task);

	void yield();

	void WakeUp()
	{
		m_sync.notify_one();
	}

	bool SyncYield();

	void Sleep()
	{
		std::unique_lock<std::mutex> lock{ m_mtx };
		m_sync.wait(lock);

		// std::cout << "Waking worker " << tls_worker->ID() << "\n";
	}

	bool Sync();

	constexpr uint64_t ID() const { return m_id; }
	constexpr CPU& CPUg() const { return m_cpu; }
	constexpr StateE State() const { return m_state; }

public:

	// TODO: reorganize data members for quick access.
	//
	std::mutex m_mtx;
	std::condition_variable m_sync;
	std::thread m_thread;

	uint64_t m_id;
	StateE m_state;

	Task m_task;
	CPU& m_cpu;
	Scheduler& m_scheduler;
};

#endif

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

//
//B b;
//A g_a{ b };

class Scheduler;
class CPU;
class Worker;
class Task;

thread_local Worker* tls_worker;

class CPUs final
{
public:
	CPUs();

	// Start workers on every available CPU.
	//
	void Init();

	void Execute();

private:
	uint64_t m_count;
	uint64_t m_availProcMask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

class Task
{
public:
	Task();
	std::function<void()> function;
};

class Scheduler final
{
public:
	Scheduler() = default;

	void Main(Worker& worker);

	//void RunNext(Worker& current, Worker& next)
	//{
	//	next.WakeUp();
	//	current.Sleep();
	//}

	void Insert(Worker& worker);

	void Yielddd(Worker& worker);

public:

	std::deque<Worker*> m_runnableQueue;
	std::deque<Worker*> m_idleQueue;
	std::deque<Task> m_tasksQueue;
};

class CPU final
{
public:
	CPU(uint64_t cpu_id, uint64_t afinityMask)
		: m_id{ cpu_id }
		, m_afinityMask{ afinityMask }
		, m_scheduler{}
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
	uint64_t m_afinityMask;

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

	enum StateE : int { IDLE, RUNNING };

	// Create worker object and start worker thread on a provided CPU.
	//
	Worker(uint64_t id, CPU& cpu)
		: m_id{ id }
		, m_state{ IDLE }
		, m_sync{}
		, m_mtx{}
		, m_cpu{ cpu }
		, m_scheduler{ cpu.m_scheduler }
		, m_thread{ &CPU::WorkerEntryPoint, &cpu, std::ref(*this) }
	{
	}

	~Worker()
	{
		if (m_thread.joinable())
			m_thread.join();
	};

	Worker(const Worker& other) = delete;
	Worker(const Worker&& other) = delete;

	Worker& operator=(const Worker& other) = delete;

	void Yielddd()
	{
		// m_scheduler.Yielddd();
		m_scheduler.Yielddd(*this);
	}

	void WakeUp()
	{
		m_sync.notify_one();
	}

	void Sleep()
	{
		std::unique_lock<std::mutex> lock{ m_mtx };
		m_sync.wait(lock);
		std::cout << "Waking worker " << tls_worker->ID() << "\n";
	}

	constexpr uint64_t ID() const { return m_id; }
	constexpr CPU& CPUg() const { return m_cpu; }
	constexpr StateE State() const { return m_state; }

private:

	// TODO: reorganize data members for quick access.
	//
	uint64_t m_id;
	StateE m_state;

	std::condition_variable m_sync;
	std::mutex m_mtx;

	CPU& m_cpu;
	Scheduler& m_scheduler;

	std::thread m_thread;
};

#endif

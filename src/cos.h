#pragma once

#ifndef COS_H
#define COS_H

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

	void operator()() { m_function(); }

	std::function<void()> m_function;
};

class Scheduler final
{
public:
	enum StateE : int { INITIALIZING, RUNNING };

	Scheduler(const CPU& cpu);

	void Start();

	void ExecuteTask();

	bool HasIdleWorkers();
	bool HasRunnableWorkers();

	void WakeUpNext();
	void WakeUpNextIdle();

	void SaveRunnable();
	void SaveIdle();

	void PrepareRunningWorker();
	void IdleLooping();

	Worker* NextFreeWorker();
	bool HasTasks();
	void ScheduleWorker(Worker& worker);
	Task NextTask();

	void Schedule();
	void ScheduleNextIdle();

public:
	const CPU& m_cpu;
	std::deque<Worker*> m_runnableQueue;
	std::deque<Worker*> m_idleQueue;
	Worker* m_worker;
	StateE m_state;
};

class CPU final
{
public:
	CPU(uint64_t cpu_id, uint64_t cpu_mask);

public:

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

enum SyncType : int { MAIN, YIELD };

// TODO: Check whether worker should be placed on std::hardware_constructive_interference_size alignment,
// to avoid false sharing.
//
class Worker final
{
public:

	enum StateE : int { IDLE, IDLE_LOOPING, RUNNABLE, RUNNING };

	// Create worker object and start worker thread on a provided CPU.
	//
	Worker(uint64_t id, CPU& cpu);

	~Worker();

	Worker(const Worker& other) = delete;
	Worker(const Worker&& other) = delete;

	Worker& operator=(const Worker& other) = delete;

	void Main();

	void yield();

	bool SyncMain();
	bool SyncYield();

	template<SyncType type>
	void Sync();

	void SetState(StateE state);

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

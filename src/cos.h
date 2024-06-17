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
	uint32_t m_systemCpusCount;
	uint64_t m_availCpusMask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

class Scheduler final
{
public:
	enum StateE : int { INITIALIZING, RUNNING, EXITING };

	Scheduler(const CPU& cpu);

	void Start();

	void EnqueueTask(const Task& task);

	bool HasIdleWorkers() const;
	bool HasRunnableWorkers() const;

	void WakeUpNext();
	void WakeUpNextIdle();

	void SaveRunnable();
	void SaveIdle();

	void PrepareWorker();
	void IdleLooping();

	bool HasTasks() const;
	void ScheduleWorker(Worker& worker);
	Task NextTask();

	void Schedule();
	void ScheduleNextIdle();

	void ExitWorkers() const;
	bool Exiting() const;
	bool Initializing() const;
	Worker* worker() const;

	void SetState(StateE state);

public:
	const CPU& m_cpu;
	std::mutex m_workersMtx;
	std::deque<Worker*> m_runnableQueue;
	std::deque<Worker*> m_idleQueue;
	Worker* m_worker;
	std::mutex m_tasksMtx;
	std::deque<Task> m_tasks;
	StateE m_state;
};

class CPU final
{
public:
	CPU(uint64_t cpu_id, uint64_t cpu_mask);

public:

	void WorkerEntryPoint(Worker& worker) const;
	void BindThread() const;
	void ExecuteTask(const Task& task);
	void IncLoad();
	void DecLoad();
	uint64_t Load() const;

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
	enum SyncType : int { MAIN, YIELD };
	enum StateE : int { INITIALIZING, IDLE, IDLE_LOOPING, RUNNABLE, RUNNING, EXITING };

	static bool StateIdle(StateE state) { return state == IDLE || state == IDLE_LOOPING; }
	static bool StateRunnable(StateE state) { return state == RUNNABLE || state == RUNNING; }

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

	bool IdleLoop() const;
	void SetState(StateE state);
	bool Exiting() const;

	constexpr uint64_t ID() const { return m_id; }
	constexpr CPU& CPUg() const { return m_cpu; }
	constexpr StateE State() const { return m_state; }

public:

	// TODO: reorganize data members for quick access.
	//
	std::condition_variable m_cv;
	std::thread m_thread;

	uint64_t m_id;
	StateE m_state;

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
extern TaskManager taskManager;
extern thread_local Worker* tls_worker;

#endif

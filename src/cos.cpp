#include <cstdint>
#include <cassert>
#include <memory>

#include <iostream>
#include <chrono>

#include "os_specific.h"
#include "cos.h"

using namespace std::chrono_literals;

constexpr uint64_t CFG_allowedCpus = 0b00001111;
constexpr int CFG_workersPerCPU = 16;
constexpr auto CFG_idleSleepDuration = 1ms;

CPUs::CPUs()
	: m_systemCpusCount{ CpusCount() }
	, m_availCpusMask{ CpusAvailMask() }
{
	m_availCpusMask &= CFG_allowedCpus;

	// Create new CPU for each bit available in available CPUs mask.
	//
	for (uint64_t cpu_id = 0, cpusMask = m_availCpusMask; cpusMask != 0; ++cpu_id, cpusMask >>= 1)
		if (cpusMask & 1)
			m_cpus.emplace_back(std::make_unique<CPU>(cpu_id, 1 << cpu_id));
}

CPU& CPUs::MinLoadCPU() const
{
	constexpr auto cmp = [](const std::unique_ptr<CPU>& left, const std::unique_ptr<CPU>& right) { return left->Load() < right->Load(); };
	return **std::min_element(m_cpus.begin(), m_cpus.end(), cmp);
}

CPU::CPU(uint64_t cpu_id, uint64_t cpu_mask)
	: m_id{ cpu_id }
	, m_mask{ cpu_mask }
	, m_load{ 0 }
	, m_scheduler{ *this }
	, m_workers{}
{
	for (int i = 0; i < CFG_workersPerCPU; ++i)
		m_workers.push_back(std::make_unique<Worker>(i, *this));

	m_scheduler.Start();
}

void CPU::WorkerEntryPoint(Worker& worker) const
{
	BindThread();

	std::cout << "Started thread: " << worker.ID() << " on CPU " << worker.CPUg().m_id << std::endl;

	worker.Main();

	std::cout << "Ended thread: " << worker.ID() << " on CPU " << worker.CPUg().m_id << std::endl;
}

void CPU::ExecuteTask(const Task& task)
{
	m_scheduler.EnqueueTask(task);
}

void CPU::IncLoad() { ++m_load; }
void CPU::DecLoad() { --m_load; }
uint64_t CPU::Load() const { return m_load + m_scheduler.m_tasks.size(); }

Scheduler::Scheduler(const CPU& cpu)
	: m_cpu{ cpu }
	, m_worker{ nullptr }
	, m_state{ INITIALIZING }
{ }

// Start scheduler.
//
void Scheduler::Start()
{
	m_state = RUNNING;

	IdleLooping();
	m_worker->m_cv.notify_one();
}

void Scheduler::EnqueueTask(const Task& task)
{
	std::scoped_lock<std::mutex> lock{ m_tasksMtx };
	m_tasks.push_back(task);
}

bool Scheduler::HasIdleWorkers() const { return !m_idleQueue.empty(); }
bool Scheduler::HasRunnableWorkers() const { return !m_runnableQueue.empty(); }

void Scheduler::WakeUpNext()
{
	m_worker = m_runnableQueue.front();
	m_runnableQueue.pop_front();

	m_worker->SetState(Worker::RUNNING);
	m_worker->m_cv.notify_one();
}

void Scheduler::WakeUpNextIdle()
{
	m_worker = m_idleQueue.front();
	m_idleQueue.pop_front();

	m_worker->SetState(Worker::RUNNING);
	m_worker->m_cv.notify_one();
}

void Scheduler::SaveRunnable()
{
	m_runnableQueue.push_back(m_worker);

	m_worker->SetState(Worker::RUNNABLE);
	m_worker = nullptr;
}

void Scheduler::SaveIdle()
{
	m_idleQueue.push_back(m_worker);

	m_worker->SetState(Worker::IDLE);
	m_worker = nullptr;
}

bool Scheduler::HasTasks() const
{
	return !m_tasks.empty();
}

void Scheduler::ScheduleWorker(Worker& worker)
{
	m_runnableQueue.push_back(&worker);
}

void Scheduler::PrepareWorker()
{
	m_worker = m_runnableQueue.front();
	m_runnableQueue.pop_front();

	m_worker->SetState(Worker::RUNNING);
}

void Scheduler::IdleLooping()
{
	m_worker = m_idleQueue.back();
	m_idleQueue.pop_back();

	m_worker->SetState(Worker::IDLE_LOOPING);
}

Task Scheduler::NextTask()
{
	std::scoped_lock<std::mutex> lock{ m_tasksMtx };
	Task t = m_tasks.front();
	m_tasks.pop_front();

	return t;
}

void Scheduler::ScheduleNextIdle()
{
	Worker* worker = m_idleQueue.front();
	m_idleQueue.pop_front();

	worker->m_task = NextTask();
	worker->SetState(Worker::RUNNABLE);
	m_runnableQueue.push_back(worker);
}

void Scheduler::Schedule()
{
	while (HasTasks() && HasIdleWorkers())
		ScheduleNextIdle();
}

bool Scheduler::Initializing() const
{
	return m_state == INITIALIZING;
}

bool Scheduler::Exiting() const
{
	return m_state == EXITING;
}

void Scheduler::SetState(StateE state)
{
	m_state = state;
}

Worker* Scheduler::worker() const
{
	return m_worker;
}

void Scheduler::ExitWorkers() const
{
	for (Worker* worker : m_idleQueue)
	{
		worker->SetState(Worker::EXITING);
		worker->m_cv.notify_one();
	}
}

// Creates worker object and starts worker thread on a provided CPU.
//
Worker::Worker(uint64_t id, CPU& cpu)
	: m_id{ id }
	, m_state{ INITIALIZING }
	, m_cv{}
	, m_cpu{ cpu }
	, m_task{}
	, m_scheduler{ cpu.m_scheduler }
	, m_thread{ &CPU::WorkerEntryPoint, &cpu, std::ref(*this) }
{
	// Wait for a signal from created thread, so we can continue when it is ready.
	//
	std::unique_lock<std::mutex> lock{ m_scheduler.m_workersMtx };
	m_cv.wait(lock);
}

Worker::~Worker()
{
	m_scheduler.SetState(Scheduler::EXITING);

	if (m_thread.joinable())
		m_thread.join();
}

void Worker::SetState(Worker::StateE state)
{
	if (StateIdle(m_state) && StateRunnable(state))
		m_cpu.IncLoad();
	else if (StateRunnable(m_state) && StateIdle(state))
		m_cpu.DecLoad();

	m_state = state;
}

bool Worker::Exiting() const
{
	return m_state == EXITING;
}

// Yields current worker and wakes up next worker for execution.
//
void Worker::yield()
{
	Sync<YIELD>();
}

// Synchronization point for the workers in yield.
//
bool Worker::SyncYield()
{
	m_scheduler.SaveRunnable(); // Push current worker at the end of runnable queue.
	m_scheduler.Schedule();

	m_scheduler.PrepareWorker();

	if (m_scheduler.worker() != this)
	{
		std::cout << "CPU " << m_cpu.m_id << ": Yielding worker " << this->ID() << "\n";
		std::cout << "CPU " << m_cpu.m_id << ": Waking worker   " << m_scheduler.worker()->ID() << "\n";
		m_scheduler.worker()->m_cv.notify_one(); // wake up next.
		return true; // go to sleep
	}
	else
		return false; // continue with execution.
}

// Synchronization point for the workers.
//
bool Worker::SyncMain()
{
	// Park all new threads before scheduler is initialized.
	// Notify thread that created us to continue.
	//
	if (m_scheduler.Initializing())
	{
		m_scheduler.SaveIdle();
		m_cv.notify_one();
		return true; // go to sleep.
	}

	m_scheduler.SaveIdle();
	m_scheduler.Schedule();

	if (m_scheduler.HasRunnableWorkers())
	{
		m_scheduler.PrepareWorker();

		if (m_scheduler.worker() != this)
		{
			std::cout << "CPU " << m_cpu.m_id << ": Yielding worker " << this->ID() << "\n";
			std::cout << "CPU " << m_cpu.m_id << ": Waking worker   " << m_scheduler.worker()->ID() << "\n";
			m_scheduler.worker()->m_cv.notify_one(); // wake up next.
			return true; // go to sleep
		}
	}
	else if (m_scheduler.Exiting())
		m_scheduler.ExitWorkers();
	else
		m_scheduler.IdleLooping();

	return false; // continue with execution.
}

// Synchronization point for the workers.
// We will call sync function based on provided sync type and go to sleep if sync function returns true.
// Sync function will wake up new worker if needed.
// Notes:
// There is a single mutex on scheduler used for workers synchronization and every worker has it's own condition variable.
// In order to atomically suspend single worker thread (go to sleep by calling wait) and wake up next,
// we will take lock on mutex before notifying another thread to wake up. Condition_variable::wait function
// guarantees that it will unlock mutex and go to sleep atomically and it also guarantees that it will take lock on mutex
// when wait is done. So when we notify another thread to wake up we are already holding lock on mutex
// (and notified thread can not wake up until we release lock) and mutex will be unlocked only when we call wait on this thread,
// which will release lock and wake another thread.
//
template<Worker::SyncType type>
void Worker::Sync()
{
	// Pointer to sync member function. Horrible syntax.
	//
	constexpr bool (Worker::*sync)(void) = type == MAIN ? &Worker::SyncMain : &Worker::SyncYield;

	std::unique_lock<std::mutex> lock{ m_scheduler.m_workersMtx };
	if ((this->*sync)())
		m_cv.wait(lock);
}

bool Worker::IdleLoop() const
{
	if (m_state == IDLE_LOOPING)
	{
		std::cout << "CPU " << m_cpu.m_id << ": idle looping worker " << this->ID() << "\n";
		std::this_thread::sleep_for(CFG_idleSleepDuration);
		return true;
	}
	else
		return false;
}

// Main worker loop.
//
void Worker::Main()
{
	tls_worker = m_scheduler.m_worker = this;

	while (true)
	{
		Sync<MAIN>();

		if (Exiting())
			return;

		if (IdleLoop())
			continue;

		try
		{
			m_task();
			std::cout << "CPU " << m_cpu.m_id << ": worker id " << this->ID() << " task done.\n";
		}
		catch (std::exception& ex)
		{
			std::cout << ex.what() << "\n";
		}
	}
}

void TaskManager::ExecuteTask(Task task)
{
	CPU& bestCPU = m_cpus.MinLoadCPU();
	bestCPU.ExecuteTask(task);
}

CPUs cpus;
TaskManager taskManager{ cpus };
thread_local Worker* tls_worker;

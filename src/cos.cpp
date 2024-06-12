#include <cstdint>
#include <cassert>

// Rnadom
#include <cstdlib>
#include <random>

std::random_device rd;     // only used once to initialise (seed) engine
std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)


#include "os_specific.h"
#include "cos.h"

using namespace std::chrono_literals;

constexpr uint64_t FS_AllowedCPUs = 0b00000001;
constexpr int workersPerCPU = 20;
constexpr int tasksPerCPU = 40;
constexpr auto workerSleepTime = 1ms;

thread_local Worker* tls_worker;

std::mutex g_tasksMtx;
std::deque<Task> g_tasksQueue;

void f2()
{
	tls_worker->yield();
	return;
}

void f1()
{
	auto start = std::chrono::high_resolution_clock::now();

	std::vector<int> v;
	for (int i = 0; i < 1000; ++i)
		v.push_back(rand());
	
	std::uniform_int_distribution<int> uni(0, 6);
	int funcDur = uni(rng);

	int i = 0;
	while (true)
	{
		auto end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double, std::milli> duration = end - start;
		if (duration.count() > funcDur * 1000)
		{
			std::cerr << "Task execution exceeded time limit of " << duration.count() << "ms." << std::endl;
			break;
		}

		if (v[rand() % v.size()] == rand() % v.size() && i++ % 100 == 0)
		{
			tls_worker->yield();
		}
	}

	return;
}

CPUs::CPUs()
	: m_systemCpusCount{ CpusCount() }
	, m_availCpusMask{ CpusAvailMask() }
{
	m_availCpusMask &= FS_AllowedCPUs;

	// Create new CPU for each bit available in available CPUs mask.
	//
	for (uint64_t cpu_id = 0, cpusMask = m_availCpusMask; cpusMask != 0; ++cpu_id, cpusMask >>= 1)
		if (cpusMask & 1)
			m_cpus.emplace_back(std::make_unique<CPU>(cpu_id, 1 << cpu_id));
}

CPU::CPU(uint64_t cpu_id, uint64_t cpu_mask)
	: m_id{ cpu_id }
	, m_mask{ cpu_mask }
	, m_scheduler{ *this }
	, m_workers{}
{
	for (int i = 0; i < workersPerCPU; ++i)
		m_workers.push_back(std::make_unique<Worker>(i, *this));

	m_scheduler.Start();
}

void CPU::WorkerEntryPoint(Worker& worker) const
{
	BindThread();

	std::cout << "Started thread: " << worker.ID() << " on CPU " << worker.CPUg().m_id << " " << "Win32: " << GetCurrentProcessorNumber() <<  std::endl;

	worker.Main();

	std::cout << "Ended thread: " << worker.ID() << " on CPU " << worker.CPUg().m_id << " " << "Win32: " << GetCurrentProcessorNumber() <<  std::endl;
}

void CPUs::Execute() const
{
	// for (int i = 0; i < tasksPerCPU; ++i)
	{
		for (auto& cpu : m_cpus)
			cpu->ExecuteTasks();
	}
}

void CPU::ExecuteTasks() const
{
	// for (int i = 0; i < tasksPerCPU; ++i)
	// 	m_scheduler.ExecuteTask();
	
	for (int i = 0; i < tasksPerCPU; ++i)
	{
		// if (i % 10 == 0)
		// 	std::this_thread::sleep_for(1ms);

		m_scheduler.ExecuteTask();
	}
}

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

	m_worker = m_idleQueue.front();
	m_idleQueue.pop_front();

	m_worker->SetState(Worker::IDLE_LOOPING);
	m_worker->m_sync.notify_one();
}

void Scheduler::ExecuteTask() const
{
	std::scoped_lock<std::mutex> lock{ g_tasksMtx };
	g_tasksQueue.push_back(Task{ f1 });
}

bool Scheduler::HasIdleWorkers() const { return !m_idleQueue.empty(); }
bool Scheduler::HasRunnableWorkers() const { return !m_runnableQueue.empty(); }

void Scheduler::WakeUpNext()
{
	m_worker = m_runnableQueue.front();
	m_runnableQueue.pop_front();

	m_worker->SetState(Worker::RUNNING);
	m_worker->m_sync.notify_one();
}

void Scheduler::WakeUpNextIdle()
{
	m_worker = m_idleQueue.front();
	m_idleQueue.pop_front();

	m_worker->SetState(Worker::RUNNING);
	m_worker->m_sync.notify_one();
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
	// TODO: Check whether we should lock here.
	//
	// std::scoped_lock<std::mutex> lock{ g_tasksMtx };
	return !g_tasksQueue.empty();
}

void Scheduler::ScheduleWorker(Worker& worker)
{
	m_runnableQueue.push_back(&worker);
}

void Scheduler::PrepareRunningWorker() const
{
	m_worker->m_task = NextTask();
	m_worker->SetState(Worker::RUNNING);
}

void Scheduler::IdleLooping() const
{
	m_worker->SetState(Worker::IDLE_LOOPING);
}

Task Scheduler::NextTask() const
{
	std::scoped_lock<std::mutex> lock{ g_tasksMtx };
	Task t = g_tasksQueue.front();
	g_tasksQueue.pop_front();

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

void Scheduler::ExitWorkers() const
{
	m_worker->SetState(Worker::EXITING);

	for (Worker* worker : m_idleQueue)
	{
		worker->SetState(Worker::EXITING);
		worker->m_sync.notify_one();
	}
}

// Creates worker object and starts worker thread on a provided CPU.
//
Worker::Worker(uint64_t id, CPU& cpu)
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

Worker::~Worker()
{
	m_scheduler.SetState(Scheduler::EXITING);

	if (m_thread.joinable())
		m_thread.join();
}

void Worker::SetState(Worker::StateE state)
{
	// TODO: Collect some statistics here.
	//
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
	m_scheduler.Schedule();

	if (m_scheduler.HasRunnableWorkers())
	{
		std::cout << "CPU " << m_cpu.m_id << ": Yielding worker " << this->ID() << "\n";
		std::cout << "CPU " << m_cpu.m_id << ": Waking worker   " << m_scheduler.m_runnableQueue.front()->ID() << "\n";

		m_scheduler.SaveRunnable(); // Push current worker at the end of runnable queue.
		m_scheduler.WakeUpNext();
		return true; // go to sleep.
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
		m_sync.notify_one();
		return true; // go to sleep.
	}

	m_scheduler.Schedule();

	if (m_scheduler.HasRunnableWorkers())
	{
		m_scheduler.SaveIdle();
		m_scheduler.WakeUpNext();
		return true; // go to sleep.
	}
	else if (m_scheduler.HasTasks())
	{
		m_scheduler.PrepareRunningWorker();
		return false; // continue with execution.
	}
	else if (m_scheduler.Exiting())
	{
		m_scheduler.ExitWorkers();
		return false; // continue with exit.
	}
	else
	{
		m_scheduler.IdleLooping();
		return false; // continue with idle looping.
	}
}

template<SyncType type>
void Worker::Sync()
{
	// Synchronize workers.
	//
	std::unique_lock<std::mutex> lock{ m_mtx };

	if constexpr (type == MAIN)
	{
		if (SyncMain())
			m_sync.wait(lock);
	}
	else if constexpr (type == YIELD)
	{
		if (SyncYield())
			m_sync.wait(lock);
	}
}

bool Worker::IdleLoop() const
{
	if (m_state == IDLE_LOOPING)
	{
		std::cout << "CPU " << m_cpu.m_id << ": idle looping worker " << this->ID() << "\n";
		std::this_thread::sleep_for(workerSleepTime);
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

using namespace std::chrono_literals;

std::condition_variable cv;
std::mutex cv_m;

std::mutex io_mtx;

void fun()
{
	SetThreadAffinityMask(GetCurrentThread(), 1);
	std::this_thread::sleep_for(1s);

	{
		std::scoped_lock<std::mutex> io_lock{ io_mtx };
		std::cout << "Thread " << std::this_thread::get_id() << " running on " << GetCurrentProcessorNumber() << std::endl;
	}

	{
		std::unique_lock<std::mutex> lock(cv_m);
		std::cout << "Thread " << std::this_thread::get_id() << " waiting on conditional lock. \n";
		cv.wait(lock);
		std::cout << "Thread " << std::this_thread::get_id() << " resuming with lock. \n";
	}

	std::cout << "Thread " << std::this_thread::get_id() << " released lock and notifying one. \n";
	cv.notify_one();
	for (int i = 0; i < 100000000; ++i)
		if (i % 100000 == 0)
			std::cout << "I (" << std::this_thread::get_id() << ") am doing some really hard work.\n";
	// std::cout << "I (" << std::this_thread::get_id() << ") am doing to sleep.\n";
	std::this_thread::sleep_for(1s);
	std::cout << "Thread " << std::this_thread::get_id() << " resumed on " << GetCurrentProcessorNumber() << ". \n";
}

void hardwork()
{
	SetThreadAffinityMask(GetCurrentThread(), 1);
	std::this_thread::sleep_for(1s);

	std::vector<int> v;
	for (int i = 0; i < 1000; ++i)
		v.push_back(rand());

	int i = 0;
	while (true)
	{
		if (v[rand() % v.size()] == rand() % v.size() && i++ % 10000 == 0)
			std::cout << "Hit\n";
	}
}

void entryPoint()
{

}

int main()
{
	//unsigned __int64 thisProcessAfinityMask = 0;
	//unsigned __int64 systemAfinityMask = 0;
	//GetProcessAffinityMask(GetCurrentProcess(), &thisProcessAfinityMask, &systemAfinityMask);
	//std::bitset<32> bsp{ thisProcessAfinityMask };
	//std::bitset<32> bss{ systemAfinityMask };

	//std::cout << bsp << "\n" << bss << "\n";

	//std::cout << "Thread " << std::this_thread::get_id() << " running on " << GetCurrentProcessorNumber();

	//unsigned __int64 newThreadAfinityMask = 1 << 0;

	//auto res = SetThreadAffinityMask(GetCurrentThread(), newThreadAfinityMask);
	//std::cout << res << "\n";

	//std::this_thread::sleep_for(1s);
	//std::cout << "Thread " << std::this_thread::get_id() << " running on " << GetCurrentProcessorNumber();

	//std::vector<std::thread> v;

	//for (int i = 0; i < 3; ++i)
	//	v.push_back(std::thread{ fun });

	//std::this_thread::sleep_for(3s);

	//std::cout << "Main thread notifying one. \n";
	//cv.notify_one();

	//for (auto it = v.begin(); it != v.end(); ++it)
	//	it->join();

	//SetThreadAffinityMask(GetCurrentThread(), 1);
	//std::this_thread::sleep_for(1s);

	//std::thread hardthread{ hardwork };

	//for (int i = 0; true; ++i)
	//{
	//	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	//	// std::this_thread::yield();
	//	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

	//	auto res = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	//	if (res > 1)
	//		std::cout << res << "ms\n";

	//	// std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[µs]" << std::endl;
	//	// std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::nanoseconds> (end - begin).count() << "[ns]" << std::endl;
	//}

	//hardthread.join();
	//return 0;

	// TODO: Izmeriti koliko traje kontext switch sa notify and wait: pustiti 2 thread-a na istom koru i startovati jedan sa tajmerom.
	// SIgnal drugom i on odmah hvata vreme i stampa.

	//std::thread t{ entryPoint };
	//t.join();

	// std::cout << GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

	/*while (true)
		std::this_thread::sleep_for(workerSleepTime);*/

	CPUs cpus;
	
	cpus.Execute();
}

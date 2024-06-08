#include <cstdint>
#include <cassert>

#include "os_specific.h"
#include "cos.h"

using namespace std::chrono_literals;

constexpr uint64_t FS_AllowedCPUs = 0b00000001;
constexpr int workersPerCPU = 5;
constexpr int tasksPerCPU = 100;
constexpr auto workerSleepTime = 1ms;

thread_local Worker* tls_worker;

std::deque<Task> g_tasksQueue;

void f1()
{
	auto start = std::chrono::high_resolution_clock::now();

	std::vector<int> v;
	for (int i = 0; i < 1000; ++i)
		v.push_back(rand());

	int i = 0;
	while (true)
	{
		auto end = std::chrono::high_resolution_clock::now();

		std::chrono::duration<double, std::milli> duration = end - start;
		if (duration.count() > 1000)
		{
			std::cerr << "Task execution exceeded time limit of 4ms: " << duration.count() << "ms." << std::endl;
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

// Start workers on every available CPU.
//
void CPUs::Init()
{
	for (auto& it : m_cpus)
	{
		it->InitWorkers();
	}
}

void CPU::InitWorkers()
{
	for (int i = 0; i < workersPerCPU; ++i)
		m_workers.push_back(std::make_unique<Worker>(i, *this));
}

void CPU::WorkerEntryPoint(Worker& worker)
{
	BindThread();
	tls_worker = &worker;

	std::cout << "Started thread: " << worker.ID() << " on CPU " << worker.CPUg().m_id << " " << "Win32: " << GetCurrentProcessorNumber() <<  std::endl;

	worker.Main();
}

void CPUs::Execute()
{
	while (true)
	{
		for (auto& cpu : m_cpus)
			cpu->ExecuteTasks();
	}
}

void CPU::ExecuteTasks()
{
	// for (int i = 0; i < tasksPerCPU; ++i)
	// 	m_scheduler.ExecuteTask();
	
	// while (true)
	{
		m_scheduler.ExecuteTask();
	}
}

void Scheduler::ExecuteTask()
{
	if (Idle())
	{
		/*for (int i = 0; i < 1000; ++i)
			assert(m_worker == nullptr);*/
		g_tasksQueue.push_back(Task{ f1 });
		Schedule();
		WakeUpNext();
	}
	else
	{
		assert(m_worker != nullptr);
		std::scoped_lock<std::mutex> lock{ m_worker->m_mtx };
		g_tasksQueue.push_back(Task{ f1 });
	}
}

bool Scheduler::HasIdleWorkers() { return !m_idleQueue.empty(); }
bool Scheduler::HasRunnableWorkers() { return !m_runnableQueue.empty(); }

void Scheduler::WakeUpNext()
{
	m_worker = m_runnableQueue.front();
	m_runnableQueue.pop_front();
	m_worker->m_sync.notify_one();
}

void Scheduler::WakeUpNextIdle()
{
	m_worker = m_idleQueue.front();
	m_idleQueue.pop_front();
	m_worker->m_sync.notify_one();
}

void Scheduler::SaveRunnable()
{
	m_runnableQueue.push_back(m_worker);
	m_worker = nullptr;
}

void Scheduler::SaveIdle()
{
	m_idleQueue.push_back(m_worker);
	m_worker = nullptr;
}

void Scheduler::ContinueRunnable()
{
	m_worker = m_runnableQueue.front();
	m_runnableQueue.pop_front();
}

Worker* Scheduler::NextRunnableWorker() { return !m_runnableQueue.empty() ? m_runnableQueue.front() : nullptr; }

Worker* Scheduler::NextFreeWorker()
{
	Worker* worker = nullptr;

	if (m_idleQueue.size() > 0)
	{
		worker = m_idleQueue.front();
		m_idleQueue.pop_front();
	}

	return worker;
}

bool Scheduler::HasTasks()
{
	return !g_tasksQueue.empty();
}

void Scheduler::ScheduleWorker(Worker& worker)
{
	m_runnableQueue.push_back(&worker);
}

Worker* Scheduler::NextRunWorker()
{
	Worker* worker = nullptr;

	if (m_runnableQueue.size() > 0)
	{
		worker = m_runnableQueue.front();
		m_runnableQueue.pop_front();
	}

	return worker;
}

Task Scheduler::NextTask()
{
	Task t = g_tasksQueue.front();
	g_tasksQueue.pop_front();

	return t;
}

void Scheduler::ScheduleNextIdle()
{
	Worker* idle = m_idleQueue.front();
	m_idleQueue.pop_front();

	idle->m_task = NextTask();
	m_runnableQueue.push_back(idle);
}

void Scheduler::Schedule()
{
	while (HasTasks() && HasIdleWorkers())
		ScheduleNextIdle();
}

bool Scheduler::Idle() { return m_worker == nullptr; }

// Synchronization point for the workers in yield.
// If there are pending tasks in tasks queue, wake up next worker and go to sleep.
// Otherwise, just continue with execution.
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

// Yields current worker and wakes up next worker for execution.
//
void Worker::yield()
{
	Sync<YIELD>();
}

// Synchronization point for the workers.
//
bool Worker::SyncMain()
{
	m_scheduler.Schedule();

	if (m_scheduler.HasRunnableWorkers())
	{
		m_scheduler.SaveIdle();
		m_scheduler.WakeUpNext();
		return true; // go to sleep.
	}
	else if (m_scheduler.HasTasks())
	{
		m_task = m_scheduler.NextTask();
		return false; // continue with execution.
	}
	else
	{
		m_scheduler.SaveIdle();
		return true; // go to sleep.
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

void Worker::Main()
{
	m_sync.notify_one();
	m_scheduler.m_worker = this;

	while (true)
	{
		Sync<MAIN>();

		if (false);
			// Exit code -> !m_scheduler.Running() && g_tasksQueue.empty();

		try
		{
			m_task();
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
	cpus.Init();

	cpus.Execute();
}

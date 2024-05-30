#include "cos.h"

constexpr int workersPerCPU = 3;
constexpr uint64_t FS_AllowedCPUs = 0b00001111;

CPUs::CPUs()
	: m_count{ GetActiveProcessorCount(ALL_PROCESSOR_GROUPS) }
{
	uint64_t thisProcessAfinityMask = 0;
	uint64_t systemAfinityMask = 0;

	GetProcessAffinityMask(GetCurrentProcess(), &thisProcessAfinityMask, &systemAfinityMask);

	std::bitset<32> bsp{ thisProcessAfinityMask };
	std::bitset<32> bss{ systemAfinityMask };

	std::bitset<32> aps{ thisProcessAfinityMask & systemAfinityMask };

	m_availProcMask = thisProcessAfinityMask & systemAfinityMask;

	std::cout << bsp << " " << bss << "\n";
	std::cout << "Available processors mask: " << m_availProcMask << " " << aps << std::endl;

	uint64_t availProcMask = m_availProcMask;
	availProcMask &= FS_AllowedCPUs;

	for (uint64_t i = 0; availProcMask != 0; ++i, availProcMask >>= 1)
		if (availProcMask & 1)
			m_cpus.emplace_back(std::make_unique<CPU>(i, 1 << i));
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

void CPUs::Execute()
{
	for (auto& cpu : m_cpus)
		cpu->ExecuteTasks();
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

	{
		static std::mutex io_mtx;
		std::scoped_lock<std::mutex> lock(io_mtx);

		std::cout << "Started thread: " << worker.ID() << " on CPU " << worker.CPUg().m_id << " " << "Win32: " << GetCurrentProcessorNumber() <<  std::endl;
	}

	m_scheduler.Insert(worker);
	m_scheduler.Main(worker);
}

// Binds current thread to this CPU.
//
void CPU::BindThread()
{
#ifdef _WIN32
	SetThreadAffinityMask(GetCurrentThread(), m_afinityMask);
#else
	// linux
#endif
}

void CPU::ExecuteTasks()
{
	// Simulate some task execution here.
//
	for (int i = 0; i < workersPerCPU; ++i)
		m_scheduler.m_tasksQueue.push_back(Task{});

	using namespace std::chrono_literals;
	std::this_thread::sleep_for(1s);

	m_scheduler.m_runnableQueue.front()->WakeUp();
}

void Scheduler::Insert(Worker& worker)
{
	{
		static std::mutex mtx;
		std::scoped_lock lock{ mtx };
		m_runnableQueue.push_front(&worker);
	}
}

// Main scheduler loop.
//
void Scheduler::Main(Worker& worker)
{
	//{
	//	static std::mutex mtx;
	//	std::scoped_lock lock{ mtx };
	//	m_idleQueue.push(&worker);
	//}

	// Are there some tasks to execute?
	//
	for (;;)
	{
		worker.Sleep(); // Waiting for work.

		// std::cout << "Starting worker " << worker.ID() << "\n";

		if (!m_tasksQueue.empty())
		{
			Task t = m_tasksQueue.front();
			m_tasksQueue.pop_front();

			// m_runnableQueue.push_front(&worker);

			try
			{
				t.function();
			}
			catch (std::exception& ex)
			{
				std::cout << ex.what() << "\n";
			}

			// m_runnableQueue.pop_front();
		}
	}
}

void Scheduler::Yielddd(Worker& worker)
{
	Worker* w = m_runnableQueue.front();
	m_runnableQueue.pop_front();
	m_runnableQueue.push_back(w);

	// Switch to next worker.
	//
	Worker* next = m_runnableQueue.front();

	if (w != next)
	{
		// It would be good to do this atomically.
		//
		std::cout << "CPU " << m_cpu.m_id << ": Yielding worker " << w->ID() << "\n";
		std::cout << "CPU " << m_cpu.m_id << ": Waking worker   " << next->ID() << "\n";
		next->WakeUp();
		w->Sleep();
	}
}

Task::Task()
{
	function = []()
		{
			std::vector<int> v;
			for (int i = 0; i < 1000; ++i)
				v.push_back(rand());

			int i = 0;
			while (true)
			{
				if (v[rand() % v.size()] == rand() % v.size() && i++ % 100 == 0)
				{
					tls_worker->Yielddd();
				}
			}

			return;
		};
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

	CPUs cpus;
	cpus.Init();

	cpus.Execute();
}
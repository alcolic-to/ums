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

#include "cos.h"
//
//B b;
//A g_a{ b };

constexpr int workersPerCPU = 3;

class Scheduler;
class Worker;

const thread_local Worker* tls_worker;

class Scheduler final
{
public:
	Scheduler() = default;

	//void RunNext(Worker& current, Worker& next)
	//{
	//	next.WakeUp();
	//	current.Sleep();
	//}

	void Yielddd()
	{

	}

public:

	std::queue<Worker*> m_runnableQueue;
	std::queue<Worker*> m_idleQueue;
};


// TODO: Check whether worker should be placed on std::hardware_constructive_interference_size alignment,
// to avoid false sharing.
//
class Worker final
{
public:

	enum StateE : int { IDLE, RUNNING };

	Worker(uint64_t id, uint64_t cpu_id, uint64_t afinityMask, Scheduler& scheduler)
		: m_id{ id }
		, m_cpu_id{ cpu_id }
		, m_afinityMask{ afinityMask }
		, m_state{ IDLE }
		, m_thread{ &Worker::EntryPoint, this }
		, m_sync{}
		, m_mtx{}
		, m_scheduler{ scheduler }
	{
	}

	~Worker()
	{
		if (m_thread.joinable())
			m_thread.join();
	};

	void EntryPoint()
	{
		tls_worker = this;
		SetThreadAffinityMask(GetCurrentThread(), m_afinityMask);

		{
			static std::mutex io_mtx;
			std::scoped_lock<std::mutex> lock(io_mtx);

			std::cout << "Started thread: " << m_id << " on CPU " << m_cpu_id << std::endl;
		}

		Yielddd();
	}

	void Yielddd()
	{
		// m_scheduler.Yielddd();
		m_scheduler.Yielddd();
	}

	void WakeUp()
	{
		// m_sync.notify_one();
	}

	void Sleep()
	{
		// std::unique_lock<std::mutex> lock(m_mtx);
		// m_sync.wait(lock);
	}

	constexpr uint64_t ID() const { return m_id; }
	constexpr uint64_t CpuID() const { return m_cpu_id; }
	constexpr uint64_t AfinityMask() const { return m_afinityMask; }
	constexpr StateE State() const { return m_state; }

private:

	// TODO: reorganize data members for quick access.
	//
	uint64_t m_id;
	uint64_t m_cpu_id;
	uint64_t m_afinityMask;
	StateE m_state;

	std::thread m_thread;
	std::condition_variable m_sync;
	std::mutex m_mtx;

	Scheduler& m_scheduler;
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

	void InitWorkers()
	{
		for (int i = 0; i < workersPerCPU; ++i)
		{
			m_workers.push_back(std::make_unique<Worker>(i, m_id, m_afinityMask, m_scheduler));
			m_scheduler.m_runnableQueue.push(m_workers.back().get());
		}
	}

private:
	uint64_t m_id;
	uint64_t m_afinityMask;

	Scheduler m_scheduler;

	// TODO: Create workers in place next to each other.
	//
	std::vector<std::unique_ptr<Worker>> m_workers;
};

class CPUs final
{
public:
	CPUs()
		: m_count{GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)}
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
		for (uint64_t i = 0; availProcMask != 0; ++i, availProcMask >>= 1)
			if (availProcMask & 1)
				m_cpus.emplace_back(std::make_unique<CPU>(i, 1 << i));
	}

	// Start workers on every available CPU.
	//
	void Init()
	{
		for (auto& it : m_cpus)
		{
			it->InitWorkers();
		}
	}

private:
	uint64_t m_count;
	uint64_t m_availProcMask;
	std::vector<std::unique_ptr<CPU>> m_cpus;
};

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
}
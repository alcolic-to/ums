#include <cstdint>
#include <cassert>
#include <memory>

// Random
#include <random>

std::random_device rd;     // only used once to initialise (seed) engine
std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)

#include "os_specific.h"
#include "cos.h"

using namespace std::chrono_literals;

constexpr uint64_t FS_AllowedCPUs = 0b00001111;
constexpr int workersPerCPU = 256;
constexpr int tasksPerCPU = 40;
constexpr auto workerSleepTime = 1ms;

thread_local Worker* tls_worker;

CPUs cpus;
TaskManager taskManager{ cpus };

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
	// int funcDur = uni(rng);
	int funcDur = 1;

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
	, m_load{ 0 }
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

CPU& CPUs::MinLoadCPU() const
{
	constexpr auto cmp = [](const std::unique_ptr<CPU>& left, const std::unique_ptr<CPU>& right) { return left->Load() < right->Load(); };
	return **std::min_element(m_cpus.begin(), m_cpus.end(), cmp);
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
	// TODO: Check whether we should lock here.
	//
	// std::scoped_lock<std::mutex> lock{ m_tasksMtx };
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

	// Take lock on mutex before calling sync to ensure that other worker won't wake up immediatelly when notified,
	// but only when we call wait on this thread.
	//
	std::unique_lock<std::mutex> lock{ m_scheduler.m_workersMtx };
	if ((this->*sync)())
		m_cv.wait(lock);
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

void TaskManager::ExecuteTask(Task task)
{
	CPU& bestCPU = m_cpus.MinLoadCPU();
	bestCPU.ExecuteTask(task);
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

class A
{
public:
	A(int& i)
		: m_i{ i }
	{
	}

	int& m_i;
};

std::string strfunc()
{
	return std::string("Op");
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

	for (int i = 0; i < 100000; ++i)
		taskManager.ExecuteTask(Task{ f1 });
}

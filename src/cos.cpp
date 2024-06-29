#include <cstdint>
#include <cassert>
#include <memory>

#include <iostream>
#include <chrono>

#include "os_specific.h"
#include "cos.h"

using namespace std::chrono_literals;

constexpr uint64_t CFG_allowed_cpus = 0b0000'1111;
constexpr int CFG_workers_per_cpu = 16;
constexpr auto CFG_idle_sleep = 1ms;

CPUs::CPUs()
	: m_system_cpus_count{ cpus_count() }
	, m_avail_cpus_mask{ cpus_avail_mask() }
{
	m_avail_cpus_mask &= CFG_allowed_cpus;

	// Create new CPU for each bit available in available CPUs mask.
	//
	for (uint64_t cpu_id = 0, cpus_mask = m_avail_cpus_mask; cpus_mask != 0; ++cpu_id, cpus_mask >>= 1)
		if (cpus_mask & 1)
			m_cpus.emplace_back(std::make_unique<CPU>(cpu_id, 1 << cpu_id));
}

CPU& CPUs::min_load_cpu() const
{
	constexpr auto cmp = [](const std::unique_ptr<CPU>& left, const std::unique_ptr<CPU>& right) { return left->load() < right->load(); };
	return **std::min_element(m_cpus.begin(), m_cpus.end(), cmp);
}

CPU::CPU(uint64_t cpu_id, uint64_t cpu_mask)
	: m_id{ cpu_id }
	, m_mask{ cpu_mask }
	, m_load{ 0 }
	, m_scheduler{ *this }
	, m_workers{}
{
	for (int i = 0; i < CFG_workers_per_cpu; ++i)
		m_workers.push_back(std::make_unique<Worker>(i, *this));

	m_scheduler.start();
}

void CPU::worker_entry_point(Worker& worker) const
{
	bind_thread();

	std::cout << "Started thread: " << worker.id() << " on CPU " << worker.cpu().m_id << std::endl;

	worker.main_loop();

	std::cout << "Ended thread: " << worker.id() << " on CPU " << worker.cpu().m_id << std::endl;
}

void CPU::execute_task(const Task& task)
{
	m_scheduler.enqueue_task(task);
}

void CPU::inc_load() { ++m_load; }
void CPU::dec_load() { --m_load; }
uint64_t CPU::load() const { return m_load + m_scheduler.m_tasks.size(); }

Scheduler::Scheduler(const CPU& cpu)
	: m_cpu{ cpu }
	, m_worker{ nullptr }
	, m_state{ State::initializing }
{ }

// start scheduler.
//
void Scheduler::start()
{
	m_state = State::running;

	idle_looping();
	m_worker->m_cv.notify_one();
}

void Scheduler::enqueue_task(const Task& task)
{
	std::scoped_lock<std::mutex> lock{ m_tasks_mtx };
	m_tasks.push_back(task);
}

bool Scheduler::has_idle_workers() const { return !m_idle_queue.empty(); }
bool Scheduler::has_runnable_workers() const { return !m_runnable_queue.empty(); }

void Scheduler::save_runnable()
{
	m_runnable_queue.push_back(m_worker);

	m_worker->set_state(Worker::State::runnable);
	m_worker = nullptr;
}

void Scheduler::save_idle()
{
	m_idle_queue.push_back(m_worker);

	m_worker->set_state(Worker::State::idle);
	m_worker = nullptr;
}

bool Scheduler::has_tasks() const
{
	return !m_tasks.empty();
}

void Scheduler::prepare_worker()
{
	m_worker = m_runnable_queue.front();
	m_runnable_queue.pop_front();

	m_worker->set_state(Worker::State::running);
}

void Scheduler::idle_looping()
{
	m_worker = m_idle_queue.back();
	m_idle_queue.pop_back();

	m_worker->set_state(Worker::State::idle_looping);
}

Task Scheduler::next_task()
{
	std::scoped_lock<std::mutex> lock{ m_tasks_mtx };
	Task t = m_tasks.front();
	m_tasks.pop_front();

	return t;
}

void Scheduler::schedule_idle_worker()
{
	Worker* worker = m_idle_queue.front();
	m_idle_queue.pop_front();

	worker->m_task = next_task();
	worker->set_state(Worker::State::runnable);
	m_runnable_queue.push_back(worker);
}

void Scheduler::schedule()
{
	while (has_tasks() && has_idle_workers())
		schedule_idle_worker();
}

bool Scheduler::initializing() const { return m_state == State::initializing; }
bool Scheduler::exiting() const { return m_state == State::exiting; }
void Scheduler::set_state(State state) { m_state = state; }
Worker* Scheduler::worker() const { return m_worker; }

void Scheduler::exit_workers() const
{
	for (Worker* worker : m_idle_queue)
	{
		worker->set_state(Worker::State::exiting);
		worker->m_cv.notify_one();
	}
}

// Creates worker object and starts worker thread on a provided CPU.
//
Worker::Worker(uint64_t id, CPU& cpu)
	: m_id{ id }
	, m_state{ State::initializing }
	, m_cv{}
	, m_cpu{ cpu }
	, m_task{}
	, m_scheduler{ cpu.m_scheduler }
	, m_thread{ &CPU::worker_entry_point, &cpu, std::ref(*this) }
{
	// Wait for a signal from created thread, so we can continue when it is ready.
	//
	std::unique_lock<std::mutex> lock{ m_scheduler.m_workers_mtx };
	m_cv.wait(lock);
}

Worker::~Worker()
{
	m_scheduler.set_state(Scheduler::State::exiting);

	if (m_thread.joinable())
		m_thread.join();
}

void Worker::set_state(Worker::State state)
{
	if (state_idle(m_state) && state_runnable(state))
		m_cpu.inc_load();
	else if (state_runnable(m_state) && state_idle(state))
		m_cpu.dec_load();

	m_state = state;
}

bool Worker::exiting() const
{
	return m_state == State::exiting;
}

// Yields current worker and wakes up next worker for execution.
//
void Worker::yield()
{
	m_scheduler.sync<SyncCtx::yield>(*this);
}

// Synchronization point for the workers in yield.
//
bool Scheduler::sync_yield(Worker& worker)
{
	save_runnable(); // Push current worker at the end of runnable queue.
	schedule();

	prepare_worker();

	if (m_worker != &worker)
	{
		std::cout << "CPU " << m_cpu.m_id << ": Yielding worker " << worker.id() << "\n";
		std::cout << "CPU " << m_cpu.m_id << ": Waking worker   " << m_worker->id() << "\n";
		m_worker->m_cv.notify_one(); // wake up next.
		return true; // go to sleep
	}
	else
		return false; // continue with execution.
}

// Synchronization point for the workers.
//
bool Scheduler::sync_main(Worker& worker)
{
	// Park all new threads before scheduler is initialized.
	// Notify thread that created us to continue.
	//
	if (initializing())
	{
		save_idle();
		worker.m_cv.notify_one();
		return true; // go to sleep.
	}

	save_idle();
	schedule();

	if (has_runnable_workers())
	{
		prepare_worker();

		if (m_worker != &worker)
		{
			std::cout << "CPU " << m_cpu.m_id << ": Yielding worker " << worker.id() << "\n";
			std::cout << "CPU " << m_cpu.m_id << ": Waking worker   " << m_worker->id() << "\n";
			m_worker->m_cv.notify_one(); // wake up next.
			return true; // go to sleep
		}
	}
	else if (exiting())
		exit_workers();
	else
		idle_looping();

	return false; // continue with execution.
}

// Returns pointer to scheduler's sync member function based on provided sync context.
//
template<SyncCtx ctx>
constexpr auto sync_func()
{
	if      constexpr (ctx == SyncCtx::main)  return &Scheduler::sync_main;
	else if constexpr (ctx == SyncCtx::yield) return &Scheduler::sync_yield;
}

// Synchronization point for the workers.
// We will call sync function based on provided sync type and go to sleep if sync function returns true.
// sync function will wake up new worker if needed.
// Notes:
// There is a single mutex on scheduler used for workers synchronization and every worker has it's own condition variable.
// In order to atomically suspend single worker thread (go to sleep by calling wait) and wake up next,
// we will take lock on mutex before notifying another thread to wake up. Condition_variable::wait function
// guarantees that it will unlock mutex and go to sleep atomically and it also guarantees that it will take lock on mutex
// when wait is done. So when we notify another thread to wake up we are already holding lock on mutex
// (and notified thread can not wake up until we release lock) and mutex will be unlocked only when we call wait on this thread,
// which will release lock and wake another thread.
//
template<SyncCtx ctx>
void Scheduler::sync(Worker& worker)
{
	std::unique_lock<std::mutex> lock{ m_workers_mtx };
	if ((this->*sync_func<ctx>())(worker))
		worker.m_cv.wait(lock);
}

bool Worker::idle_loop() const
{
	if (m_state == State::idle_looping)
	{
		std::cout << "CPU " << m_cpu.m_id << ": idle looping worker " << id() << "\n";
		std::this_thread::sleep_for(CFG_idle_sleep);
		return true;
	}
	else
		return false;
}

// Main worker loop.
//
void Worker::main_loop()
{
	tls_worker = m_scheduler.m_worker = this;

	while (true)
	{
		m_scheduler.sync<SyncCtx::main>(*this);

		if (exiting())
			return;

		if (idle_loop())
			continue;

		try
		{
			m_task();
			std::cout << "CPU " << m_cpu.m_id << ": worker id " << id() << " task done.\n";
		}
		catch (std::exception& ex)
		{
			std::cout << ex.what() << "\n";
		}
	}
}

void Task_manager::execute_task(Task task)
{
	CPU& best_cpu = m_cpus.min_load_cpu();
	best_cpu.execute_task(task);
}

CPUs cpus;
Task_manager task_manager{ cpus };
thread_local Worker* tls_worker;

#include "scheduler.h"
#include "cpu.h"
#include "config.h"
#include "task_manager.h"

Scheduler::Scheduler(const CPU& cpu)
	: m_cpu{ cpu }
	, m_worker{ nullptr }
	, m_state{ State::initializing }
	, m_workers_started{ false }
	, m_load{ 0 }
{
	for (int i = 0; i < CFG_workers_per_cpu; ++i)
		m_workers.push_back(std::make_unique<Worker>(i, *this));

	m_state = State::running;

	m_worker = m_idle_queue.front();
	m_worker->m_cv.notify_one();
}

bool Scheduler::has_idle_workers() const { return !m_idle_queue.empty(); }
bool Scheduler::has_runnable_workers() const { return !m_runnable_queue.empty(); }
bool Scheduler::has_waiting_workers() const { return !m_waiting_queue.empty(); }

void Scheduler::save_runnable(Worker* worker)
{
	m_runnable_queue.push_back(worker);
	worker->set_state(Worker::State::runnable);
}

template<bool back>
void Scheduler::save_idle(Worker* worker)
{
	if constexpr (back)
		m_idle_queue.push_back(worker);
	else
		m_idle_queue.push_front(worker);

	worker->set_state(Worker::State::idle);
}

void Scheduler::save_waiting(Worker* worker)
{
	m_waiting_queue.push_back(worker);
	worker->set_state(Worker::State::waiting);
}

void Scheduler::prepare_next_worker()
{
	m_worker = m_runnable_queue.front();
	m_runnable_queue.pop_front();

	m_worker->set_state(Worker::State::running);
}

void Scheduler::enqueue_task(std::shared_ptr<Task> task)
{
	std::scoped_lock<std::mutex> lock{ m_tasks_mtx };
	m_tasks.push_back(task);
}

std::shared_ptr<Task> Scheduler::next_task()
{
	std::scoped_lock<std::mutex> lock{ m_tasks_mtx };
	std::shared_ptr<Task> t = m_tasks.front();
	m_tasks.pop_front();

	return t;
}

bool Scheduler::has_tasks()
{
	std::scoped_lock<std::mutex> lock{ m_tasks_mtx };
	return !m_tasks.empty();
}

void Scheduler::schedule_idle_worker()
{
	Worker* worker = m_idle_queue.front();
	m_idle_queue.pop_front();

	worker->m_task = next_task();
	worker->set_state(Worker::State::runnable);
	m_runnable_queue.push_back(worker);
}

void Scheduler::schedule_idle_workers()
{
	while (has_tasks() && has_idle_workers())
		schedule_idle_worker();
}

// Moves workers from waiting to runnable queue if worker's event is signaled.
//
void Scheduler::schedule_waiting_workers()
{
	auto it = m_waiting_queue.begin();
	while (it != m_waiting_queue.end())
	{
		Worker* worker = *it;

		if (worker->m_event->m_cond)
		{
			m_waiting_queue.erase(it++);
			worker->m_event = nullptr;
			worker->set_state(Worker::State::runnable);
			m_runnable_queue.push_back(worker);
		}
		else
			++it;
	}
}

void Scheduler::schedule_workers()
{
	schedule_waiting_workers();
	schedule_idle_workers();

	// Idle loop if there is no work.
	//
	while (!has_runnable_workers() && !should_exit())
	{
		idle_sleep();

		schedule_waiting_workers();
		schedule_idle_workers();
	}

	if (should_exit())
		exit_workers();
	else
		prepare_next_worker();
}

void Scheduler::idle_sleep()
{
	// TODO: Check what should be done here.
	// TODO: Release CPU (sleep) only when our time slice expires.
	// TODO: Check why there is a problem with sync tasks execution
	// if there is a sleep after task is done.
	//

	// tatic int c = 0;
	// onstexpr int sleep_cycle = 1 << 20;
	// 
	// f ((++c & (sleep_cycle-1)) == 0)
	// 
	// 	std::cout << "Zzzz...\n";
	// 	std::this_thread::sleep_for(CFG_idle_sleep);
	// 	c = 0;
	// 

	// auto start = std::chrono::high_resolution_clock::now();
	// 
	// std::this_thread::sleep_for(CFG_idle_sleep);
	// 
	// auto end = std::chrono::high_resolution_clock::now();
	// 
	// std::chrono::duration<double, std::milli> duration = end - start;
	// std::cout << "Sleep duration: " << duration.count() << "ms.\n";

	// auto start = std::chrono::high_resolution_clock::now();
	// 
	// std::unique_lock lock{ m_workers_mtx };
	// m_worker->wait(lock);
	// 
	// auto end = std::chrono::high_resolution_clock::now();
	// 
	// std::chrono::duration<double, std::milli> duration = end - start;
	// std::cout << "Sleep duration: " << duration.count() << "ms.\n";
}

bool Scheduler::initializing() const { return m_state == State::initializing; }
bool Scheduler::exiting() const { return m_state == State::exiting; }
void Scheduler::set_state(State state) { m_state = state; }
Worker* Scheduler::worker() const { return m_worker; }

void Scheduler::inc_load() { ++m_load; }
void Scheduler::dec_load() { --m_load; }
uint64_t Scheduler::load() const { return m_load + m_tasks.size(); }

void Scheduler::manage_load(Worker::State prevState, Worker::State newState)
{
	if (Worker::state_idle(prevState) && Worker::state_runnable(newState))
		inc_load();
	else if (Worker::state_runnable(prevState) && Worker::state_idle(newState))
		dec_load();
}

// Switches thread execution context from previous worker to current.
//
// Notes:
// There is a single mutex on scheduler used for workers synchronization and every worker has it's own condition variable.
// In order to atomically suspend single worker thread (go to sleep by calling wait) and wake up next,
// we will take lock on mutex before notifying another thread to wake up. Condition_variable::wait function
// guarantees that it will unlock mutex and go to sleep atomically and it also guarantees that it will take lock on mutex
// when wait is done. So when we notify another thread to wake up we are already holding lock on mutex
// (and notified thread can not wake up until we release lock) so mutex will be unlocked only when we call wait on this thread,
// which will release lock and wake another thread.
//
void Scheduler::context_switch(Worker* prev_worker)
{
	std::unique_lock<std::mutex> lock{ m_workers_mtx };
	m_worker->notify();
	prev_worker->wait(lock);
}

// Synchronization point for the workers for wait event.
//
bool Scheduler::sync_wait_event(Worker* worker)
{
	save_waiting(worker);
	return true; // proceed with scheduling.
}

// Synchronization point for the workers in yield.
//
bool Scheduler::sync_yield(Worker* worker)
{
	save_runnable(worker);
	return true; // proceed with scheduling.
}

// Synchronization point for the workers in main worker loop.
//
bool Scheduler::sync_main(Worker* worker)
{
	if (initializing())
	{
		save_idle<true>(worker);

		std::unique_lock<std::mutex> lock{ m_workers_mtx };
		worker->notify();   // Notify thread that created us to continue
		worker->wait(lock); // and go to sleep.

		// If we are the first started worker on scheduler we are going to schedule work;
		// otherwise our work is already scheduled, so we return false.
		//
		if (!m_workers_started)
			return m_workers_started = true; // proceed with scheduling.
		else
			return false; // skip scheduling.
	}
	else
	{
		save_idle<false>(worker);
		return true; // proceed with scheduling.
	}
}

// Returns pointer to scheduler's sync member function based on provided sync context.
//
template<SyncCtx ctx>
constexpr auto sync_func()
{
	if      constexpr (ctx == SyncCtx::main)       return &Scheduler::sync_main;
	else if constexpr (ctx == SyncCtx::yield)      return &Scheduler::sync_yield;
	else if constexpr (ctx == SyncCtx::wait_event) return &Scheduler::sync_wait_event;
}

// Synchronization point for the workers.
// We will call sync function based on provided sync type and proceed with scheduling if it returns true.
// Currently, only important work is done within Scheduler::sync_main on workers initialization.
//
template<SyncCtx ctx>
void Scheduler::sync(Worker* worker)
{
	if ((this->*sync_func<ctx>())(worker))
	{
		schedule_workers();

		if (m_worker != worker)
			context_switch(worker);
	}
}

void Scheduler::exit_workers() const
{
	for (Worker* worker : m_idle_queue)
	{
		worker->set_state(Worker::State::exiting);
		worker->m_cv.notify_one();
	}
}

bool Scheduler::should_exit()
{
	return exiting() && !has_runnable_workers() && !has_waiting_workers() && !has_tasks();
}

template void Scheduler::sync<SyncCtx::main>(Worker* worker);
template void Scheduler::sync<SyncCtx::yield>(Worker* worker);
template void Scheduler::sync<SyncCtx::wait_event>(Worker* worker);

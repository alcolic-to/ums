#include "task_manager.h"
#include "scheduler.h"
#include "cpu.h"

Task::Task() : m_state{ State::not_started } { }

Task::Task(const std::function<void()> function)
	: m_func{ function }
	, m_state{ State::not_started }
{ }

void Task::wait()
{
	std::unique_lock<std::mutex> lock{ m_mtx };
	if (m_state != Task::State::done)
		m_cv.wait(lock);
}

void Task::notify()
{
	std::unique_lock<std::mutex> lock{ m_mtx };
	m_state = State::done;
	m_cv.notify_one();
}

void Task::operator()()
{
	m_func();
}

Task_manager::Task_manager(const CPUs& cpus) : m_cpus{ cpus } { }

template<bool async>
void Task_manager::execute_task(const std::function<void()> func)
{
	std::shared_ptr<Task> task = std::make_shared<Task>(func);

	Scheduler& best_scheduler = m_cpus.min_load_scheduler();
	best_scheduler.enqueue_task(task);

	if constexpr (!async)
		task->wait();
}

// Global task manager.
//
Task_manager task_manager{ cpus };

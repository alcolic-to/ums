#include "async.h"

#include <functional>
#include <memory>
#include <utility>

#include "scheduler.h"
#include "task.h"
#include "ums.h"

// Enques task in best (minimum load) scheduler.
//
void enque_task(std::shared_ptr<Task> task)
{
    Scheduler& best_scheduler = schedulers->min_load_scheduler();
    best_scheduler.enqueue_task(std::move(task));
}

template<bool wait>
std::shared_ptr<Task> async(std::function<void()> func)
{
    const std::shared_ptr<Task> task{std::make_shared<Task>(std::move(func))};

    enque_task(task);

    if constexpr (wait)
        task->wait();

    return task;
}

template std::shared_ptr<Task> async<true>(std::function<void()> func);
template std::shared_ptr<Task> async<false>(std::function<void()> func);

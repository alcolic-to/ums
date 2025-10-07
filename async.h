#pragma once

#ifndef COS_ASYNC_H
#define COS_ASYNC_H

#include <functional>
#include <memory>
#include <type_traits>

#include "scheduler.h"
#include "task.h"
#include "ums.h"

namespace ums {

// template<class T, class... Args, class Result = TaskBase<std::invoke_result_t<T, Args...>,
// Args...>> Result async2(T&& t, Args&&... args)
// {
//     Result task{std::forward<T>(t), std::forward<Args>(args)...};
//     return task;

//     // return t(std::forward<Args>(args)...);

//     // std::function<T(Args...)> func = t;
//     // func(std::forward<Args>(args)...);
// }

// Executes function asynchronously.
// If wait is true, waits for function to finish.
//
template<class T, class... Args, class TaskType = Task<std::invoke_result_t<T, Args...>, Args...>>
std::shared_ptr<TaskType> async(T&& t, Args&&... args)
{
    auto task{std::make_shared<TaskType>(std::forward<T>(t), std::forward<Args>(args)...)};

    enque_task(task);

    // if constexpr (wait)
    //     task->wait();

    return task;
}

void enque_task(auto task)
{
    Scheduler& best_scheduler = schedulers->min_load_scheduler();
    best_scheduler.enqueue_task(std::move(std::static_pointer_cast<TaskBase>(task)));
};

// Don't look at code below to prevent brain damage...
//
// template<bool async, typename Fn, typename... Fns>
// void asyncs_helper(auto& tasks, Fn&& fn, Fns&&... fns)
// {
//     std::shared_ptr<Task> task{std::make_shared<Task>(std::forward<Fn>(fn))};
//     enque_task(task);
//     tasks.push_back(std::move(task));

//     asyncs_helper<async>(tasks, std::forward<Fns>(fns)...);
// }

// template<bool async>
// void asyncs_helper(auto& tasks)
// {
// }

// Executes functions asynchronously.
// If wait is true, waits for functions to finish.
//
// template<bool wait = false, typename... Fns>
// std::vector<std::shared_ptr<Task>> asyncs(Fns&&... fns)
// {
//     std::vector<std::shared_ptr<Task>> tasks;
//     tasks.reserve(sizeof...(Fns));

//     asyncs_helper<wait>(tasks, std::forward<Fns>(fns)...);

//     if constexpr (wait)
//         for (auto&& task : tasks)
//             task->wait();

//     return tasks;
// }

} // namespace ums

#endif // COS_ASYNC_H

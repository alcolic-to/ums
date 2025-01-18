#pragma once

#ifndef COS_ASYNC_H
#define COS_ASYNC_H

#include <functional>

#include "task.h"

// Executes function asynchronously.
// If wait is true, waits for function to finish.
//
template<bool wait = false>
std::shared_ptr<Task> async(std::function<void()> func);

void enque_task(std::shared_ptr<Task> task);

// Don't look at code below to prevent brain damage...
//
template<bool async, typename Fn, typename... Fns>
void asyncs_helper(auto& tasks, Fn&& fn, Fns&&... fns)
{
    std::shared_ptr<Task> task{std::make_shared<Task>(std::forward<Fn>(fn))};
    enque_task(task);
    tasks.push_back(std::move(task));

    asyncs_helper<async>(tasks, std::forward<Fns>(fns)...);
}

template<bool async>
void asyncs_helper(auto& tasks)
{
}

// Executes functions asynchronously.
// If wait is true, waits for functions to finish.
//
template<bool wait = false, typename... Fns>
std::vector<std::shared_ptr<Task>> asyncs(Fns&&... fns)
{
    std::vector<std::shared_ptr<Task>> tasks;
    tasks.reserve(sizeof...(Fns));

    asyncs_helper<wait>(tasks, std::forward<Fns>(fns)...);

    if constexpr (wait)
        for (auto&& task : tasks)
            task->wait();

    return tasks;
}

#endif // COS_ASYNC_H

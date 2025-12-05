/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef UMS_ASYNC_HPP
#define UMS_ASYNC_HPP

#include <memory>
#include <type_traits>

#include "scheduler.hpp"
#include "task.hpp"
#include "ums.hpp"
#include "util.hpp"

namespace ums {

/**
 * Enques task for asynchronous execution and returns task.
 * If wait is true, waits for function to finish.
 */
template<bool wait = false, class Fn, class... Args,
         class ReturnType = std::invoke_result_t<Fn, Args...>,
         class TaskExecType = TaskExec<Fn, Args...>>
Task<ReturnType> async(Fn&& t, Args&&... args)
{
    TZoneScopedC(tracy::Color::DarkGreen);

    auto task{std::make_shared<TaskExecType>(std::forward<Fn>(t), std::forward<Args>(args)...)};
    enque_task(task);

    if constexpr (wait)
        task->wait();

    return Task{std::static_pointer_cast<TaskResult<ReturnType>>(task)};
}

void enque_task(const auto& task)
{
    TZoneScopedC(tracy::Color::DarkGreen);

    Scheduler& best_scheduler = schedulers->min_load_scheduler();
    best_scheduler.enqueue_task(std::static_pointer_cast<TaskBase>(task));
};

/**
 * Don't look at code below to prevent brain damage...
 */
template<bool wait = false, typename Fn, typename... Fns>
void asyncs_helper(auto& tasks, Fn&& fn, Fns&&... fns)
{
    Task<void> task{async(std::forward<Fn>(fn))};
    tasks.push_back(std::move(task));

    asyncs_helper<wait>(tasks, std::forward<Fns>(fns)...);
}

template<bool async>
void asyncs_helper(auto& tasks)
{
}

/**
 * Executes functions asynchronously.
 * If wait is true, waits for functions to finish.
 */
template<bool wait = false, typename... Fns>
std::vector<Task<void>> asyncs(Fns&&... fns)
{
    std::vector<Task<void>> tasks;
    tasks.reserve(sizeof...(Fns));

    asyncs_helper<wait>(tasks, std::forward<Fns>(fns)...);

    if constexpr (wait)
        for (auto&& task : tasks)
            task->wait();

    return tasks;
}

} // namespace ums

#endif // UMS_ASYNC_HPP

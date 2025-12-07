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
#include "task.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

#include "spinlock.hpp"
#include "types.hpp"

namespace ums {

void Tasks::enque(std::shared_ptr<TaskBase> task)
{
    const std::scoped_lock<Spinlock> l{m_lock};

    m_tasks.push_back(std::move(task));
    m_size.fetch_add(1, std::memory_order_relaxed);
}

std::shared_ptr<TaskBase> Tasks::deque() noexcept
{
    const std::scoped_lock<Spinlock> l{m_lock};

    if (m_tasks.empty())
        return nullptr;

    std::shared_ptr<TaskBase> t{std::move(m_tasks.front())};
    m_tasks.pop_front();

    m_size.fetch_sub(1, std::memory_order_relaxed);
    return t;
}

[[nodiscard]] usize Tasks::size() const noexcept
{
    return m_size.load(std::memory_order_relaxed);
}

[[nodiscard]] bool Tasks::empty() const noexcept
{
    return size() == 0;
}

} // namespace ums

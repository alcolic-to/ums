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
#include "ums.hpp"

#include <functional>
#include <memory>
#include <utility>

#include "async.hpp"
#include "options.hpp"
#include "scheduler.hpp"

namespace ums {

// Thread local worker.
//
thread_local Worker* this_worker; // NOLINT

// Global Schedulers.
//
std::unique_ptr<Schedulers> schedulers; // NOLINT

// void init_ums(std::function<int(int, char**)>& main, int argc, char** argv)
void init_ums(std::function<void()> main, Options opt)
{
    schedulers = std::make_unique<Schedulers>(opt);

    async(std::move(main));

    schedulers->wait_exit();
    schedulers.reset();
}

} // namespace ums

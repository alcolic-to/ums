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
#include <util.hpp> // NOLINT
#include <utility>

#include "async.hpp"
#include "options.hpp"
#include "scheduler.hpp"

namespace ums {

void init_ums(std::function<void()> main, Options opt)
{
    TZoneScopedC(tracy::Color::LightBlue); // NOLINT

    sch::get() = std::make_unique<Schedulers>(opt);

    async(std::move(main));

    sch::get()->wait_exit();
    sch::get().reset();
}

} // namespace ums

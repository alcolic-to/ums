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

// NOLINTBEGIN

#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#include "async.h"
#include "types.h"
#include "ums.h"
#include "util.h"
#include "worker.h"

using namespace std::chrono_literals;
using namespace ums;

void f1()
{
    auto start = now();

    std::vector<u64> v;
    for (int i = 0; i < 1000; ++i)
        v.push_back(random<uint8_t>());

    int funcDur = random() % 11;

    int i = 0;
    while (true) {
        auto end = now();

        std::chrono::duration<double, std::milli> duration = end - start;
        if (duration.count() > funcDur) {
            std::cout << "Task execution exceeded time limit of " << duration.count() << "ms.\n";
            break;
        }

        if (v[random() % v.size()] == random<u8>() % v.size() && i++ % 100 == 0)
            this_worker->yield();
    }

    return;
}

// Duration of ~1s when plugged in.
//
u64 f3()
{
    u64 first = 0, second = 1;

    // Fibbonaci seq.
    //
    for (u64 i = 2; i < 3000000000; ++i) {
        u64 sum = first + second;
        first = second;
        second = sum;
    }

    return second;
}

void thread_function()
{
    for (int i = 0; i < 1000; ++i) {
        auto t = async(f3);
        t->wait();
    }
}

u64 fib(u64 n)
{
    u64 first = 0, second = 1;

    if (n > 0)
        std::cout << first;

    for (u64 i = 1; i < n; ++i) {
        u64 sum = first + second;
        first = second;
        second = sum;
        std::cout << " " << second;
    }

    std::cout << "\n";
    return second;
}

void ms3_function()
{
    Stopwatch s;

    for (int i = 0; i < 1000; ++i)
        async(fib, 1);
}

void ums_main(int argc, char* argv[])
{
    std::cout << "argc: " << argc << "\n";
    std::cout << "argv: \n";
    for (int i = 0; i < argc; ++i)
        std::cout << argv[0] << "\n";

    std::cout << "Schedulers count: " << schedulers->cpus_count() << "\n";
    std::cout << "Workers count: " << schedulers->workers_count() << "\n";

    Stopwatch<true, std::chrono::microseconds> s;

    Task<u64> task{async(fib, 16)};
    std::cout << task->get() << "\n";

    std::vector<Task<void>> v;
    v.emplace_back(async([] {}));

    v[0]->wait();
}

int main(int argc, char* argv[])
{
    init_ums([&] { ums_main(argc, argv); });
}

// NOLINTEND

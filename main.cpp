// NOLINTBEGIN

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "condition_variable.h"
#include "task_manager.h"
#include "ums.h"
#include "util.h"
#include "worker.h"

using namespace std::chrono_literals;

void f1()
{
    auto start = now();

    std::vector<uint64_t> v;
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

        if (v[random() % v.size()] == random<uint8_t>() % v.size() && i++ % 100 == 0)
            tls_worker->yield();
    }

    return;
}

// Duration of ~1s when plugged in.
//
uint64_t f3()
{
    uint64_t first = 0, second = 1;

    // Fibbonaci seq.
    //
    for (uint64_t i = 2; i < 3000000000; ++i) {
        uint64_t sum = first + second;
        first = second;
        second = sum;
    }

    return second;
}

void thread_function()
{
    for (int i = 0; i < 1000; ++i)
        task_manager->execute_task<false>(f3);
}

// Duration of ~4ms when plugged in.
//
uint64_t f4()
{
    uint64_t first = 0, second = 1;

    // Fibbonaci seq.
    //
    for (uint64_t i = 2; i < 10000000; ++i) {
        uint64_t sum = first + second;
        first = second;
        second = sum;
    }

    return second;
}

void ms3_function()
{
    Stopwatch s;

    for (int i = 0; i < 1000; ++i)
        task_manager->execute_task<true>(f4);
}

// int ums_main(int argc, char* argv[])
void ums_main()
{
    Stopwatch<true, std::chrono::microseconds> s;

    for (int i = 0; i < 1000; ++i) {
        auto task{task_manager->execute_task<true>(f4)};
        task->wait();
    }
}

int main(int argc, char* argv[])
{
    init_ums(ums_main);
}

// NOLINTEND

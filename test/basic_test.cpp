#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <ratio>

#include "task_manager.h"
#include "worker.h"

using namespace std::chrono_literals;

// NOLINTBEGIN

void f()
{
    tls_worker->sleep_for(10ms);
}

TEST(BasicTestSuite, BasicTest)
{
    uint64_t r = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
        task_manager.execute_task<false>(f);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Total exec time: " << duration.count() << "ms.\n";

    std::cout << r << "\n";
}

// NOLINTEND

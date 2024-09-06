#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <ratio>
#include <thread>

#include "task_manager.h"

#define ENDL '\n'

void f()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST(BasicTestSuite, BasicTest)
{
    uint64_t r = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
        task_manager.execute_task<false>(f);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Total exec time: " << duration.count() << "ms." << ENDL;

    std::cout << r << ENDL;
}

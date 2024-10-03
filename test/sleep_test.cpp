#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>

#include "task_manager.h"
#include "util.h"
#include "worker.h"

using namespace std::chrono_literals;

// NOLINTBEGIN

void sleep_test()
{
    tls_worker->sleep_for(1000ms);
}

TEST(Sleep, SimpleSleepTest)
{
    auto start = now();

    for (std::size_t i = 0; i < 10; ++i)
        task_manager.execute_task<false>(sleep_test);

    auto end = now();
    auto diff = 10000ms - (end - start);
    ASSERT_LE(std::chrono::abs(diff), 100ms);
}

// NOLINTEND

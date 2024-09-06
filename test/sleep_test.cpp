#include <gtest/gtest.h>

#include "sync_api.h"
#include "task_manager.h"
#include "util.h"

void sleep_test()
{
    cos_sleep(1000ms);
}

TEST(Sleep, SimpleSleepTest)
{
    auto start = now();

    for (std::size_t i = 0; i < 10; ++i)
        task_manager.execute_task<false>(sleep_test);

    auto end = now();
    auto diff = 10000ms - (end - start);
    ASSERT_LE(abs(diff), 100ms);
}

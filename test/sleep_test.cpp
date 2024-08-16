#include <gtest/gtest.h>

#include "sync_api.h"
#include "util.h"
#include "task_manager.h"

void sleep_test()
{
    cos_sleep(1000 /* miliseconds */);
}

TEST(Sleep, SimpleSleepTest)
{
    std::uint64_t start = get_time_in_ms();

    for (std::size_t i = 0; i < 10; ++i)
        task_manager.execute_task<false>(sleep_test);

    std::uint64_t end = get_time_in_ms();
    std::int64_t diff = 10000 - (end - start);
    ASSERT_LE(abs(diff), 100);
}

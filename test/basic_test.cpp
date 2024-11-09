#include <gtest/gtest.h>

#include "ums.h"
#include "util.h"
#include "worker.h"

using namespace std::chrono_literals;

// NOLINTBEGIN

TEST(BasicTestSuite, BasicTest)
{
    auto f = [] { tls_worker->sleep_for(10ms); };

    auto test = [&] {
        Stopwatch s;

        for (int i = 0; i < 1000; ++i)
            task_manager->execute_task<false>(f);
    };

    init_ums(test);
}

// NOLINTEND

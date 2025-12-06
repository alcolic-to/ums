#include <chrono>
#include <gtest/gtest.h>

#include "async.hpp"
#include "config.hpp"
#include "types.hpp"
#include "ums.hpp"
#include "util.hpp"
#include "worker.hpp"

using namespace ums;
using namespace std::chrono_literals;

// NOLINTBEGIN

void sleep_test(const u32 iterations, const auto sleep_time)
{
    auto test = [&] {
        auto start = now();

        for (u32 i = 0; i < iterations; ++i)
            async<true>([&] { this_worker->sleep_for(sleep_time); });

        auto duration = now() - start;
        auto expected_duration = iterations * sleep_time;

        ASSERT_LE(std::chrono::abs(expected_duration - duration), 1ms);
    };

    init_ums(test);
}

TEST(Sleep, sleep_test)
{
    for (u32 i = 1; i <= 5; ++i) {
        for (u32 sleep_ms = 1; sleep_ms <= 512; sleep_ms *= 2) {
            // Only if idle spin is allowed, we can (almost) guarantee sleeps of < 20ms.
            if (!FS_idle_spinning_allowed && sleep_ms < 16)
                continue;

            sleep_test(i, std::chrono::milliseconds{sleep_ms});
        }
    }
}

// NOLINTEND

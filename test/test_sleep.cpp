#include <chrono>
#include <gtest/gtest.h>

#include "async.hpp"
#include "scheduler.hpp"
#include "types.hpp"
#include "ums.hpp"
#include "util.hpp"
#include "worker.hpp"

using namespace ums;
using namespace std::chrono_literals;

// NOLINTBEGIN

/**
 * Sanity sleep test. Note that this might fail if OS schedule us out at any moment.
 */
TEST(Sleep, sanity_test)
{
    auto test = [&] {
        for (auto sleep_ms = 1ms; sleep_ms <= 1024ms; sleep_ms *= 2) {
            auto start = now();

            worker::sleep_for(sleep_ms);

            auto dur = now() - start;
            ASSERT_TRUE(dur >= sleep_ms && dur <= sleep_ms + 1ms);
        }
    };

    init_ums(test);
}

TEST(Sleep, sleep_test)
{
    auto test = [&] {
        for (auto sleep_ms = 1ms; sleep_ms <= 1024ms; sleep_ms *= 2) {
            std::vector<Task<void>> tasks;

            auto start = now();
            async([=] { worker::sleep_for(sleep_ms); })->wait();

            auto dur = now() - start;
            ASSERT_TRUE(dur >= sleep_ms && dur <= sleep_ms + 1ms);
        }
    };

    init_ums(test);
}

// NOLINTEND

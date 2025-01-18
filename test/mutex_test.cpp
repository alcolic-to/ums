// NOLINTBEGIN

#include <atomic>
#include <functional>
#include <gtest/gtest.h>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "async.h"
#include "condition_variable.h"
#include "mutex.h"
#include "scheduler.h"
#include "shared_mutex.h"
#include "ums.h"
#include "util.h"

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

// Condition_variable, shared mutex and timed mutex does not have standard layout because
// they all have (explicitly or implicitly) std::vector inside.

// clang-format off

STATIC_ASSERT(std::is_standard_layout_v<Spinlock>);
STATIC_ASSERT(std::is_standard_layout_v<Mutex>); // N4928 [thread.mutex.class]/3
STATIC_ASSERT(std::is_standard_layout_v<Recursive_mutex>); // N4928 [thread.mutex.recursive]/2
// STATIC_ASSERT(std::is_standard_layout_v<Timed_mutex>); // N4928 [thread.timedmutex.class]/2
// STATIC_ASSERT(std::is_standard_layout_v<Recursive_timed_mutex>); // N4928 [thread.timedmutex.recursive]/2
// STATIC_ASSERT(std::is_standard_layout_v<Shared_mutex>); // N4928 [thread.sharedmutex.class]/2
// STATIC_ASSERT(std::is_standard_layout_v<Shared_timed_mutex>); // N4928 [thread.sharedtimedmutex.class]/2
// STATIC_ASSERT(std::is_standard_layout_v<Condition_variable>); // N4928 [thread.condition.condvar]/1

// nothrow-destructibility required by N4928 [res.on.exception.handling]/3
STATIC_ASSERT(std::is_nothrow_destructible_v<Mutex>);
STATIC_ASSERT(std::is_nothrow_destructible_v<Recursive_mutex>);
STATIC_ASSERT(std::is_nothrow_destructible_v<Timed_mutex>);
STATIC_ASSERT(std::is_nothrow_destructible_v<Recursive_timed_mutex>);
STATIC_ASSERT(std::is_nothrow_destructible_v<Shared_mutex>);
STATIC_ASSERT(std::is_nothrow_destructible_v<Shared_timed_mutex>);
STATIC_ASSERT(std::is_nothrow_destructible_v<std::shared_lock<Shared_mutex>>);
STATIC_ASSERT(std::is_nothrow_destructible_v<std::shared_lock<Shared_timed_mutex>>);
STATIC_ASSERT(std::is_nothrow_destructible_v<Condition_variable>);

STATIC_ASSERT(std::is_nothrow_default_constructible_v<Mutex>); // N4928 [thread.mutex.class]
STATIC_ASSERT(std::is_nothrow_default_constructible_v<Recursive_mutex>); // strengthened
STATIC_ASSERT(std::is_nothrow_default_constructible_v<Timed_mutex>); // strengthened
STATIC_ASSERT(std::is_nothrow_default_constructible_v<Recursive_timed_mutex>); // strengthened
STATIC_ASSERT(std::is_nothrow_default_constructible_v<Shared_mutex>); // strengthened
STATIC_ASSERT(std::is_nothrow_default_constructible_v<Shared_timed_mutex>); // strengthened
STATIC_ASSERT(std::is_nothrow_default_constructible_v<std::shared_lock<Shared_mutex>>); // N4928 [thread.lock.shared.cons]/1
STATIC_ASSERT(std::is_nothrow_default_constructible_v<std::shared_lock<Shared_timed_mutex>>); // N4928 [thread.lock.shared.cons]/1
STATIC_ASSERT(std::is_nothrow_default_constructible_v<Condition_variable>); // strengthened

// gcc's libstdc++ does not align with these.
// STATIC_ASSERT(std::is_nothrow_constructible_v<std::shared_lock<Shared_mutex>, Shared_mutex&, std::adopt_lock_t>); // strengthened
// STATIC_ASSERT(std::is_nothrow_constructible_v<std::shared_lock<Shared_mutex>, Shared_mutex&, const std::adopt_lock_t&>); // strengthened
// STATIC_ASSERT(std::is_nothrow_constructible_v<std::shared_lock<Shared_timed_mutex>, Shared_timed_mutex&, std::adopt_lock_t>); // strengthened
// STATIC_ASSERT(std::is_nothrow_constructible_v<std::shared_lock<Shared_timed_mutex>, Shared_timed_mutex&, const std::adopt_lock_t&>); // strengthened

// Also test mandatory and strengthened exception specification for try_lock().
STATIC_ASSERT(noexcept(std::declval<Mutex&>().try_lock())); // strengthened
STATIC_ASSERT(noexcept(std::declval<Recursive_mutex&>().try_lock())); // N4928 [thread.mutex.recursive]
STATIC_ASSERT(noexcept(std::declval<Timed_mutex&>().try_lock())); // strengthened
STATIC_ASSERT(noexcept(std::declval<Recursive_timed_mutex&>().try_lock())); // N4928 [thread.timedmutex.recursive]
STATIC_ASSERT(noexcept(std::declval<Shared_mutex&>().try_lock())); // strengthened

// clang-format on

TEST(Condition_variable, cv_sanity_test_1)
{
    // if (schedulers->workers_count() < 2)
    //     GTEST_SKIP() << "At least 2 workers needed for this test.";

    auto test = [] {
        Mutex mutex;
        Condition_variable cv;
        bool ready = false;

        auto waiter = [&] {
            std::unique_lock<Mutex> lock(mutex);
            while (!ready)
                cv.wait(lock);

            ASSERT_TRUE(true);
        };

        auto notifier = [&] {
            std::unique_lock<Mutex> lock(mutex);
            ready = true;
            cv.notify_one();
        };

        asyncs<true>(waiter, notifier);
    };

    init_ums(test);
}

TEST(Condition_variable, cv_sanity_test_2)
{
    // if (schedulers->workers_count() < 4)
    //     GTEST_SKIP() << "At least 4 workers needed for this test.";

    auto test = [] {
        Mutex mtx;
        Condition_variable cv1, cv2, cv3;
        std::vector<std::uint8_t> v;

        auto waiter_1 = [&] {
            std::unique_lock<Mutex> l{mtx};
            cv1.wait(l);
            v.push_back(1);
        };

        auto waiter_2 = [&] {
            std::unique_lock<Mutex> l{mtx};
            cv2.wait(l);
            v.push_back(2);
        };

        auto waiter_3 = [&] {
            std::unique_lock<Mutex> l{mtx};
            cv3.wait(l);
            v.push_back(3);
        };

        auto notifier = [&] {
            this_worker->sleep_for(20ms);
            cv2.notify_one();

            this_worker->sleep_for(20ms);
            cv1.notify_one();

            this_worker->sleep_for(20ms);
            cv3.notify_one();

            // Wait for the threads to finish.
            this_worker->sleep_for(100ms);
        };

        asyncs<true>(waiter_1, waiter_2, waiter_3, notifier);

        ASSERT_TRUE(v.size() == 3);
        ASSERT_TRUE(v[0] == 2);
        ASSERT_TRUE(v[1] == 1);
        ASSERT_TRUE(v[2] == 3);
    };

    init_ums(test);
}

TEST(Condition_variable, cv_producer_consumer_test)
{
    // if (schedulers->workers_count() < 2)
    //     GTEST_SKIP() << "At least 2 workers needed for this test.";

    auto test = [] {
        Mutex mutex;
        Condition_variable cv;
        int data = 0;
        bool ready = false;

        auto producer = [&] {
            this_worker->sleep_for(100ms); // Simulate work

            {
                std::unique_lock<Mutex> lock(mutex);
                data = 42; // Set shared data
                ready = true;
            }

            cv.notify_one(); // Notify the consumer
        };

        auto consumer = [&] {
            std::unique_lock<Mutex> lock(mutex);
            cv.wait(lock, [&] { return ready; });

            ASSERT_TRUE(data == 42);
        };

        asyncs<true>(producer, consumer);
    };

    init_ums(test);
}

TEST(Condition_variable, cv_spurious_wakeup_test)
{
    // if (schedulers->workers_count() < 2)
    //     GTEST_SKIP() << "At least 2 workers needed for this test.";

    auto test = [] {
        Mutex mutex;
        Condition_variable cv;
        bool ready = false;

        auto spurious_wakeup = [&] {
            std::unique_lock<Mutex> lock(mutex);
            cv.wait(lock); // This should block until a real notification
            ASSERT_TRUE(!ready);
        };

        auto notifier = [&] {
            this_worker->sleep_for(std::chrono::milliseconds(100)); // Simulate some wait
            cv.notify_one();
        };

        asyncs<true>(spurious_wakeup, notifier);
    };

    init_ums(test);
}

// https://github.com/microsoft/STL/blob/1e312b38db8df1dfbea17adc344454feb8d00dd9/tests/std/include/test_header_units_and_modules.hpp#L150
TEST(Condition_variable, cv_complex_test_1)
{
    // if (schedulers->workers_count() < 2)
    //     GTEST_SKIP() << "At least 2 workers needed for this test.";

    auto test = [] {
        Condition_variable cv;
        Mutex mtx;
        std::vector<int> vec = {5};

        auto odd = [&] {
            std::unique_lock<Mutex> lk{mtx};

            while (vec.size() < 6) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 1; });
                const int n = vec.back();
                vec.push_back(n * 10 + 1);
                cv.notify_one();
            }
        };

        auto even = [&] {
            std::unique_lock<Mutex> lk{mtx};

            while (vec.size() < 7) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 0; });
                const int n = vec.back();
                vec.push_back(n * 10 + 2);
                cv.notify_one();
            }
        };

        asyncs<true>(odd, even);

        const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
        ASSERT_TRUE(vec == expected_val);

        static_assert(static_cast<int>(std::cv_status::no_timeout) == 0);
        static_assert(static_cast<int>(std::cv_status::timeout) == 1);
    };

    init_ums(test);
}

TEST(Condition_variable, cv_complex_test_2)
{
    // if (schedulers->workers_count() < 9)
    //     GTEST_SKIP() << "At least 9 workers needed for this test.";

    auto test = [] {
        Mutex mutex;
        Condition_variable increment_cv;
        Condition_variable notifier_cv;
        bool ready = false;

        std::atomic<uint32_t> atomic_counter = 0;
        uint32_t threads_count = 8;

        auto increment = [&] {
            // First round of increments.
            //
            uint32_t r = ++atomic_counter;
            ASSERT_TRUE(r > 0 && r <= threads_count);

            std::unique_lock<Mutex> lock{mutex};

            // Waking up notifier when all threads incremented counter.
            //
            if (r == threads_count)
                notifier_cv.notify_one();

            increment_cv.wait(lock, [&] { return ready; });

            // Second round of increments.
            //
            r = ++atomic_counter;
            ASSERT_TRUE(r > threads_count && r <= 2 * threads_count);
        };

        auto notifier = [&] {
            std::unique_lock<Mutex> lock{mutex};
            notifier_cv.wait(lock, [&] { return atomic_counter.load() == threads_count; });

            this_worker->sleep_for(100ms);
            ready = true;
            increment_cv.notify_all();
        };

        asyncs<true>(increment, increment, increment, increment, increment, increment, increment,
                     increment, notifier);

        ASSERT_TRUE(atomic_counter.load() == 2 * threads_count);
    };

    init_ums(test);
}

// Mext section is slightly modified stl mutex tests:
// https://github.com/microsoft/STL/blob/1e312b38db8df1dfbea17adc344454feb8d00dd9/tests/std/tests/VSO_0226079_mutex/test.cpp

class Event {
public:
    Event() = default;

    void signal()
    {
        std::unique_lock<Mutex> lock(mtx);
        is_set = true;
        cv.notify_all();
    }

    void wait()
    {
        std::unique_lock<Mutex> lock(mtx);
        cv.wait(lock, [this]() { return is_set; });
        is_set = false; // reset after wait
    }

private:
    Mutex mtx;
    Condition_variable cv;
    bool is_set = false;
};

template<typename _Mutex>
class other_mutex_thread {
public:
    enum class message { idle, shutdown, lock, tryLock, unlock, unlockDelayed };

    explicit other_mutex_thread(_Mutex& targetMtx)
        : mtx(targetMtx)
        , currentMessage(message::idle)
        , tryLockSuccess(false)
    {
        async([&] { thread_func(); });
    }

    void join() { send_message(message::shutdown); }

    void lock() { send_message(message::lock); }

    bool try_lock()
    {
        send_message(message::tryLock);
        return tryLockSuccess;
    }

    void unlock() { send_message(message::unlock); }

    void unlock_delayed()
    {
        currentMessage = message::unlockDelayed;
        backgroundEvent.signal();
    }

    void finish_delayed() { foregroundEvent.wait(); }

private:
    void thread_func()
    {
        for (;;) {
            backgroundEvent.wait();
            const message sentMessage = std::exchange(currentMessage, message::idle);
            switch (sentMessage) {
            case message::lock:
                mtx.lock();
                break;
            case message::tryLock:
                tryLockSuccess = mtx.try_lock();
                break;
            case message::unlockDelayed:
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                mtx.unlock();
                break;
            case message::unlock:
                mtx.unlock();
                break;
            case message::shutdown:
                // nothing to do
                break;
            case message::idle:
            default:
                ASSERT_TRUE(!"Bad message received on background thread");
            }

            foregroundEvent.signal();
            if (sentMessage == message::shutdown)
                return;
        }
    }

    void send_message(const message msg)
    {
        currentMessage = msg;
        backgroundEvent.signal();
        foregroundEvent.wait();
    }

    _Mutex& mtx;
    Event backgroundEvent;
    Event foregroundEvent;
    message currentMessage;
    bool tryLockSuccess;
};

template<typename Func>
std::chrono::nanoseconds time_execution(Func&& f)
{
    const auto startTime = std::chrono::steady_clock::now();
    std::forward<Func>(f)();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() -
                                                                startTime);
}

struct throwing_mutex_threw : std::exception {
    using std::exception::exception;
};

struct throwing_mutex {
    void lock() { throw throwing_mutex_threw(); }

    bool try_lock() { throw throwing_mutex_threw(); }

    void unlock() {}
};

template<typename _Mutex>
struct mutex_test_fixture {
    mutex_test_fixture() : mtx(), mtx2(), ot(mtx) {}

    ~mutex_test_fixture() { ot.join(); }

    void test_lockable()
    {
        mtx.lock();
        mtx.unlock();
        ASSERT_TRUE(mtx.try_lock());
        mtx.unlock();

        ot.lock();
        ASSERT_TRUE(!mtx.try_lock());
        ot.unlock();

        test_guard<std::lock_guard<_Mutex>>();
        test_guard<std::unique_lock<_Mutex>>();

        { // unique_lock constructor, move constructor, move assignment
            std::unique_lock<_Mutex> ulOuter;
            STATIC_ASSERT(noexcept(std::unique_lock<_Mutex>()));
            ASSERT_TRUE(!ulOuter.owns_lock());
            ASSERT_TRUE(!ulOuter);
            ASSERT_TRUE(ulOuter.mutex() == nullptr);

            {
                std::unique_lock<_Mutex> ul(mtx);
                ASSERT_TRUE(!ot.try_lock());
                ASSERT_TRUE(ul.owns_lock());
                ASSERT_TRUE(static_cast<bool>(ul));
                ASSERT_TRUE(ul.mutex() == &mtx);

                std::unique_lock<_Mutex> ulMoveConstructed(std::move(ul));
                ASSERT_TRUE(!ul);
                ASSERT_TRUE(ul.mutex() == nullptr);
                ASSERT_TRUE(static_cast<bool>(ulMoveConstructed));
                ASSERT_TRUE(ulMoveConstructed.mutex() == &mtx);

                ulOuter = std::move(ulMoveConstructed);
                ASSERT_TRUE(!ulMoveConstructed);
            }

            ASSERT_TRUE(static_cast<bool>(ulOuter));
        }

        { // unique_lock defer
            STATIC_ASSERT(noexcept(std::unique_lock<_Mutex>(mtx, std::defer_lock)));
            std::unique_lock<_Mutex> ul(mtx, std::defer_lock);
            ASSERT_TRUE(!ul);
            ASSERT_TRUE(ot.try_lock());
            ot.unlock();
        }

        { // unique_lock try_to_lock success
            std::unique_lock<_Mutex> ul(mtx, std::try_to_lock);
            ASSERT_TRUE(static_cast<bool>(ul));
            ASSERT_TRUE(!ot.try_lock());
        }

        ASSERT_TRUE(ot.try_lock());

        { // unique_lock try_to_lock failure
            std::unique_lock<_Mutex> ul(mtx, std::try_to_lock);
            ASSERT_TRUE(!ul);
        }

        ot.unlock();

        { // swap with empty object
            std::unique_lock<_Mutex> ulLeft(mtx);
            std::unique_lock<_Mutex> ulRight;
            ASSERT_TRUE(static_cast<bool>(ulLeft));
            ASSERT_TRUE(!ulRight);
            std::swap(ulLeft, ulRight);
            ASSERT_TRUE(!ulLeft);
            ASSERT_TRUE(static_cast<bool>(ulRight));
        }

        { // swap objects
            std::unique_lock<_Mutex> ulLeft(mtx);
            std::unique_lock<_Mutex> ulRight(mtx2);
            ASSERT_TRUE(ulLeft.mutex() == &mtx);
            ASSERT_TRUE(ulRight.mutex() == &mtx2);
            STATIC_ASSERT(noexcept(std::swap(ulLeft, ulRight)));
            std::swap(ulLeft, ulRight);
            ASSERT_TRUE(ulLeft.mutex() == &mtx2);
            ASSERT_TRUE(ulRight.mutex() == &mtx);
            STATIC_ASSERT(noexcept(ulLeft.swap(ulRight)));
            ulLeft.swap(ulRight);
            ASSERT_TRUE(ulLeft.mutex() == &mtx);
            ASSERT_TRUE(ulRight.mutex() == &mtx2);
        }

        { // lock/try_lock/unlock
            std::unique_lock<_Mutex> ul(mtx);
            ASSERT_TRUE(!ot.try_lock());
            ul.unlock();
            ASSERT_TRUE(ot.try_lock());
            ASSERT_TRUE(!ul.try_lock());
            ot.unlock();
            ASSERT_TRUE(ul.try_lock());
            ul.unlock();
            ul.lock();
        }

        { // release
            std::unique_lock<_Mutex> ul(mtx);
            ASSERT_TRUE(&mtx == ul.release());
        }

        ASSERT_TRUE(!ot.try_lock());
        mtx.unlock();
    }

    template<typename Guard>
    void test_guard()
    {
        {
            Guard lg(mtx);
            ASSERT_TRUE(!ot.try_lock());
        }

        {
            mtx.lock();
            Guard lg(mtx, std::adopt_lock);
            ASSERT_TRUE(!ot.try_lock());
        }

        ASSERT_TRUE(ot.try_lock());
        ot.unlock();
    }

    void test_timed_lockable()
    {
        // Test acquiring locks successfully
        ASSERT_TRUE(time_execution([this] {
                        ASSERT_TRUE(mtx.try_lock_for(std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        mtx.unlock();
        ASSERT_TRUE(time_execution([this] {
                        ASSERT_TRUE(mtx.try_lock_until(std::chrono::steady_clock::now() +
                                                       std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        mtx.unlock();
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<_Mutex> ul(mtx, std::defer_lock);
                        ASSERT_TRUE(ul.try_lock_for(std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<_Mutex> ul(mtx, std::chrono::hours(24));
                        ASSERT_TRUE(ul.owns_lock());
                    }) < std::chrono::hours(1));
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<_Mutex> ul(mtx, std::defer_lock);
                        ASSERT_TRUE(ul.try_lock_until(std::chrono::steady_clock::now() +
                                                      std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<_Mutex> ul(mtx, std::chrono::steady_clock::now() +
                                                             std::chrono::hours(24));
                        ASSERT_TRUE(ul.owns_lock());
                    }) < std::chrono::hours(1));
    }

    void test_recursive_lockable()
    {
        // Lock recursively
        mtx.lock();
        mtx.lock();
        mtx.unlock();
        mtx.unlock();

        ASSERT_TRUE(mtx.try_lock());
        ASSERT_TRUE(mtx.try_lock());
        mtx.unlock();
        mtx.unlock();
    }

    void test_shared_lockable()
    {
        mtx.lock();
        ASSERT_TRUE(!mtx2.try_lock());
        ot.unlock();

        mtx2.lock_shared();
        mtx2.lock_shared();
        mtx2.unlock_shared();
        mtx2.unlock_shared();

        ot.lock();
        ASSERT_TRUE(!mtx.try_lock());
        ot.unlock();
    }

    _Mutex mtx;
    _Mutex mtx2;
    other_mutex_thread<_Mutex> ot;
};

void test_nonmember_lock()
{
    Mutex mtx;
    other_mutex_thread<Mutex> ot(mtx);

    Recursive_mutex rMtx;
    Timed_mutex tMtx;

    Recursive_timed_mutex rtMtx;
    other_mutex_thread<Recursive_timed_mutex> rtOt(rtMtx);

    std::lock(mtx, rMtx, tMtx, rtMtx);
    mtx.unlock();
    rMtx.unlock();
    tMtx.unlock();
    rtMtx.unlock();

    ot.lock();
    rtOt.lock();
    ot.unlock_delayed();   // no timing assumptions: lock() retries until it
    rtOt.unlock_delayed(); // can lock all input mutexes
    std::lock(rtMtx, tMtx, mtx, rMtx);
    mtx.unlock();
    rMtx.unlock();
    tMtx.unlock();
    rtMtx.unlock();
    ot.finish_delayed();
    rtOt.finish_delayed();

    throwing_mutex throwing;
    try {
        std::lock(rtMtx, mtx, throwing); // throws
        abort();
    }
    catch (const throwing_mutex_threw&) {
        ASSERT_TRUE(ot.try_lock()); // check that other mutexes unlocked after throw
        ot.unlock();
        ASSERT_TRUE(rtOt.try_lock());
        rtOt.unlock();
    }

    ot.join();
    rtOt.join();
}

void test_nonmember_try_lock()
{
    Mutex mtx;
    other_mutex_thread<Mutex> ot(mtx);

    Recursive_mutex rMtx;
    Timed_mutex tMtx;

    Recursive_timed_mutex rtMtx;
    other_mutex_thread<Recursive_timed_mutex> rtOt(rtMtx);

    // try_lock with all mutexes unlocked
    ASSERT_TRUE(std::try_lock(mtx, rMtx, tMtx, rtMtx) == -1);
    mtx.unlock();
    rMtx.unlock();
    tMtx.unlock();
    rtMtx.unlock();

    // try_lock with some mutexes locked
    rtOt.lock();
    ASSERT_TRUE(std::try_lock(mtx, rMtx, tMtx, rtMtx) == 3);
    ASSERT_TRUE(ot.try_lock());
    ot.unlock();
    rtOt.unlock();

    // try_lock with throw
    throwing_mutex throwing;
    try {
        (void)std::try_lock(mtx, rtMtx, throwing);
        abort();
    }
    catch (const throwing_mutex_threw&) {
        ASSERT_TRUE(ot.try_lock()); // check that other mutexes unlocked after throw
        ot.unlock();
        ASSERT_TRUE(rtOt.try_lock());
        rtOt.unlock();
    }

    ot.join();
    rtOt.join();
}

// // Also test VSO-1253916, in which RWC like the following broke when we annotated unique_lock
// with [[nodiscard]].
std::unique_lock<std::shared_mutex> do_locked_things(std::unique_lock<std::shared_mutex> lck)
{
    return lck;
}

std::shared_lock<std::shared_mutex> do_shared_locked_things(std::shared_lock<std::shared_mutex> lck)
{
    return lck;
}

void test_vso_1253916()
{
    std::shared_mutex mtx;
    do_locked_things(std::unique_lock<std::shared_mutex>{mtx});
    do_shared_locked_things(std::shared_lock<std::shared_mutex>{mtx});
}

// Helper class used for different tests.
// STL uses plain atomics and spins while atomic reaches some value, which we can not affort,
// because we are operating in non-preemptive mode. Instead of spinning, we will call wait and
// notify CV on every operation.
// TODO: Extend as other operations are needed like --, -= etc.
//
class Atomic_wrapper {
public:
    using mo = std::memory_order;

    Atomic_wrapper(int v) : m_atom{v} {}

    int op(int (std::atomic<int>::*atomic_op)(int, std::memory_order), int v)
    {
        std::unique_lock<Mutex> lock{m_mtx};
        int res = (m_atom.*atomic_op)(v, mo::relaxed);

        lock.unlock();
        m_cv.notify_all();

        return res;
    }

    operator int() { return m_atom.load(mo::relaxed); }

    int operator+=(int v) { return op(&std::atomic<int>::fetch_add, v) + v; }

    int operator++() { return op(&std::atomic<int>::fetch_add, 1) + 1; }

    int exchange(int v) { return op(&std::atomic<int>::exchange, v); }

    template<class Predicate>
    void wait(Predicate pred)
    {
        std::unique_lock<Mutex> lock{m_mtx};
        m_cv.wait(lock, [&] { return pred(); });
    }

    void wait_while_eq(int v)
    {
        wait([&] { return m_atom.load(mo::relaxed) != v; });
    }

    void wait_while_ne(int v)
    {
        wait([&] { return m_atom.load(mo::relaxed) == v; });
    }

    void wait_while_lt(int v)
    {
        wait([&] { return m_atom.load(mo::relaxed) >= v; });
    }

    void wait_while_gt(int v)
    {
        wait([&] { return m_atom.load(mo::relaxed) <= v; });
    }

private:
    Mutex m_mtx;
    Condition_variable m_cv;
    std::atomic<int> m_atom;
};

template<typename _Mutex>
void test_one_writer()
{
    if (schedulers->workers_count() < 4)
        GTEST_SKIP() << "At least 4 workers needed for this test.";

    // One simultaneous writer.
    Atomic_wrapper atom{-1};
    _Mutex mut;

    auto f = [&] {
        atom.wait_while_eq(-1);

        std::lock_guard<_Mutex> ExclusiveLock(mut);
        const int val = ++atom;
        this_worker->sleep_for(25ms); // Not a timing assumption.
        ASSERT_TRUE(atom == val);
    };

    ASSERT_TRUE(atom.exchange(0) == -1);
    asyncs<true>(f, f, f, f);
    ASSERT_TRUE(atom == 4);
}

template<typename _Mutex>
void test_multiple_readers()
{
    if (schedulers->workers_count() < 4)
        GTEST_SKIP() << "At least 4 workers needed for this test.";

    // Many simultaneous readers.
    Atomic_wrapper atom{-1};
    _Mutex mut;

    auto f = [&] {
        atom.wait_while_eq(-1);
        std::shared_lock<_Mutex> SharedLock(mut);
        ++atom;
        atom.wait_while_lt(4);
    };

    ASSERT_TRUE(atom.exchange(0) == -1);
    asyncs<true>(f, f, f, f);
    ASSERT_TRUE(atom == 4);
}

template<typename _Mutex>
void test_writer_blocking_readers()
{
    // One writer blocking many readers.
    Atomic_wrapper atom{-4};
    _Mutex mut;

    auto f1 = [&] {
        atom.wait_while_lt(0);

        std::lock_guard<_Mutex> ExclusiveLock(mut);
        ASSERT_TRUE(atom.exchange(1000) == 0);
        this_worker->sleep_for(50ms); // Not a timing assumption.
        ASSERT_TRUE(atom.exchange(1729) == 1000);
    };

    auto f2 = [&] {
        ++atom;
        atom.wait_while_lt(1000);

        std::shared_lock<_Mutex> SharedLock(mut);
        ASSERT_TRUE(atom == 1729);
    };

    asyncs<true>(f1, f2, f2, f2, f2);
    ASSERT_TRUE(atom == 1729);
}

template<typename _Mutex>
void test_readers_blocking_writer()
{
    // Many readers blocking one writer.
    Atomic_wrapper atom{-5};
    _Mutex mut;

    auto f1 = [&] {
        std::shared_lock<_Mutex> SharedLock(mut);
        ++atom;
        atom.wait_while_lt(0);

        std::this_thread::sleep_for(50ms); // Not a timing assumption.
        atom += 10;
    };

    auto f2 = [&] {
        ++atom;
        atom.wait_while_lt(0);

        std::lock_guard<_Mutex> ExclusiveLock(mut);
        ASSERT_TRUE(atom == 40);
    };

    // join_and_clear(threads);
    asyncs<true>(f1, f1, f1, f1, f2);
    ASSERT_TRUE(atom == 40);
}

template<typename _Mutex>
void test_try_lock_and_try_lock_shared()
{
    // Test try_lock() and try_lock_shared().
    _Mutex mut;

    {
        std::unique_lock<_Mutex> MainExclusive(mut, std::try_to_lock);
        ASSERT_TRUE(MainExclusive.owns_lock());

        async<true>([&] {
            {
                std::unique_lock<_Mutex> ExclusiveLock(mut, std::try_to_lock);
                ASSERT_TRUE(!ExclusiveLock.owns_lock());
            }

            {
                std::shared_lock<_Mutex> SharedLock(mut, std::try_to_lock);
                ASSERT_TRUE(!SharedLock.owns_lock());
            }
        });
    }

    {
        std::shared_lock<_Mutex> MainShared(mut, std::try_to_lock);
        ASSERT_TRUE(MainShared.owns_lock());

        async<true>([&] {
            {
                std::unique_lock<_Mutex> ExclusiveLock(mut, std::try_to_lock);
                ASSERT_TRUE(!ExclusiveLock.owns_lock());
            }

            {
                std::shared_lock<_Mutex> SharedLock(mut, std::try_to_lock);
                ASSERT_TRUE(SharedLock.owns_lock());
            }
        });
    }
}

void test_timed_behavior()
{
    { // Test try_lock_for() and try_lock_shared_for(). No timing assumptions.
        Shared_timed_mutex stm;

        {
            std::unique_lock<Shared_timed_mutex> MainExclusive(stm, 25ms);
            ASSERT_TRUE(MainExclusive.owns_lock());

            async<true>([&] {
                {
                    std::unique_lock<Shared_timed_mutex> ExclusiveLock(stm, 25ms);
                    ASSERT_TRUE(!ExclusiveLock.owns_lock());
                }

                {
                    std::shared_lock<Shared_timed_mutex> SharedLock(stm, 25ms);
                    ASSERT_TRUE(!SharedLock.owns_lock());
                }
            });
        }

        {
            std::shared_lock<Shared_timed_mutex> MainShared(stm, 25ms);
            ASSERT_TRUE(MainShared.owns_lock());

            async<true>([&] {
                {
                    std::unique_lock<Shared_timed_mutex> ExclusiveLock(stm, 25ms);
                    ASSERT_TRUE(!ExclusiveLock.owns_lock());
                }

                {
                    std::shared_lock<Shared_timed_mutex> SharedLock(stm, 25ms);
                    ASSERT_TRUE(SharedLock.owns_lock());
                }
            });
        }
    }

    // Test delayed try_lock_for() success. GENEROUS timing assumptions.
    //
    if (schedulers->cpus_count() >= 5 && schedulers->workers_count() >= 5) {
        Atomic_wrapper atom{-5};
        Shared_timed_mutex stm;

        auto f1 = [&] {
            std::shared_lock<Shared_timed_mutex> MainShared(stm);

            ++atom;
            atom.wait_while_lt(0);

            this_worker->sleep_for(50ms);
            MainShared.unlock();
        };

        auto f2 = [&] {
            this_worker->sleep_for(50ms);

            ++atom;
            atom.wait_while_lt(0);

            std::unique_lock<Shared_timed_mutex> ExclusiveLock(stm, 1min);
            ASSERT_TRUE(ExclusiveLock.owns_lock());
            const int val = (atom += 100);
            this_worker->sleep_for(25ms);
            ASSERT_TRUE(atom == val);
        };

        asyncs<true>(f1, f2, f2, f2, f2);
        ASSERT_TRUE(atom == 400);
    }

    // Test delayed try_lock_shared_for() success. GENEROUS timing assumptions.
    //
    if (schedulers->cpus_count() >= 5 && schedulers->workers_count() >= 5) {
        Atomic_wrapper atom{-5};
        Shared_timed_mutex stm;

        auto f1 = [&] {
            std::unique_lock<Shared_timed_mutex> MainExclusive(stm);

            ++atom;
            atom.wait_while_lt(0);

            this_worker->sleep_for(50ms);
            MainExclusive.unlock();
        };

        auto f2 = [&] {
            this_worker->sleep_for(50ms);

            ++atom;
            atom.wait_while_lt(0);

            std::shared_lock<Shared_timed_mutex> SharedLock(stm, 1min);
            ASSERT_TRUE(SharedLock.owns_lock());
            atom += 11;
            atom.wait_while_lt(44);
        };

        asyncs<true>(f1, f2, f2, f2, f2);
        ASSERT_TRUE(atom == 44);
    }

    // THE GRAND FINALE: If try_lock_for() gives up due to stubborn readers,
    // it needs to deliver notifications. No timing assumptions.
    //
    if (schedulers->cpus_count() >= 3 && schedulers->workers_count() >= 3) {
        Atomic_wrapper launch_readers{0};
        Shared_timed_mutex stm;

        auto f1 = [&] { launch_readers.wait_while_eq(0); };

        auto f2 = [&] {
            this_worker->sleep_for(50ms);
            std::unique_lock<Shared_timed_mutex> ExclusiveLock(stm, 100ms);
            ASSERT_TRUE(!ExclusiveLock.owns_lock());
            launch_readers.exchange(1);
        };

        auto f3 = [&] {
            this_worker->sleep_for(50ms);
            while (!launch_readers) {
                std::shared_lock<Shared_timed_mutex> SharedLock(stm, std::try_to_lock);

                if (!SharedLock.owns_lock())
                    launch_readers.exchange(1);
            }
        };

        std::shared_lock<Shared_timed_mutex> MainShared(stm);

        asyncs<true>(f1, f2, f3);

        Atomic_wrapper readers{0};

        auto f4 = [&] {
            std::shared_lock<Shared_timed_mutex> SharedLock(stm);
            ++readers;
            readers.wait_while_lt(4);
        };

        asyncs<true>(f4, f4, f4, f4);
        ASSERT_TRUE(readers == 4);
    }
}

TEST(Mutex, mutex_sanity_test_1)
{
    auto test = [] {
        Mutex mutex;
        int counter = 0;

        auto f = [&] {
            mutex.lock();
            ++counter;
            mutex.unlock();
        };

        asyncs<true>(f, f);
        ASSERT_TRUE(counter == 2);
    };

    init_ums(test);
}

TEST(Mutex, mutex_sanity_test_2)
{
    auto test = [] {
        Mutex mutex;
        int counter = 0;
        int iterations = 100000;

        // Test 2: Mutex Contention Test
        auto f = [&] {
            for (int i = 0; i < iterations; ++i) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        };

        asyncs<true>(f, f, f, f);
        ASSERT_TRUE(counter == 4 * iterations);
    };

    init_ums(test);
}

TEST(Mutex, mutex_sanity_test_3)
{
    auto test = [] {
        try {
            Mutex mutex;

            mutex.lock();
            mutex.lock();
        }
        catch (std::system_error& ex) {
            ASSERT_TRUE(ex.code() ==
                        std::make_error_code(std::errc::resource_deadlock_would_occur));
        }
    };

    init_ums(test);
}

TEST(Mutex, mutex_sanity_test_4)
{
    auto test = [] {
        if (schedulers->cpus_count() < 2)
            GTEST_SKIP() << "At least 2 CPUs needed for this test.";

        Condition_variable_any cv;
        Mutex mut;
        std::vector<int> vec = {5};

        auto odd = [&] {
            std::unique_lock<Mutex> lk{mut};

            while (vec.size() < 6) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 1; });
                const int n = vec.back();
                vec.push_back(n * 10 + 1);
                cv.notify_one();
            }
        };

        auto even = [&] {
            std::unique_lock<Mutex> lk{mut};

            while (vec.size() < 7) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 0; });
                const int n = vec.back();
                vec.push_back(n * 10 + 2);
                cv.notify_one();
            }
        };

        asyncs<true>(odd, even);

        const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
        ASSERT_TRUE(vec == expected_val);
    };

    init_ums(test);
}

TEST(Mutex, mutex_test_fixture)
{
    auto test = [] {
        mutex_test_fixture<Mutex> fixture;
        fixture.test_lockable();
    };

    init_ums(test);
}

TEST(Shared_mutex, mutex_sanity_test_1)
{
    auto test = [] {
        Shared_mutex mutex;
        int counter = 0;

        auto f = [&] {
            mutex.lock();
            ++counter;
            mutex.unlock();
        };

        asyncs<true>(f, f);
        ASSERT_TRUE(counter == 2);
    };

    init_ums(test);
}

TEST(Shared_mutex, mutex_sanity_test_2)
{
    auto test = [] {
        Shared_mutex mutex;
        int counter = 0;
        int iterations = 100000;

        // Test 2: Mutex Contention Test
        auto f = [&] {
            for (int i = 0; i < iterations; ++i) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        };

        asyncs<true>(f, f, f, f);
        ASSERT_TRUE(counter == 4 * iterations);
    };

    init_ums(test);
}

TEST(Shared_mutex, mutex_sanity_test_3)
{
    GTEST_SKIP() << "Skipping test. Not implement throwing for deadlock on shared mutex.";

    auto test = [] {
        try {
            Shared_mutex mutex;

            mutex.lock();
            mutex.lock();
            ASSERT_TRUE(false);
        }
        catch (std::system_error& ex) {
            ASSERT_TRUE(ex.code() ==
                        std::make_error_code(std::errc::resource_deadlock_would_occur));
        }
    };

    init_ums(test);
}

TEST(Shared_mutex, sanity_test_4)
{
    auto test = [] {
        if (schedulers->cpus_count() < 2)
            GTEST_SKIP() << "At least 2 CPUs needed for this test.";

        Condition_variable_any cv;
        Shared_mutex mut;
        std::vector<int> vec = {5};

        auto odd = [&] {
            std::unique_lock<Shared_mutex> lk{mut};

            while (vec.size() < 6) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 1; });
                const int n = vec.back();
                vec.push_back(n * 10 + 1);
                cv.notify_one();
            }
        };

        auto even = [&] {
            std::unique_lock<Shared_mutex> lk{mut};

            while (vec.size() < 7) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 0; });
                const int n = vec.back();
                vec.push_back(n * 10 + 2);
                cv.notify_one();
            }
        };

        asyncs<true>(odd, even);

        const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
        ASSERT_TRUE(vec == expected_val);
    };

    init_ums(test);
}

TEST(Shared_mutex, mutex_test_fixture)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 workers needed for this test.";

    auto test = [] {
        mutex_test_fixture<Shared_mutex> fixture;
        fixture.test_lockable();
    };

    init_ums(test);
}

TEST(Shared_mutex, complex_tests)
{
    auto test = [] {
        test_one_writer<Shared_mutex>();
        test_multiple_readers<Shared_mutex>();
        test_writer_blocking_readers<Shared_mutex>();
        test_readers_blocking_writer<Shared_mutex>();
        test_try_lock_and_try_lock_shared<Shared_mutex>();
    };

    init_ums(test);
}

TEST(Recursive_mutex, mutex_sanity_test_1)
{
    auto test = [] {
        Recursive_mutex mutex;
        int counter = 0;

        auto f = [&] {
            mutex.lock();
            ++counter;
            mutex.unlock();
        };

        asyncs<true>(f, f);
        ASSERT_TRUE(counter == 2);
    };

    init_ums(test);
}

TEST(Recursive_mutex, mutex_sanity_test_2)
{
    auto test = [] {
        Recursive_mutex mutex;
        int counter = 0;
        int iterations = 100000;

        // Test 2: Mutex Contention Test
        auto f = [&] {
            for (int i = 0; i < iterations; ++i) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        };

        asyncs<true>(f, f, f, f);
        ASSERT_TRUE(counter == 4 * iterations);
    };

    init_ums(test);
}

TEST(Recursive_mutex, mutex_sanity_test_3)
{
    auto test = [] {
        Recursive_mutex mutex;

        mutex.lock();
        mutex.lock();
        mutex.unlock();
        mutex.unlock();

        ASSERT_TRUE(true);
    };

    init_ums(test);
}

TEST(Recursive_mutex, sanity_test_4)
{
    GTEST_SKIP() << "Skipping test since it lasts long, especially for debug build.";

    auto test = [] {
        try {
            Recursive_mutex mutex;

            while (true)
                mutex.lock();
        }
        catch (std::system_error& ex) {
            ASSERT_TRUE(ex.code() ==
                        std::make_error_code(std::errc::resource_unavailable_try_again));
        }
    };

    init_ums(test);
}

TEST(Recursive_mutex, recursive_mutex_test_fixture)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 workers needed for this test.";

    auto test = [] {
        mutex_test_fixture<Recursive_mutex> fixture;
        fixture.test_lockable();
        fixture.test_recursive_lockable();
    };

    init_ums(test);
}

TEST(Timed_mutex, mutex_sanity_test_1)
{
    auto test = [] {
        Timed_mutex mutex;
        int counter = 0;

        auto f = [&] {
            mutex.lock();
            ++counter;
            mutex.unlock();
        };

        asyncs<true>(f, f);
        ASSERT_TRUE(counter == 2);
    };

    init_ums(test);
}

TEST(Timed_mutex, mutex_sanity_test_2)
{
    auto test = [] {
        Timed_mutex mutex;
        int counter = 0;
        int iterations = 100000;

        // Test 2: Mutex Contention Test
        auto f = [&] {
            for (int i = 0; i < iterations; ++i) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        };

        asyncs<true>(f, f, f, f);
        ASSERT_TRUE(counter == 4 * iterations);
    };

    init_ums(test);
}

TEST(Timed_mutex, mutex_sanity_test_3)
{
    GTEST_SKIP() << "Deadlock detection not implemented.";

    auto test = [] {
        try {
            Timed_mutex mutex;

            mutex.lock();
            mutex.lock();
        }
        catch (std::system_error& ex) {
            ASSERT_TRUE(ex.code() ==
                        std::make_error_code(std::errc::resource_deadlock_would_occur));
        }
    };

    init_ums(test);
}

TEST(Timed_mutex, mutex_sanity_test_4)
{
    auto test = [] {
        if (schedulers->cpus_count() < 2)
            GTEST_SKIP() << "At least 2 CPUs needed for this test.";

        Condition_variable_any cv;
        Timed_mutex mut;
        std::vector<int> vec = {5};

        auto odd = [&] {
            std::unique_lock<Timed_mutex> lk{mut};

            while (vec.size() < 6) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 1; });
                const int n = vec.back();
                vec.push_back(n * 10 + 1);
                cv.notify_one();
            }
        };

        auto even = [&] {
            std::unique_lock<Timed_mutex> lk{mut};

            while (vec.size() < 7) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 0; });
                const int n = vec.back();
                vec.push_back(n * 10 + 2);
                cv.notify_one();
            }
        };

        asyncs<true>(odd, even);

        const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
        ASSERT_TRUE(vec == expected_val);
    };

    init_ums(test);
}

TEST(Timed_mutex, timed_mutex_test_fixture)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 workers needed for this test.";

    auto test = [] {
        mutex_test_fixture<Timed_mutex> fixture;
        fixture.test_lockable();
        fixture.test_timed_lockable();
    };

    init_ums(test);
}

TEST(Timed_mutex, timed_mutex_complex_test)
{
    auto test = [] {
        // Test try_lock_for() and try_lock_shared_for(). No timing assumptions.
        //
        {
            Timed_mutex timed_mutex;

            std::unique_lock<Timed_mutex> MainExclusive(timed_mutex, 25ms);
            ASSERT_TRUE(MainExclusive.owns_lock());

            async<true>([&] {
                std::unique_lock<Timed_mutex> ExclusiveLock(timed_mutex, 25ms);
                ASSERT_TRUE(!ExclusiveLock.owns_lock());
            });
        }

        // Test delayed try_lock_for() success. GENEROUS timing assumptions.
        //
        if (schedulers->cpus_count() >= 4 && schedulers->workers_count() >= 4) {
            Atomic_wrapper atom{-4};
            Timed_mutex timed_mutex;

            auto f = [&] {
                this_worker->sleep_for(50ms);

                ++atom;
                atom.wait_while_lt(0);

                std::unique_lock<Timed_mutex> TryExclusiveLock(timed_mutex, std::try_to_lock);
                ASSERT_TRUE(!TryExclusiveLock.owns_lock());

                std::unique_lock<Timed_mutex> TryTimedExclusiveLock(timed_mutex, 5ms);
                ASSERT_TRUE(!TryExclusiveLock.owns_lock());

                std::unique_lock<Timed_mutex> ExclusiveLock(timed_mutex, 1min);
                ASSERT_TRUE(ExclusiveLock.owns_lock());
                const int val = (atom += 100);
                this_worker->sleep_for(25ms);
                ASSERT_TRUE(atom == val);
            };

            auto f2 = [&] {
                std::unique_lock<Timed_mutex> MainUnique(timed_mutex);
                this_worker->sleep_for(50ms);

                ++atom;
                atom.wait_while_lt(0);

                this_worker->sleep_for(50ms);
                MainUnique.unlock();
            };

            asyncs<true>(f, f, f, f2);
            ASSERT_TRUE(atom == 300);
        }
    };

    init_ums(test);
}

TEST(Recursive_timed_mutex, mutex_sanity_test_1)
{
    auto test = [] {
        Recursive_timed_mutex mutex;
        int counter = 0;

        auto f = [&] {
            mutex.lock();
            ++counter;
            mutex.unlock();
        };

        asyncs<true>(f, f);
        ASSERT_TRUE(counter == 2);
    };

    init_ums(test);
}

TEST(Recursive_timed_mutex, mutex_sanity_test_2)
{
    auto test = [] {
        Recursive_timed_mutex mutex;
        int counter = 0;
        int iterations = 100000;

        // Test 2: Mutex Contention Test
        auto f = [&] {
            for (int i = 0; i < iterations; ++i) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        };

        asyncs<true>(f, f, f, f);
        ASSERT_TRUE(counter == 4 * iterations);
    };

    init_ums(test);
}

TEST(Recursive_timed_mutex, mutex_sanity_test_3)
{
    auto test = [] {
        Recursive_timed_mutex mutex;

        mutex.lock();
        mutex.lock();
        mutex.unlock();
        mutex.unlock();

        ASSERT_TRUE(true);
    };

    init_ums(test);
}

TEST(Recursive_timed_mutex, mutex_sanity_test_4)
{
    GTEST_SKIP() << "Skipping test since it lasts long, especially for debug build.";

    auto test = [] {
        try {
            Recursive_timed_mutex mutex;

            while (true)
                mutex.lock();
        }
        catch (std::system_error& ex) {
            ASSERT_TRUE(ex.code() ==
                        std::make_error_code(std::errc::resource_unavailable_try_again));
        }
    };

    init_ums(test);
}

TEST(Recursive_timed_mutex, recursive_timed_mutex_test_fixture)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 workers needed for this test.";

    auto test = [] {
        mutex_test_fixture<Recursive_timed_mutex> fixture;
        fixture.test_lockable();
        fixture.test_timed_lockable();
        fixture.test_recursive_lockable();
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, mutex_sanity_test_1)
{
    auto test = [] {
        Shared_timed_mutex mutex;
        int counter = 0;

        auto f = [&] {
            mutex.lock();
            ++counter;
            mutex.unlock();
        };

        asyncs<true>(f, f);
        ASSERT_TRUE(counter == 2);
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, mutex_sanity_test_2)
{
    auto test = [] {
        Shared_timed_mutex mutex;
        int counter = 0;
        int iterations = 100000;

        // Test 2: Mutex Contention Test
        auto f = [&] {
            for (int i = 0; i < iterations; ++i) {
                mutex.lock();
                ++counter;
                mutex.unlock();
            }
        };

        asyncs<true>(f, f, f, f);
        ASSERT_TRUE(counter == 4 * iterations);
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, mutex_sanity_test_3)
{
    GTEST_SKIP() << "Skipping test. Not implement throwing for deadlock on shared timed mutex.";

    auto test = [] {
        try {
            Shared_timed_mutex mutex;

            mutex.lock();
            mutex.lock();
            ASSERT_TRUE(false);
        }
        catch (std::system_error& ex) {
            ASSERT_TRUE(ex.code() ==
                        std::make_error_code(std::errc::resource_deadlock_would_occur));
        }
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, sanity_test_4)
{
    auto test = [] {
        if (schedulers->cpus_count() < 2)
            GTEST_SKIP() << "At least 2 CPUs needed for this test.";

        Condition_variable_any cv;
        Shared_timed_mutex mut;
        std::vector<int> vec = {5};

        auto odd = [&] {
            std::unique_lock<Shared_timed_mutex> lk{mut};

            while (vec.size() < 6) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 1; });
                const int n = vec.back();
                vec.push_back(n * 10 + 1);
                cv.notify_one();
            }
        };

        auto even = [&] {
            std::unique_lock<Shared_timed_mutex> lk{mut};

            while (vec.size() < 7) {
                cv.wait(lk, [&vec] { return vec.size() % 2 == 0; });
                const int n = vec.back();
                vec.push_back(n * 10 + 2);
                cv.notify_one();
            }
        };

        asyncs<true>(odd, even);

        const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
        ASSERT_TRUE(vec == expected_val);
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, shared_timed_mutex_test_fixture)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 workers needed for this test.";

    auto test = [] {
        mutex_test_fixture<Shared_timed_mutex> fixture;
        fixture.test_lockable();
        fixture.test_timed_lockable();
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, complex_tests_1)
{
    auto test = [] {
        test_one_writer<Shared_timed_mutex>();
        test_multiple_readers<Shared_timed_mutex>();
        test_writer_blocking_readers<Shared_timed_mutex>();
        test_readers_blocking_writer<Shared_timed_mutex>();
        test_try_lock_and_try_lock_shared<Shared_timed_mutex>();
    };

    init_ums(test);
}

TEST(Shared_timed_mutex, complex_tests_2)
{
    init_ums(test_timed_behavior);
}

TEST(STL_Mutex, nonmember_lock_test)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 CPUs needed for this test.";

    init_ums(test_nonmember_lock);
}

TEST(STL_Mutex, nonmember_try_lock_test)
{
    // if (schedulers->workers_count() < 3)
    //     GTEST_SKIP() << "At least 3 CPUs needed for this test.";

    init_ums(test_nonmember_try_lock);
}

TEST(STL_Mutex, test_vso_1253916)
{
    init_ums(test_vso_1253916);
}

// More mutex tests:
// https://github.com/microsoft/STL/blob/1e312b38db8df1dfbea17adc344454feb8d00dd9/tests/std/tests/Dev11_1150223_shared_mutex/test.cpp

// NOLINTEND

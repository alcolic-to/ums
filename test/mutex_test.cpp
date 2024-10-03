// NOLINTBEGIN

#include <atomic>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "condition_variable.h"
#include "gtest/gtest.h"
#include "mutex.h"
#include "task_manager.h"
#include "util.h"

template<class Lockable>
void run_lock_perf_test(const std::string& test_name, int num_threads)
{
    static Lockable lockable;
    constexpr uint64_t iter_count = 100000;
    uint64_t counter = 0;

    auto lock_fn = [&] {
        for (uint64_t i = 0; i < iter_count; ++i) {
            std::scoped_lock<Lockable> lock{lockable};
            ++counter;
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    // Disabling next line since it causes clang-tidy dump with AV.
    // Stopwatch<std::chrono::microseconds> sw{
    //     std::format("{:8} with {:4} threads", test_name, num_threads)};

    std::stringstream ss;
    ss << std::setw(10) << std::left << test_name << " with " << std::setw(4) << std::right
       << num_threads << " thread(s)";

    Stopwatch<std::chrono::microseconds> sw{ss.str()};

    for (int i = 0; i < num_threads; ++i)
        threads.emplace_back(lock_fn);

    for (auto&& t : threads)
        t.join();

    ASSERT_TRUE(counter == num_threads * iter_count);
}

TEST(Mutex, mutex_vs_spinlock_perf_test_1)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//     GTEST_SKIP() << "Skipping perf tests in debug build.";
// #endif

    std::cout << "---------------------------------------------------------------\n";
    for (int threads_count = 1; threads_count <= 128; threads_count *= 2) {
        run_lock_perf_test<Spinlock>("Spinlock", threads_count);
        run_lock_perf_test<std::mutex>("std::mutex", threads_count);
        std::cout << "---------------------------------------------------------------\n";
    }
}

TEST(Mutex, mutex_sanity_test_1)
{
    Mutex mutex;
    int counter = 0;

    auto f = [&] {
        mutex.lock();
        ++counter;
        mutex.unlock();
    };

    task_manager.execute_tasks<false>(f, f);
    ASSERT_TRUE(counter == 2);
}

TEST(Mutex, mutex_sanity_test_2)
{
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

    task_manager.execute_tasks<false>(f, f, f, f);
    ASSERT_TRUE(counter == 4 * iterations);
}

TEST(Mutex, mutex_peft_test_1)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//     GTEST_SKIP() << "Skipping perf tests in debug build.";
// #endif

    Mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        for (int i = 0; i < iterations; ++i) {
            std::scoped_lock<Mutex> lock{mutex};
            ++counter;
        }
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, spinlock_peft_test_1)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//     GTEST_SKIP() << "Skipping perf tests in debug build.";
// #endif

    Spinlock mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        for (int i = 0; i < iterations; ++i) {
            std::scoped_lock<Spinlock> lock{mutex};
            ++counter;
        }
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, std_mutex_peft_test_1)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//    GTEST_SKIP() << "Skipping perf tests in debug build.";    
// #endif

    std::mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        for (int i = 0; i < iterations; ++i) {
            std::scoped_lock<std::mutex> lock{mutex};
            ++counter;
        }
    };

    std::vector<std::thread> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i)
        v.emplace_back(std::thread{f});

    for (auto&& thread : v)
        thread.join();

    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, mutex_peft_test_2)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//     GTEST_SKIP() << "Skipping perf tests in debug build.";
// #endif

    Mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        std::scoped_lock<Mutex> lock{mutex};
        for (int i = 0; i < iterations; ++i)
            ++counter;
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, spinlock_peft_test_2)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//     GTEST_SKIP() << "Skipping perf tests in debug build.";
// #endif

    Spinlock mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        std::scoped_lock<Spinlock> lock{mutex};
        for (int i = 0; i < iterations; ++i)
            ++counter;
    };

    task_manager.execute_tasks<false>(f, f, f, f, f, f, f, f, f, f, f, f, f, f, f, f);
    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Mutex, std_mutex_peft_test_2)
{
    GTEST_SKIP() << "Skipping perf test, since it last long.";
// #if defined(DEBUG)
//     GTEST_SKIP() << "Skipping perf tests in debug build.";
// #endif

    std::mutex mutex;
    int counter = 0;
    int iterations = 100000000;

    auto f = [&] {
        std::scoped_lock<std::mutex> lock{mutex};
        for (int i = 0; i < iterations; ++i)
            ++counter;
    };

    std::vector<std::thread> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i)
        v.emplace_back(std::thread{f});

    for (auto&& thread : v)
        thread.join();

    ASSERT_TRUE(counter == 16 * iterations);
}

TEST(Condition_variable, cv_sanity_test_1)
{
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

    task_manager.execute_tasks<false>(waiter, notifier);
}

TEST(Condition_variable, cv_sanity_test_2)
{
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
        tls_worker->sleep_for(1s);
        cv2.notify_one();

        tls_worker->sleep_for(1s);
        cv1.notify_one();

        tls_worker->sleep_for(1s);
        cv3.notify_one();

        // Wait for the threads to finish.
        tls_worker->sleep_for(2s);
    };

    task_manager.execute_tasks<false>(waiter_1, waiter_2, waiter_3, notifier);

    ASSERT_TRUE(v.size() == 3);
    ASSERT_TRUE(v[0] == 2);
    ASSERT_TRUE(v[1] == 1);
    ASSERT_TRUE(v[2] == 3);
}

TEST(Condition_variable, cv_producer_consumer_test)
{
    Mutex mutex;
    Condition_variable cv;
    int data = 0;
    bool ready = false;

    auto producer = [&] {
        tls_worker->sleep_for(100ms); // Simulate work

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

    task_manager.execute_tasks<false>(producer, consumer);
}

TEST(Condition_variable, cv_spurious_wakeup_test)
{
    Mutex mutex;
    Condition_variable cv;
    bool ready = false;

    auto spurious_wakeup = [&] {
        std::unique_lock<Mutex> lock(mutex);
        cv.wait(lock); // This should block until a real notification
        ASSERT_TRUE(!ready);
    };

    auto notifier = [&] {
        tls_worker->sleep_for(std::chrono::milliseconds(100)); // Simulate some wait
        cv.notify_one();
    };

    task_manager.execute_tasks<false>(spurious_wakeup, notifier);
}

// https://github.com/microsoft/STL/blob/1e312b38db8df1dfbea17adc344454feb8d00dd9/tests/std/include/test_header_units_and_modules.hpp#L150
TEST(Condition_variable, condition_variable_complex_test_1)
{
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

    task_manager.execute_tasks<false>(odd, even);

    const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
    ASSERT_TRUE(vec == expected_val);

    static_assert(static_cast<int>(std::cv_status::no_timeout) == 0);
    static_assert(static_cast<int>(std::cv_status::timeout) == 1);
}

TEST(Condition_variable, condition_variable_complex_test_2)
{
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

        tls_worker->sleep_for(100ms);
        ready = true;
        increment_cv.notify_all();
    };

    task_manager.execute_tasks<false>(increment, increment, increment, increment, increment,
                                      increment, increment, increment, notifier);

    ASSERT_TRUE(atomic_counter.load() == 2 * threads_count);
}

void test_shared_mutex()
{
    std::condition_variable_any cv;
    std::shared_mutex mut;
    std::vector<int> vec = {5};

    std::thread odd{[&] {
        std::unique_lock<std::shared_mutex> lk{mut};

        while (vec.size() < 6) {
            cv.wait(lk, [&vec] { return vec.size() % 2 == 1; });
            const int n = vec.back();
            vec.push_back(n * 10 + 1);
            cv.notify_one();
        }
    }};

    std::thread even{[&] {
        std::unique_lock<std::shared_mutex> lk{mut};

        while (vec.size() < 7) {
            cv.wait(lk, [&vec] { return vec.size() % 2 == 0; });
            const int n = vec.back();
            vec.push_back(n * 10 + 2);
            cv.notify_one();
        }
    }};

    odd.join();
    even.join();

    const std::vector<int> expected_val = {5, 51, 512, 5121, 51212, 512121, 5121212};
    ASSERT_TRUE(vec == expected_val);
}

// Mext section is slightly modified stl mutex tests:
// https://github.com/microsoft/STL/blob/1e312b38db8df1dfbea17adc344454feb8d00dd9/tests/std/tests/VSO_0226079_mutex/test.cpp
#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

[[noreturn]] void api_unexpected(const char* const api_name)
{
    std::perror(api_name);
    std::abort();
}

class event {
public:
    event() = default;

    void signal()
    {
        std::unique_lock<std::mutex> lock(mtx);
        is_set = true;
        cv.notify_all();
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return is_set; });
        is_set = false; // reset after wait
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    bool is_set = false;
};

template<typename Mutex>
class other_mutex_thread {
public:
    enum class message { idle, shutdown, lock, tryLock, unlock, unlockDelayed };

    explicit other_mutex_thread(Mutex& targetMtx)
        : mtx(targetMtx)
        , currentMessage(message::idle)
        , tryLockSuccess(false)
    {
        task_manager.execute_task<true>([&] { thread_func(); });
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

    Mutex& mtx;
    event backgroundEvent;
    event foregroundEvent;
    message currentMessage;
    bool tryLockSuccess;
};

template<typename Func>
std::chrono::nanoseconds time_execution(Func&& f)
{
    const auto startTime = std::chrono::system_clock::now();
    std::forward<Func>(f)();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() -
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

template<typename Mutex>
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

        test_guard<std::lock_guard<Mutex>>();
        test_guard<std::unique_lock<Mutex>>();

        { // unique_lock constructor, move constructor, move assignment
            std::unique_lock<Mutex> ulOuter;
            STATIC_ASSERT(noexcept(std::unique_lock<Mutex>()));
            ASSERT_TRUE(!ulOuter.owns_lock());
            ASSERT_TRUE(!ulOuter);
            ASSERT_TRUE(ulOuter.mutex() == nullptr);

            {
                std::unique_lock<Mutex> ul(mtx);
                ASSERT_TRUE(!ot.try_lock());
                ASSERT_TRUE(ul.owns_lock());
                ASSERT_TRUE(static_cast<bool>(ul));
                ASSERT_TRUE(ul.mutex() == &mtx);

                std::unique_lock<Mutex> ulMoveConstructed(std::move(ul));
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
            STATIC_ASSERT(noexcept(std::unique_lock<Mutex>(mtx, std::defer_lock)));
            std::unique_lock<Mutex> ul(mtx, std::defer_lock);
            ASSERT_TRUE(!ul);
            ASSERT_TRUE(ot.try_lock());
            ot.unlock();
        }

        { // unique_lock try_to_lock success
            std::unique_lock<Mutex> ul(mtx, std::try_to_lock);
            ASSERT_TRUE(static_cast<bool>(ul));
            ASSERT_TRUE(!ot.try_lock());
        }

        ASSERT_TRUE(ot.try_lock());

        { // unique_lock try_to_lock failure
            std::unique_lock<Mutex> ul(mtx, std::try_to_lock);
            ASSERT_TRUE(!ul);
        }

        ot.unlock();

        { // swap with empty object
            std::unique_lock<Mutex> ulLeft(mtx);
            std::unique_lock<Mutex> ulRight;
            ASSERT_TRUE(static_cast<bool>(ulLeft));
            ASSERT_TRUE(!ulRight);
            std::swap(ulLeft, ulRight);
            ASSERT_TRUE(!ulLeft);
            ASSERT_TRUE(static_cast<bool>(ulRight));
        }

        { // swap objects
            std::unique_lock<Mutex> ulLeft(mtx);
            std::unique_lock<Mutex> ulRight(mtx2);
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
            std::unique_lock<Mutex> ul(mtx);
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
            std::unique_lock<Mutex> ul(mtx);
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
                        ASSERT_TRUE(mtx.try_lock_until(std::chrono::system_clock::now() +
                                                       std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        mtx.unlock();
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<Mutex> ul(mtx, std::defer_lock);
                        ASSERT_TRUE(ul.try_lock_for(std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<Mutex> ul(mtx, std::chrono::hours(24));
                        ASSERT_TRUE(ul.owns_lock());
                    }) < std::chrono::hours(1));
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<Mutex> ul(mtx, std::defer_lock);
                        ASSERT_TRUE(ul.try_lock_until(std::chrono::system_clock::now() +
                                                      std::chrono::hours(24)));
                    }) < std::chrono::hours(1));
        ASSERT_TRUE(time_execution([this] {
                        std::unique_lock<Mutex> ul(mtx, std::chrono::system_clock::now() +
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

    Mutex mtx;
    Mutex mtx2;
    other_mutex_thread<Mutex> ot;
};

// TODO: Replace std::recursive_mutex, std::timed_mutex, std::recursive_timed_mutex
// as we implement these.
//
void test_nonmember_lock()
{
    Mutex mtx;
    other_mutex_thread<Mutex> ot(mtx);

    std::recursive_mutex rMtx;
    std::timed_mutex tMtx;

    std::recursive_timed_mutex rtMtx;
    other_mutex_thread<std::recursive_timed_mutex> rtOt(rtMtx);

    lock(mtx, rMtx, tMtx, rtMtx);
    mtx.unlock();
    rMtx.unlock();
    tMtx.unlock();
    rtMtx.unlock();

    ot.lock();
    rtOt.lock();
    ot.unlock_delayed();   // no timing assumptions: lock() retries until it
    rtOt.unlock_delayed(); // can lock all input mutexes
    lock(rtMtx, tMtx, mtx, rMtx);
    mtx.unlock();
    rMtx.unlock();
    tMtx.unlock();
    rtMtx.unlock();
    ot.finish_delayed();
    rtOt.finish_delayed();

    throwing_mutex throwing;
    try {
        lock(rtMtx, mtx, throwing); // throws
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

// TODO: Replace std::recursive_mutex, std::timed_mutex, std::recursive_timed_mutex
// as we implement these.
//
void test_nonmember_try_lock()
{
    Mutex mtx;
    other_mutex_thread<Mutex> ot(mtx);

    std::recursive_mutex rMtx;
    std::timed_mutex tMtx;

    std::recursive_timed_mutex rtMtx;
    other_mutex_thread<std::recursive_timed_mutex> rtOt(rtMtx);

    // try_lock with all mutexes unlocked
    ASSERT_TRUE(try_lock(mtx, rMtx, tMtx, rtMtx) == -1);
    mtx.unlock();
    rMtx.unlock();
    tMtx.unlock();
    rtMtx.unlock();

    // try_lock with some mutexes locked
    rtOt.lock();
    ASSERT_TRUE(try_lock(mtx, rMtx, tMtx, rtMtx) == 3);
    ASSERT_TRUE(ot.try_lock());
    ot.unlock();
    rtOt.unlock();

    // try_lock with throw
    throwing_mutex throwing;
    try {
        (void)try_lock(mtx, rtMtx, throwing);
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

TEST(STL_Mutex, mutex_test)
{
    task_manager.execute_task<false>([] {
        mutex_test_fixture<Mutex> fixture;
        fixture.test_lockable();
    });
}

TEST(STL_Mutex, nonmember_lock_test)
{
    task_manager.execute_task<false>([] { test_nonmember_lock(); });
}

TEST(STL_Mutex, nonmember_try_lock_test)
{
    task_manager.execute_task<false>([] { test_nonmember_try_lock(); });
}

// Next section contains things that needs to be tested too. We only implemented regular mutex.
// Remove below when implemented.
//
// int main()
// {
//     {
//         mutex_test_fixture<Mutex> fixture;
//         fixture.test_lockable();
//     }

//     {
//         mutex_test_fixture<std::timed_mutex> fixture;
//         fixture.test_lockable();
//         fixture.test_timed_lockable();
//     }

//     {
//         mutex_test_fixture<std::recursive_mutex> fixture;
//         fixture.test_lockable();
//         fixture.test_recursive_lockable();
//     }

//     {
//         mutex_test_fixture<std::recursive_timed_mutex> fixture;
//         fixture.test_lockable();
//         fixture.test_timed_lockable();
//         fixture.test_recursive_lockable();
//     }

//     {
//         mutex_test_fixture<std::shared_mutex> fixture;
//         fixture.test_lockable();
//         // shared-ownership locking behavior tested in Dev11_1150223_shared_mutex
//     }

//     {
//         mutex_test_fixture<std::shared_timed_mutex> fixture;
//         fixture.test_lockable();
//         fixture.test_timed_lockable();
//     }

//     test_nonmember_lock();
//     test_nonmember_try_lock();

//     test_vso_1253916();
// }

// More mutex tests:
// https://github.com/microsoft/STL/blob/1e312b38db8df1dfbea17adc344454feb8d00dd9/tests/std/tests/Dev11_1150223_shared_mutex/test.cpp

// NOLINTEND

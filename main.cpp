// NOLINTBEGIN

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#undef min
#undef max
#endif

#include "condition_variable.h"
#include "io_api.h"
#include "mutex.h"
#include "task_manager.h"
#include "util.h"
#include "worker.h"

using namespace std::chrono_literals;

void f2()
{
    tls_worker->yield();
}

void f1()
{
    auto start = now();

    std::vector<uint64_t> v;
    for (int i = 0; i < 1000; ++i)
        v.push_back(random());

    int funcDur = random() % 11;

    int i = 0;
    while (true) {
        auto end = now();

        std::chrono::duration<double, std::milli> duration = end - start;
        if (duration.count() > funcDur) {
            std::cout << "Task execution exceeded time limit of " << duration.count() << "ms.\n";
            break;
        }

        if (v[random() % v.size()] == random() % v.size() && i++ % 100 == 0)
            tls_worker->yield();
    }

    return;
}

// Duration of ~1s when plugged in.
//
uint64_t f3()
{
    uint64_t first = 0, second = 1;

    // Fibbonaci seq.
    //
    for (uint64_t i = 2; i < 3000000000; ++i) {
        uint64_t sum = first + second;
        first = second;
        second = sum;

        // if (i % 10000 == 0)
        //     tls_worker->wait_event(e);
    }

    return second;
}

void thread_function()
{
    for (int i = 0; i < 1000; ++i)
        task_manager.execute_task<false>(f3);
}

// Duration of ~4ms when plugged in.
//
uint64_t f4()
{
    uint64_t first = 0, second = 1;

    // Fibbonaci seq.
    //
    for (uint64_t i = 2; i < 10000000; ++i) {
        uint64_t sum = first + second;
        first = second;
        second = sum;

        // if (i % 10000 == 0)
        //     tls_worker->wait_event(e);
    }

    return second;
}

void ms3_function()
{
    auto start = now();

    for (int i = 0; i < 1000; ++i) {
        task_manager.execute_task<false>(f4);
        // std::cout << GetCurrentProcessorNumber() << "\n";
        // r += f4();
    }

    auto end = now();

    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Total exec time: " << duration.count() << "ms.\n";

    // std::this_thread::sleep_for(10s);

    // std::cout << r << "\n";
}

void sleep_test()
{
    tls_worker->sleep_for(1000ms);
}

#if defined _WIN32

HANDLE file = CreateFile("io_testing_file_0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OVERLAPPED |
                             FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                         NULL);

void single_read()
{
    constexpr int read_size = 8 * 1024;

    auto buf = std::make_unique<char[]>(read_size);

    cos_read_file(file, {buf.get(), read_size}, 0);
    std::cout << "Read buffer: " << buf.get() << "\n";
}

void single_write()
{
    int write_size = 8 * 1024;

    std::string io_str(write_size, 'a');

    cos_write_file(file, {io_str.data(), io_str.size()}, 0);
}

void read_from_file()
{
    constexpr int read_size = 8 * 1024;

    auto buf = std::make_unique<char[]>(read_size);

    cos_read_file(file, {buf.get(), read_size}, (random() % read_size) * read_size);
    // std::cout << "Read buffer: " << buf.get() << "\n";
}

void write_to_file()
{
    int write_size = 8 * 1024;
    int max_file_size = write_size * 128;

    std::string io_str(write_size, 'a');

    // for (int i = 0; i < 10; ++i)
    //     std::cout << prng.rand<uint64_t>() << "\n";

    cos_write_file(file, {io_str.data(), io_str.size()},
                   (random() % max_file_size) * io_str.size());
}

static Spinlock sl;

static uint64_t sum = 0;

void testing_spinlock()
{
    // std::scoped_lock<Spinlockic> lock{sl};
    for (int i = 0; i < 1000000; ++i) {
        std::scoped_lock<Spinlock> lock{sl};
        sum += 1;
    }
}

static Mutex m;

// static Spinlock spinlock;

void testing_mutex()
{
    // std::scoped_lock<Mutex> lock{m};
    for (int i = 0; i < 1000000; ++i) {
        std::scoped_lock<Mutex> lock{m};
        // std::scoped_lock<Spinlock> lock{spinlock};
        sum += 1;
    }
}

void execute_testing_mutex_tasks()
{
    constexpr int tasks_count = 1000;

    std::vector<std::shared_ptr<Task>> tasks;
    tasks.reserve(tasks_count);

    Stopwatch sw{"Testing mutex"};

    for (int i = 0; i < tasks_count; ++i) {
        std::shared_ptr<Task> task{std::make_shared<Task>(testing_mutex)};
        task_manager.enque_task(task);
        tasks.push_back(std::move(task));
    }

    for (auto&& task : tasks)
        task->wait();

    std::cout << sum << "\n";
}

int main(int argc, char* argv[])
{
    Stopwatch<microseconds> s{"Sleep for stopwatch"};
    std::cout << "Entering sleep for!\n";

    task_manager.execute_task<false>([] { tls_worker->sleep_for(5s); });

    std::cout << "Exiting sleep for!\n";
}

#else

int main(int argc, char* argv[])
{
    Stopwatch sw("Sleep test");

    for (std::size_t i = 0; i < 10; ++i)
        task_manager.execute_task<false>(sleep_test);
}

#endif

// NOLINTEND

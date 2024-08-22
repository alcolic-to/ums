#include <cstdint>
#include <cstdio>
#include <iostream>
#include <random>
#include <chrono>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

#include "worker.h"
#include "task_manager.h"
#include "util.h"
#include "sync_api.h"
#include "io_api.h"

std::random_device rd;     // only used once to initialise (seed) engine
std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)

class PRNG
{
public:
    PRNG(uint64_t seed) : m_seed(seed) { }

    template<typename T>
    T rand() { return T(rand64()); }

private:
    uint64_t m_seed;

    uint64_t rand64()
    {
        m_seed ^= m_seed >> 12, m_seed ^= m_seed << 25, m_seed ^= m_seed >> 27;
        return m_seed * 2685821657736338717LL;
    }
};

static PRNG prng{ 1070372 };

using namespace std::chrono_literals;

void f2()
{
    tls_worker->yield();
}

void f1()
{
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<int> v;
    for (int i = 0; i < 1000; ++i)
        v.push_back(rand());

    std::uniform_int_distribution<int> uni(0, 10);
    int funcDur = uni(rng);
    // int funcDur = 1;

    int i = 0;
    while (true)
    {
        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> duration = end - start;
        if (duration.count() > funcDur)
        {
            std::cout << "Task execution exceeded time limit of " << duration.count() << "ms.\n";
            break;
        }

        if (v[rand() % v.size()] == rand() % v.size() && i++ % 100 == 0)
        {
            tls_worker->yield();
        }
    }

    return;
}

ConditionalEvent e;

// Duration of ~1s when plugged in.
//
uint64_t f3()
{
    uint64_t first = 0, second = 1;
    
    // Fibbonaci seq.
    //
    for (uint64_t i = 2; i < 3000000000; ++i)
    {
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
    for (uint64_t i = 2; i < 10000000; ++i)
    {
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
    uint64_t r = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i)
    {
        task_manager.execute_task<false>(f4);
        // std::cout << GetCurrentProcessorNumber() << "\n";
        // r += f4();
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Total exec time: " << duration.count() << "ms.\n";

    // std::this_thread::sleep_for(10s);

    // std::cout << r << "\n";
}

void test_signal()
{
    std::vector<std::thread> v;
    
    for (int i = 0; i < 10; ++i)
        v.push_back(std::thread{ thread_function });
    
    for (auto& it : v)
        it.join();

    std::this_thread::sleep_for(5s);
    e.signal();

    std::cout << f3() << "\n";
}

#if defined _WIN32

HANDLE file = CreateFile("io_testing_file_0", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

void single_read()
{
    constexpr int read_size = 8 * 1024;
    int max_read_size = read_size * 128;

    auto buf = std::make_unique<char[]>(read_size);

    cos_read_file(file, buf.get(), read_size, 0);
    std::cout << "Read buffer: " << buf.get() << "\n";
}

void single_write()
{
    int write_size = 8 * 1024;
    int max_file_size = write_size * 128;

    std::string io_str(write_size, 'a');

    cos_write_file(file, io_str.data(), io_str.size(), 0);
}

void read_from_file()
{
    constexpr int read_size = 8 * 1024;
    int max_read_size = read_size * 128;

    auto buf = std::make_unique<char[]>(read_size);

    cos_read_file(file, buf.get(), read_size, (prng.rand<uint64_t>() % read_size) * read_size);
    // std::cout << "Read buffer: " << buf.get() << "\n";
}

void write_to_file()
{
    int write_size = 8 * 1024;
    int max_file_size = write_size * 128;

    std::string io_str(write_size, 'a');
    
    // for (int i = 0; i < 10; ++i)
    //     std::cout << prng.rand<uint64_t>() << "\n";

    cos_write_file(file, io_str.data(), io_str.size(), (prng.rand<uint64_t>() % max_file_size) * io_str.size());
}

int main(int argc, char* argv[])
{
    // for (int i = 0; i < 1024; ++i)
    //     task_manager.execute_task<true>(write_to_file);
    // 
    // for (int i = 0; i < 1000 * 1024; ++i)
    //     task_manager.execute_task<true>(read_from_file);

    // task_manager.execute_task<false>(write_to_file);
    // task_manager.execute_task<false>(read_from_file);

    for (int i = 0; i < 10000000; ++i)
        task_manager.execute_task<true>(f4);
}

#else

#ifdef __linux__

#include <fcntl.h>
#include <string.h>
#include <liburing.h>

#include <errno.h>

const std::size_t block_size = 4096;
std::mutex io_mutex;
const std::size_t num_threads = 100;

void create_file(const char *file, size_t size, char pattern)
{
	char *buf;
	int fd;

	buf = (char*) malloc(size);
	memset(buf, pattern, size);

	fd = open(file, O_WRONLY | O_CREAT, 0644);

	write(fd, buf, size);
	fsync(fd);
	close(fd);
	free(buf);
}

void wait_complete(struct io_uring* ring, std::size_t thread_id)
{
    // cqe is a pointer to the completion queue entry
    struct io_uring_cqe *cqe;

    int ret = io_uring_wait_cqe(ring, &cqe);
    if (ret == 0 && cqe != nullptr)
    {
        std::uint64_t tid = io_uring_cqe_get_data64(cqe);
        if (thread_id != tid)
        {
            std::cout << "Thread ids don't match. tid " << tid << " thread_id " << thread_id << std::endl;
            return;
        }

        if (cqe->res != block_size)
        {
            std::cout << "Thread " << tid << " wrong number of bytes " << cqe->res << std::endl;
            return;
        }

        std::cout << "Everything ok for thread " << tid << " cqe->res " << cqe->res << std::endl;
    }
    else
    {
        std::cout << "Something went wrong on thread " << thread_id << std::endl;
    }
}

void write_to_file(int thread_id)
{
    int fd = open("io_testing_file_0", O_RDWR | O_DIRECT);
    char *buffer;

    // Allocate aligned memory
    if (posix_memalign((void **)&buffer, block_size, block_size))
    {
        std::cerr << "posix_memalign failed" << std::endl;
        return;
    }

    struct io_uring ring;
    io_uring_queue_init(1, &ring, 0);

    // Get a submission queue entry
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (sqe == nullptr)
    {
        std::cerr << "io_uring_get_sqe failed" << std::endl;
        close(fd);
        free(buffer);
        return;
    }

    struct iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = block_size;

    // Prepare the I/O request
    io_uring_prep_writev(sqe, fd, &iov, 1, block_size * thread_id);
    io_uring_sqe_set_data64(sqe, thread_id);

    // Submit the I/O request
    int ret = io_uring_submit(&ring);
    if (ret < 0)
    {
        std::cerr << "io_uring_submit failed" << std::endl;
        close(fd);
        free(buffer);
        io_uring_queue_exit(&ring);
        return;
    }
    else if (ret!= 1)
    {
        std::cerr << "io_uring failed to submit 1 task " << std::endl;
        return;
    }

    wait_complete(&ring, thread_id);

    free(buffer);
    close(fd);
}

#endif // __linux__

void sleep_test()
{
    cos_sleep(1000);
}

int fd = open("uring_testing", O_RDWR, O_DIRECT);

char msg1[] = "This is thread1.\n";
std::uint64_t off1 = 0;

char msg2[] = "This is thread2.\n";
std::uint64_t off2 = strlen(msg1);

char msg3[] = "This is thread3.\n";
std::uint64_t off3 = off2 + strlen(msg2);

void write_thread1()
{
    cos_write_file(reinterpret_cast<void*>(&fd),
            reinterpret_cast<void*>(msg1),
            strlen(msg1),
            off1);
}

void write_thread2()
{
    cos_write_file(reinterpret_cast<void*>(&fd),
            reinterpret_cast<void*>(msg2),
            strlen(msg2),
            off2);
}

void write_thread3()
{
    cos_write_file(reinterpret_cast<void*>(&fd),
            reinterpret_cast<void*>(msg3),
            strlen(msg3),
            off3);
}

void read_thread1()
{
    std::size_t to_read = strlen(msg1);
    std::cout << "To read " << to_read << std::endl;
    char* buffer = new char[to_read + 1];
    cos_read_file(reinterpret_cast<void*>(&fd),
            reinterpret_cast<void*>(buffer),
            to_read,
            0);

    buffer[to_read] = '\0';
    std::cout << buffer;

    delete[] buffer;
}

#include <signal.h>
#include <execinfo.h> // backtrace/backtrace_symbols_fd

void signal_handler(int sig)
{
    void *array[10];
    size_t size;

    size = backtrace(array, 10);

    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    exit(1);
}

int main(int argc, char* argv[])
{
	// char *buf;
	//
	// buf = (char*) malloc(5000);
	// memset(buf, 'a', 5000);
	//
	// int temp = open("uring_testing", O_WRONLY | O_CREAT, 0644);
	//
	// write(temp, buf, 5000);
	// fsync(temp);
	// close(temp);
	// free(buf);
    
	// sleep(100);
    // signal(SIGSEGV, signal_handler);
    // create_file("io_testing_file_0", block_size * num_threads, 'a');
    // std::vector<std::thread> v;
    //
    // // for (std::size_t i = 0; i < num_threads; ++i)
    // //     write_to_file(i);
    //
    // for (std::size_t i = 0; i < num_threads; ++i)
    //     v.push_back(std::thread{ write_to_file, i });
    //
    // for (auto& it : v)
    //     it.join();
    //
    // // std::cout << "Submited ios " << submited_ios.size() << std::endl;
    // // wait_complete();
    //
    // std::cout << "All threads joined\n";
    
    task_manager.execute_task<false>(write_thread1);
    task_manager.execute_task<false>(write_thread2);
    task_manager.execute_task<false>(write_thread3);
    task_manager.execute_task<false>(read_thread1);
    close(fd);
}

#endif

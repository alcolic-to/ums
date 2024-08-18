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
		// 	tls_worker->wait_event(e);
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
		// 	tls_worker->wait_event(e);
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
	// 	std::cout << prng.rand<uint64_t>() << "\n";

	cos_write_file(file, io_str.data(), io_str.size(), (prng.rand<uint64_t>() % max_file_size) * io_str.size());
}

int main(int argc, char* argv[])
{
	// for (int i = 0; i < 1024; ++i)
	// 	task_manager.execute_task<true>(write_to_file);
	// 
	// for (int i = 0; i < 1000 * 1024; ++i)
	// 	task_manager.execute_task<true>(read_from_file);

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
struct io_uring ring;
int ret = io_uring_queue_init(32, &ring, 0);
std::mutex io_mutex;

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

void wait_complete()
{
    // cqe is a pointer to the completion queue entry
    struct io_uring_cqe *cqe;
    std::vector<std::uint64_t> thread_ids;
    while (1)
    {
        // Peek to see if I/O completed
        std::uint32_t head;
        std::uint32_t i = 0;
        io_uring_for_each_cqe(&ring, head, cqe)
        {
            std::uint64_t tid = io_uring_cqe_get_data64(cqe);
            thread_ids.push_back(tid);
            i++;
        }

        io_uring_cq_advance(&ring, i);

        if (thread_ids.size() == 100)
        {
            break;
        }
        else
        {
            std::cout << "Number of completions is " << thread_ids.size() << std::endl;
        }

        sleep(1);
    }
}

std::vector<std::uint64_t> submited_ios;

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

    std::unique_lock<std::mutex> lock(io_mutex);

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
    ret = io_uring_submit(&ring);
    if (ret < 0)
    {
        std::cerr << "io_uring_submit failed" << std::endl;
        close(fd);
        free(buffer);
        io_uring_queue_exit(&ring);
        return;
    }

    submited_ios.push_back(thread_id);

    // Release the lock
    lock.unlock();

    free(buffer);
    close(fd);
}

#endif // __linux__

void sleep_test()
{
    cos_sleep(1000);
}

int main(int argc, char* argv[])
{
    create_file("io_testing_file_0", block_size * 100, 'a');
    std::vector<std::thread> v;
    const std::size_t num_threads = 100;

    for (std::size_t i = 0; i < num_threads; ++i)
        v.push_back(std::thread{ write_to_file, i });

    for (auto& it : v)
        it.join();

    std::cout << "Submited ios " << submited_ios.size() << std::endl;
    wait_complete();

    std::cout << "All threads joined\n";
}

#endif

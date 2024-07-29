#include <iostream>
#include <random>
#include <chrono>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

#include "worker.h"
#include "task_manager.h"

std::random_device rd;     // only used once to initialise (seed) engine
std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)

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

Event e;

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

void thread_function()
{
	for (int i = 0; i < 1000; ++i)
		task_manager.execute_task<false>(f3);
}

int main(int argc, char* argv[])
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

	std::cout << r << "\n";

	// std::vector<std::thread> v;
	// 
	// for (int i = 0; i < 10; ++i)
	// 	v.push_back(std::thread{ thread_function });
	// 
	// for (auto& it : v)
	// 	it.join();

	// std::this_thread::sleep_for(5s);
	// e.signal();

	// std::cout << f3() << "\n";
}

#include <iostream>
#include <random>
#include <chrono>
#include <exception>

#include "cos.h"

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
			std::cout << "Task execution exceeded time limit of " << duration.count() << "ms." << std::endl;
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

int f3()
{
	int first = 0, second = 1;
	
	// Fibbonaci seq.
	//
	for (int i = 2; i < 1000000000; ++i)
	{
		int sum = first + second;
		first = second;
		second = sum;

		// if (i % 10000 == 0)
		// 	tls_worker->wait_event(e);
	}

	return second;
}

int main(int argc, char* argv[])
{
	for (int i = 0; i < 1000; ++i)
	{
		task_manager.execute_task(Task{ f1 });
	}

	// std::this_thread::sleep_for(5s);
	// e.signal();

	// std::cout << f3() << std::endl;
}
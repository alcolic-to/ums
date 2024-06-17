#include <iostream>

// Random
#include <random>

#include "cos.h"

std::random_device rd;     // only used once to initialise (seed) engine
std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)

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
			std::cerr << "Task execution exceeded time limit of " << duration.count() << "ms." << std::endl;
			break;
		}

		if (v[rand() % v.size()] == rand() % v.size() && i++ % 100 == 0)
		{
			tls_worker->yield();
		}
	}

	return;
}

int main(int argc, char* argv[])
{
	for (int i = 0; i < 10000; ++i)
		taskManager.ExecuteTask(Task{ f1 });
}
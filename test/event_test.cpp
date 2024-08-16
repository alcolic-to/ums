#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <iostream>

#include "sync_api.h"
#include "util.h"
#include "task_manager.h"

ConditionalEvent e1;
ConditionalEvent e2;
ConditionalEvent e3;

#define ENDL '\n'

void f1()
{
    std::cout << "ENTERED f1" << ENDL;
    e1.wait();
    std::cout << "EXITED f1" << ENDL;
}

void f2()
{
    std::cout << "ENTERED f2" << ENDL;
    e2.wait();
    std::cout << "EXITED f2" << ENDL;
}

void f3()
{
    std::cout << "ENTERED f3" << ENDL;
    e3.wait();
    std::cout << "EXITED f3" << ENDL;
}

TEST(Event, SimpleTest)
{
    std::uint64_t start = get_time_in_ms();

    task_manager.execute_task<true>(f1);
    task_manager.execute_task<true>(f2);
    task_manager.execute_task<true>(f3);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 /* miliseconds */));
    e1.signal();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 /* miliseconds */));
    e2.signal();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 /* miliseconds */));
    e3.signal();

    std::uint64_t end = get_time_in_ms();
    
    std::cout << "Total exec time: " << end - start << "ms." << ENDL;
}

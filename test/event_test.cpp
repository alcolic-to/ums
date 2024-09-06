#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

#include "sync_api.h"
#include "task_manager.h"
#include "util.h"

ConditionalEvent e1;
ConditionalEvent e2;
ConditionalEvent e3;

#define ENDL '\n'

std::mutex m;
std::vector<std::uint8_t> v;

void f1()
{
    std::cout << "ENTERED f1" << ENDL;
    e1.wait();
    std::lock_guard<std::mutex> lock(m);
    v.push_back(1);
    std::cout << "EXITED f1" << ENDL;
}

void f2()
{
    std::cout << "ENTERED f2" << ENDL;
    e2.wait();
    std::lock_guard<std::mutex> lock(m);
    v.push_back(2);
    std::cout << "EXITED f2" << ENDL;
}

void f3()
{
    std::cout << "ENTERED f3" << ENDL;
    e3.wait();
    std::lock_guard<std::mutex> lock(m);
    v.push_back(3);
    std::cout << "EXITED f3" << ENDL;
}

TEST(Event, SimpleTest)
{
    task_manager.execute_task<true>(f1);
    task_manager.execute_task<true>(f2);
    task_manager.execute_task<true>(f3);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 /* miliseconds */));
    e2.signal();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 /* miliseconds */));
    e1.signal();

    std::this_thread::sleep_for(std::chrono::milliseconds(1000 /* miliseconds */));
    e3.signal();

    // Wait for the threads to finish.
    std::this_thread::sleep_for(std::chrono::milliseconds(2000 /* miliseconds */));

    std::lock_guard<std::mutex> lock(m);
    ASSERT_EQ(v.size(), 3);
    ASSERT_EQ(v[0], 2);
    ASSERT_EQ(v[1], 1);
    ASSERT_EQ(v[2], 3);
}

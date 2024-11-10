#include "ums.h"

#include <functional>
#include <memory>

// Thread local worker.
//
thread_local Worker* tls_worker; // NOLINT

// Global Schedulers.
//
std::unique_ptr<Schedulers> schedulers; // NOLINT

// Global task manager.
//
std::unique_ptr<Task_manager> task_manager; // NOLINT

// void init_ums(std::function<int(int, char**)>& main, int argc, char** argv)
void init_ums(const std::function<void()>& main)
{
    schedulers = std::make_unique<Schedulers>();
    task_manager = std::make_unique<Task_manager>(*schedulers);

    task_manager->execute_task<true>(main);
    schedulers->wait_exit();

    task_manager.reset();
    schedulers.reset();
}

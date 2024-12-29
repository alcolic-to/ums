#include "ums.h"

#include <functional>
#include <memory>
#include <utility>

#include "options.h"
#include "scheduler.h"

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
void init_ums(std::function<void()> main, Options opt)
{
    schedulers = std::make_unique<Schedulers>(opt);
    task_manager = std::make_unique<Task_manager>(*schedulers);

    task_manager->execute_task<true>(std::move(main));
    schedulers->wait_exit();

    task_manager.reset();
    schedulers.reset();
}

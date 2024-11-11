#pragma once

#ifndef COS_UMS_H
#define COS_UMS_H

#include <functional>
#include <memory>

#include "scheduler.h"
#include "task_manager.h"

// Global Schedulers.
//
extern std::unique_ptr<Schedulers> schedulers; // NOLINT

// Global task manager.
//
extern std::unique_ptr<Task_manager> task_manager; // NOLINT

// void init_ums(std::function<int(int, char**)>& main, int argc, char** argv);
void init_ums(std::function<void()> main);

#endif // COS_UMS_H

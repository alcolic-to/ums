#pragma once

#ifndef COS_UMS_H
#define COS_UMS_H

#include <functional>
#include <memory>

#include "scheduler.h"

// Global Schedulers.
//
extern std::unique_ptr<Schedulers> schedulers; // NOLINT

// void init_ums(std::function<int(int, char**)>& main, int argc, char** argv, Options opt =
// Options{});
void init_ums(std::function<void()> main, Options opt = Options{});

#endif // COS_UMS_H

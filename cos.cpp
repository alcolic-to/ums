#include "cpu.h"
#include "task_manager.h"
#include "worker.h"

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// Thread local worker pointer.
//
thread_local Worker* tls_worker;

// Global CPUs.
//
CPUs cpus;

// Global task manager.
//
Task_manager task_manager{cpus};

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#include "ums.h"

#include <functional>
#include <memory>
#include <utility>

#include "async.h"
#include "options.h"
#include "scheduler.h"

namespace ums {

// Thread local worker.
//
thread_local Worker* this_worker; // NOLINT

// Global Schedulers.
//
std::unique_ptr<Schedulers> schedulers; // NOLINT

// void init_ums(std::function<int(int, char**)>& main, int argc, char** argv)
void init_ums(std::function<void()> main, Options opt)
{
    schedulers = std::make_unique<Schedulers>(opt);

    async(std::move(main));

    schedulers->wait_exit();
    schedulers.reset();
}

} // namespace ums

#pragma once

#ifndef COS_SCHEDULER_H
#define COS_SCHEDULER_H

#include <algorithm>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <ranges>
#include <vector>

#include "config.h"
#include "options.h"
#include "task.h"
#include "worker.h"

namespace ums {

using Cpu_Mask = std::bitset<CFG_max_supported_cpus>;

class CPU final {
public:
    explicit CPU(uint64_t cpu_id) noexcept : m_id{cpu_id}, m_mask{Cpu_Mask{}.set(cpu_id)} {}

    uint64_t m_id;
    Cpu_Mask m_mask;
};

// Synchronization context for scheduler.
//
enum class Sync_context : int { main, yield, wait, io };

class Schedulers;

class Scheduler final {
    friend class Schedulers;
    friend class Worker;

public:
    enum class State : int { initializing, running, idle_wait, idle_sleep, exiting };

    explicit Scheduler(Schedulers& schedulers, uint64_t cpu_id,
                       Options::Workers_per_scheduler workers_count);
    ~Scheduler() noexcept;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&&) noexcept = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    bool has_idle_workers() const noexcept;
    bool has_runnable_workers() const noexcept;
    bool has_waiting_workers() const noexcept;
    bool has_pending_io_workers() const noexcept;
    bool has_yielded_workers() const noexcept;

    void park_runnable(Worker* worker);
    void park_waiting(Worker* worker);
    void park_pending_io(Worker* worker);
    void park_yielded(Worker* worker);

    template<bool back>
    void park_idle(Worker* worker);

    template<Sync_context ctx>
    void park_worker(Worker* worker);

    void prepare_next_worker() noexcept;

    void prepare_exec() noexcept;

    void set_worker_state(Worker* worker, Worker::State state) noexcept;

    void enqueue_task(std::shared_ptr<Task> task);
    std::shared_ptr<Task> next_task() noexcept;
    bool has_tasks() const noexcept;

    void schedule_idle_worker(std::shared_ptr<Task> task = nullptr);

    void schedule_io_workers();
    void schedule_waiting_workers();
    void schedule_idle_workers();
    void schedule_yielded_workers();
    void steal_work();
    void schedule_workers();

    void schedule();

    auto waiters_info() const noexcept;

    void invalidate_idle_timer() noexcept { m_idle_start_time = Time_point::max(); };

    bool should_idle_spin() noexcept;

    void sleep() noexcept;
    void notify();

    [[nodiscard]] bool initializing() const noexcept { return m_state == State::initializing; }

    [[nodiscard]] bool running() const noexcept { return m_state == State::running; }

    [[nodiscard]] bool idle() const noexcept { return m_state == State::idle_sleep; }

    [[nodiscard]] bool exiting() const noexcept { return m_state == State::exiting; }

    [[nodiscard]] Worker* worker() const noexcept { return m_worker; }

    void set_state(State state) noexcept;

    void manage_load(Worker::State prev_state, Worker::State new_state) noexcept;
    uint64_t load() const noexcept;

    void context_switch(Worker* prev_worker);

    bool sync_init(Worker* worker);

    template<Sync_context ctx>
    void sync(Worker* worker);

    void exit();

    [[nodiscard]] uint32_t workers_count() const noexcept { return m_workers.size(); }

    bool has_work() const noexcept;

    void signal_exit() noexcept { m_exit.store(true, std::memory_order_relaxed); }

    bool exit_signaled() const noexcept { return m_exit.load(std::memory_order_relaxed); }

    bool should_exit() const noexcept;

    [[nodiscard]] const Tasks& tasks() const noexcept { return m_tasks; }

    [[nodiscard]] uint64_t id() const noexcept { return m_cpu.m_id; }

private:
    void wait(std::unique_lock<std::mutex>& lock, Time_point abs_time = Time_point::max());
    void notify(const std::unique_lock<std::mutex>& lock) noexcept;

    Schedulers& m_schedulers;
    CPU m_cpu;
    Worker* m_worker{nullptr};
    std::deque<Worker*> m_runnable_queue;
    std::deque<Worker*> m_idle_queue;
    std::list<Worker*> m_waiting_queue;
    std::list<Worker*> m_pending_io_queue;
    Worker* m_yielded_worker{nullptr};

    std::mutex m_workers_mtx;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    Tasks m_tasks;
    State m_state{State::initializing};
    bool m_running{true}; // Flag used for spurious wakeup check.
    bool m_workers_started{false};

    std::atomic<uint64_t> m_load{0};
    std::atomic<bool> m_exit{false};

    Time_point m_idle_start_time{Time_point::max()};

    // TODO: Create workers in place next to each other.
    //
    std::vector<std::unique_ptr<Worker>> m_workers;
};

class Schedulers final {
    friend class Scheduler;

public:
    explicit Schedulers(Options opt = Options{}) noexcept;

    [[nodiscard]] Scheduler& min_load_scheduler() const noexcept;
    [[nodiscard]] Scheduler& max_load_scheduler() const noexcept;
    [[nodiscard]] uint32_t workers_count() const noexcept;
    [[nodiscard]] uint32_t cpus_count() const noexcept;

    [[nodiscard]] auto begin() const noexcept { return m_schedulers.begin(); }

    [[nodiscard]] auto end() const noexcept { return m_schedulers.end(); }

    // Returns filtered view of schedulers.
    // I like this so much, that I am not even going to look at performance costs.
    //
    template<typename Predicate>
    [[nodiscard]] auto filter(Predicate pred) const noexcept
    {
        return m_schedulers | std::ranges::views::filter(std::move(pred));
    }

    bool all_idle() noexcept;
    void signal_idle() noexcept;
    void signal_running() noexcept;
    void wait_exit();

private:
    uint32_t m_system_cpus_count;
    Cpu_Mask m_cpus_avail_mask;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    std::atomic<uint32_t> m_idle_schedulers{0};
    std::vector<std::unique_ptr<Scheduler>> m_schedulers;
};

} // namespace ums

#endif // COS_SCHEDULER_H

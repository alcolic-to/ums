#pragma once

#ifndef COS_SCHEDULER_H
#define COS_SCHEDULER_H

#include <atomic>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "config.h"
#include "spinlock.h"
#include "task_manager.h"
#include "worker.h" // This can be moved and class Worker can be forward declared if we dispose Worker::State.


using Cpu_Mask = std::bitset<CFG_max_cpu_count>;

class CPU final {
public:
    explicit CPU(uint64_t cpu_id) noexcept : m_id{cpu_id}, m_mask{Cpu_Mask{}.set(cpu_id)} {}

    uint64_t m_id;
    Cpu_Mask m_mask;
};

// Synchronization context for scheduler.
//
enum class SyncCtx : int { main, yield, wait_cond_or_sleep, io };

class Scheduler final {
    friend class Worker;

public:
    enum class State : int { initializing, running, idle, exiting };

    explicit Scheduler(uint64_t cpu_id);
    ~Scheduler() noexcept;

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    Scheduler(Scheduler&&) noexcept = delete;
    Scheduler& operator=(Scheduler&&) = delete;

    bool has_idle_workers() const noexcept;
    bool has_runnable_workers() const noexcept;
    bool has_waiting_workers() const noexcept;
    bool has_pending_io_workers() const noexcept;

    void save_runnable(Worker* worker);
    void save_waiting(Worker* worker);
    void save_pending_io(Worker* worker);

    template<bool back>
    void save_idle(Worker* worker);

    void prepare_next_worker() noexcept;

    void enqueue_task(const std::shared_ptr<Task>& task);
    std::shared_ptr<Task> next_task() noexcept;
    bool has_tasks() const noexcept;

    void schedule_idle_worker();

    void schedule_io_workers();
    void schedule_idle_workers();
    void schedule_waiting_workers();
    void schedule_sleeping_workers();
    void schedule_workers();

    void schedule();

    void idle_sleep() noexcept;
    void notify() noexcept;

    [[nodiscard]] bool initializing() const noexcept { return m_state == State::initializing; }

    [[nodiscard]] bool running() const noexcept { return m_state == State::running; }

    [[nodiscard]] bool idle() const noexcept { return m_state == State::idle; }

    [[nodiscard]] bool exiting() const noexcept { return m_state == State::exiting; }

    [[nodiscard]] Worker* worker() const noexcept { return m_worker; }

    void set_state(State state) noexcept;

    void manage_load(Worker::State prev_state, Worker::State new_state) noexcept;
    uint64_t load() const noexcept;

    auto waiters_info() const noexcept;

    void context_switch(Worker* prev_worker);

    template<SyncCtx ctx>
    void save_worker(Worker* worker);

    bool sync_init(Worker* worker);

    template<SyncCtx ctx>
    void sync(Worker* worker);

    void exit_workers();
    void exit();

    [[nodiscard]] uint32_t workers_count() const noexcept { return m_workers.size(); }

    bool has_work() const noexcept;

    void signal_exit() noexcept { m_exit.store(true, std::memory_order_relaxed); }

    bool exit_signaled() const noexcept { return m_exit.load(std::memory_order_relaxed); }

    bool should_exit() const noexcept;

    // Since we are only accessing size under lock, all memory operations can be
    // relaxed, because Spinlock has acquire-release memory order.
    //
    class Tasks {
    public:
        void enque(const std::shared_ptr<Task>& task)
        {
            const std::scoped_lock<Spinlock> l{m_lock};

            m_tasks.push_back(task);
            m_size.fetch_add(1, std::memory_order_relaxed);
        }

        // This function is only executed by scheduler (single threaded), so
        // caller just needs to check once if there are tasks and call this function.
        //
        std::shared_ptr<Task> deque() noexcept
        {
            const std::scoped_lock<Spinlock> l{m_lock};

            std::shared_ptr<Task> t{std::move(m_tasks.front())};
            m_tasks.pop_front();

            m_size.fetch_sub(1, std::memory_order_relaxed);
            return t;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_size.load(std::memory_order_relaxed);
        }

        [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    private:
        // TODO: Create nonaligned spinlock for this and align whole struct on a cache line.
        //
        Spinlock m_lock;
        std::deque<std::shared_ptr<Task>> m_tasks;
        std::atomic<std::size_t> m_size{0};
    };

private:
    CPU m_cpu;
    Worker* m_worker{nullptr};
    std::deque<Worker*> m_runnable_queue;
    std::deque<Worker*> m_idle_queue;
    std::list<Worker*> m_waiting_queue;
    std::list<Worker*> m_pending_io_queue;

    std::mutex m_workers_mtx;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    Tasks m_tasks;
    State m_state{State::initializing};
    bool m_workers_started{false};
    uint64_t m_load{0};
    std::atomic<bool> m_exit{false};

    // TODO: Create workers in place next to each other.
    //
    std::vector<std::unique_ptr<Worker>> m_workers;
};

class Schedulers final {
public:
    Schedulers() noexcept;

    [[nodiscard]] Scheduler& min_load_scheduler() const noexcept;
    [[nodiscard]] uint32_t workers_count() const noexcept;
    [[nodiscard]] uint32_t cpus_count() const noexcept;

private:
    uint32_t m_system_cpus_count;
    Cpu_Mask m_avail_cpus_mask;
    std::vector<std::unique_ptr<Scheduler>> m_schedulers;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern Schedulers schedulers;

#endif // COS_SCHEDULER_H

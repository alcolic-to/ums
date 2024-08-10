#pragma once

#include <cstdint>

class Event
{
    public:
        using Base = Event;

        Event() = default;
        virtual void wait();
        virtual void signal() = 0;
        virtual bool check() const = 0;
};

class ConditionalEvent final : public Event
{
    public:
        ConditionalEvent();
        virtual void wait() override;
        virtual void signal() override;
        virtual bool check() const override;
    private:
        bool m_cond;
};

class TimedEvent final : public Event
{
    public:
        TimedEvent(std::uint64_t time_to_sleep_in_ms);
        virtual void wait() override;
        virtual void signal() override;
        virtual bool check() const override;
    private:
        std::uint64_t m_start_time;
        std::uint64_t m_time_to_sleep_in_ms;
};

void cos_sleep(std::uint32_t miliseconds);

#pragma once

#ifndef COS_IO_API_H
#define COS_IO_API_H

#include <cstdint>

struct IO_Buffer {
    void* m_buffer;
    uint64_t m_size;
};

struct Overlapped final {
    unsigned long long m_internal, m_internal_high;

    union {
        struct {
            unsigned long m_offset, m_offset_high;
        };

        void* m_ptr;
    };

    void* m_event;
};

struct IO_Control final {
public:
    explicit IO_Control(uint64_t offset) : m_ol{}
    {
        constexpr uint32_t ON_BITS_32 = 0xFFFFFFFF;
        constexpr uint8_t HIGH_BITS_OFFSET = 32;

        m_ol.m_offset = offset & ON_BITS_32;                            // NOLINT
        m_ol.m_offset_high = (offset >> HIGH_BITS_OFFSET) & ON_BITS_32; // NOLINT
    }

    Overlapped m_ol;
};

class IO_Request final {
public:
    enum class Type : int { read, write };
    enum class State : int { init, error, pending, completed };

    IO_Request(void* file_handle, IO_Buffer buffer, uint64_t offset, Type type);

    bool completed() const { return m_state == State::completed; }

    bool pending() const { return m_state == State::pending; }

    bool error() const { return m_state == State::error; }

    void update();

    void* m_file_handle;
    IO_Buffer m_io_buffer;
    uint64_t m_offset;
    IO_Control m_control;
    Type m_type;
    State m_state;
};

void cos_read_file(void* file_handle, IO_Buffer buffer, uint64_t offset);
void cos_write_file(void* file_handle, IO_Buffer buffer, uint64_t offset);

#endif // COS_IO_API_H

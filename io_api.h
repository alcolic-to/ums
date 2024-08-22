#pragma once

#ifndef COS_IO_API_H
#define COS_IO_API_H

#include <cstdint>

#include <cstdint>

class IO_Request final
{
public:
    enum class Type : int { read, write };
    enum class State : int { init, error, pending, completed };

    IO_Request(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset, Type type);
    ~IO_Request();

    bool completed() const { return m_state == State::completed; }
    void set_completed() { m_state = State::completed; }
    bool pending() const { return m_state == State::pending; }
    void set_pending() { m_state = State::pending; }
    bool error() const { return m_state == State::error; }
    void set_error() { m_state = State::error; }
    void update();

    void* m_file_handle;
    void* m_buffer;
    uint64_t m_nbytes;
    uint64_t m_offset;

    void* m_io_handle;

    Type m_type;
    State m_state;
};


void cos_read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);
void cos_write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);

#endif // COS_IO_API_H

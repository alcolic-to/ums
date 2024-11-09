#include <cstdint>

#include "io_api.h"
#include "os_specific.h"
#include "worker.h"

IO_Request::IO_Request(void* file_handle, IO_Buffer buffer, uint64_t offset, Type type) noexcept
    : m_file_handle{file_handle}
    , m_io_buffer{buffer}
    , m_offset{offset}
    , m_control{offset}
    , m_type{type}
    , m_state{State::init}
{
    if (m_type == Type::read)
        os::read_file(*this);
    else
        os::write_file(*this);
}

void IO_Request::update() noexcept
{
    os::update_io_state(*this);
}

void cos_read_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    tls_worker->read_file(file_handle, buffer, offset);
}

void cos_write_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    tls_worker->write_file(file_handle, buffer, offset);
}

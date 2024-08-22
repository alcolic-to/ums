#include <future>

#include "io_api.h"
#include "worker.h"
#include "os_specific.h"

// Constructor that issues async I/O (win32).
//
IO_Request::IO_Request(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset, Type type)
    : m_file_handle{ file_handle }
    , m_file_stream{ nullptr }
    , m_buffer{ buffer }
    , m_nbytes{ nbytes }
    , m_offset{ offset }
    , m_control{ offset }
    , m_type{ type }
    , m_state{ State::init }
{
    issue_io_with_handle(*this);
}

// Constructor that issues async I/O without native async I/O support.
// I/O is issued asynchronously in separate thread with sync I/O function (std::fstream::write).
//
IO_Request::IO_Request(std::fstream* file_stream, void* buffer, uint64_t nbytes, uint64_t offset, Type type)
    : m_file_handle{ nullptr }
    , m_file_stream{ file_stream }
    , m_buffer{ buffer }
    , m_nbytes{ nbytes }
    , m_offset{ offset }
    , m_control{ offset }
    , m_type{ type }
    , m_state{ State::init }
    , m_future{ std::async(std::launch::async, issue_io_with_stream, std::ref(*this)) }
{ }

// Updates I/O state. If we've issued async I/O, we need to check manually whether I/O is done.
// For streamed I/Os, we are issuing I/O with sync write function, so there is no need to check
// it's completion state, since it is already set in issue_io_with_stream.
//
void IO_Request::update()
{
    if (!streamed())
        update_io_state(*this);
}

void cos_read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset)
{
    tls_worker->issue_io(file_handle, buffer, nbytes, offset, IO_Request::Type::read);
}

void cos_write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset)
{
    tls_worker->issue_io(file_handle, buffer, nbytes, offset, IO_Request::Type::write);
}

void cos_read_file(std::fstream& file_stream, void* buffer, uint64_t nbytes, uint64_t offset)
{
    tls_worker->issue_io(&file_stream, buffer, nbytes, offset, IO_Request::Type::read);
}

void cos_write_file(std::fstream& file_stream, void* buffer, uint64_t nbytes, uint64_t offset)
{
    tls_worker->issue_io(&file_stream, buffer, nbytes, offset, IO_Request::Type::write);
}

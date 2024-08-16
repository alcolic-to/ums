#include "io_api.h"
#include "worker.h"

IO_Request::IO_Request(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset, Type type)
	: m_file_handle{ file_handle }
	, m_buffer{ buffer }
	, m_nbytes{ nbytes }
	, m_offset{ offset }
	, m_control{ offset }
	, m_type{ type }
	, m_state{ State::init }
{
	if (m_type == Type::read)
		read_file(*this);
	else
		write_file(*this);
}

void IO_Request::update()
{
	update_io_state(*this);
}

void read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset)
{
	tls_worker->read_file(file_handle, buffer, nbytes, offset);
}

void write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset)
{
	tls_worker->write_file(file_handle, buffer, nbytes, offset);
}

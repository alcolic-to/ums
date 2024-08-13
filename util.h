#pragma once

#ifndef COS_UTIL_H
#define COS_UTIL_H

#include <cstdint>

#include "os_specific.h"

class IO_Request final
{
public:
	enum class Type : int { read, write };
	enum class State : int { init, error, pending, completed };

	IO_Request(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset, Type type);

	bool completed() const { return m_state == State::completed; }
	bool pending() const { return m_state == State::pending; }
	bool error() const { return m_state == State::error; }
	void update();

	void* m_file_handle;
	void* m_buffer;
	uint64_t m_nbytes;
	uint64_t m_offset;
	IO_Control m_control;
	Type m_type;
	State m_state;
};

void read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);
void write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);

#endif // COS_UTIL_H
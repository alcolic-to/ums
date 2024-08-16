#pragma once

#ifndef COS_IO_H
#define COS_IO_H

#include <cstdint>

struct Overlapped final
{
    unsigned long long m_internal, m_internal_high;
    union
    {
        struct { unsigned long m_offset, m_offset_high; };
        void* m_ptr;
    };

    void* m_event;
};

struct IO_Control final
{
public:
	IO_Control(uint64_t offset) : m_ol{}
	{
		m_ol.m_offset = offset & 0xFFFFFFFF;
		m_ol.m_offset_high = (offset >> 32) & 0xFFFFFFFF;
	}

    Overlapped m_ol;
};

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

#endif // COS_IO_H

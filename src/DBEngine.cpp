// DBEngine.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <vector>
#include <windows.h>
#include <bitset>
#include <iostream>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>

constexpr int BUF_SIZE = 1024 * 8; // 8KB per buffer
constexpr int INIT_BUF_COUNT = 8; // buffers per buffer pool

class BufferPool
{
	class Buffer
	{
		struct Header
		{
			uint8_t m_type = 0;
			uint8_t m_occupation = 0; // buffer occupation in percentage.
			uint16_t m_freespace = 0; // buffer occupation in percentage.
		};

	private:
		Header m_header;
		unsigned char m_buf[BUF_SIZE - sizeof(Header)] = { 0 }; // physical memory buffer
	};

	static_assert(sizeof(Buffer) == BUF_SIZE, "Invalid buffer size.");

public:
	BufferPool()
	{
		m_buffers.reserve(m_bufcount);

		for (int i = 0; i < m_bufcount; ++i)
			m_buffers.emplace_back(Buffer{});
	}

private:
	int m_bufcount = INIT_BUF_COUNT;
	std::vector<Buffer> m_buffers;
};

BufferPool BP;


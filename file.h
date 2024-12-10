#pragma once

#ifndef COS_FILE_H
#define COS_FILE_H

#include <filesystem>

#include "os_specific.h"

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
#undef min
#undef max

constexpr int64_t x_file_access = GENERIC_READ | GENERIC_WRITE;
constexpr int64_t x_file_attributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS |
                                 FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING |
                                 FILE_FLAG_WRITE_THROUGH;
#elif __linux__
constexpr int64_t x_file_access = O_CREAT | O_RDWR | O_DIRECT;
constexpr int64_t x_file_attributes = 0666;
#endif

// RAII file handle wrapper
class File_handle {
public:
    // Constructor opens file handle
    File_handle(const fs::path& file_path)
        : m_handle{os::open_file(file_path.string().c_str(), x_file_access, x_file_attributes)}
    {
    }

    // Destructor closes file handle
    ~File_handle() { os::close_file(m_handle); }

    // Conversion operator to void*
    operator void*() { return m_handle; }

private:
    void* m_handle;
};

#endif // COS_FILE_H

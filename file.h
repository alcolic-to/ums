#pragma once

#ifndef COS_FILE_H
#define COS_FILE_H

#include <filesystem>

namespace fs = std::filesystem;

#ifdef _WIN32
#include <windows.h>
#undef min
#undef max

constexpr int64_t x_file_access = GENERIC_READ | GENERIC_WRITE;
constexpr int64_t x_file_attributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS |
                                 FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING |
                                 FILE_FLAG_WRITE_THROUGH;
#endif

// RAII file handle wrapper
class File_handle {
public:
    File_handle(const fs::path& file_path);
    ~File_handle();

    // Conversion operator to void*
    operator void*() { return m_handle; }

private:
    void* m_handle;
};

#endif // COS_FILE_H

#pragma once

#ifndef COS_FILE_H
#define COS_FILE_H

#include <filesystem>

namespace fs = std::filesystem;

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

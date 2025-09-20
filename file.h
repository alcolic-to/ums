#pragma once

#ifndef COS_FILE_H
#define COS_FILE_H

#include <filesystem>

namespace ums {

namespace fs = std::filesystem;

// RAII file handle wrapper
class File_handle {
public:
    explicit File_handle(const fs::path& file_path);
    ~File_handle();

    File_handle(const File_handle&) = delete;
    File_handle& operator=(const File_handle&) = delete;

    File_handle(File_handle&&) noexcept = delete;
    File_handle& operator=(File_handle&&) = delete;

    // Conversion operator to void*
    operator void*() { return m_handle; } // NOLINT

private:
    void* m_handle;
};

} // namespace ums

#endif // COS_FILE_H

/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef UMS_FILE_HPP
#define UMS_FILE_HPP

#include <filesystem>

namespace ums {

namespace fs = std::filesystem;

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

#endif // UMS_FILE_HPP

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

#ifndef UMS_OS_SPECIFIC_HPP
#define UMS_OS_SPECIFIC_HPP

#include "types.hpp"

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_UNKNOWN
#endif

namespace ums {

class IO_Request;

#ifdef OS_LINUX
struct io_uring;
#endif

namespace os {

#if defined OS_WINDOWS

struct Overlapped final {
    unsigned long long m_internal, m_internal_high;

    union {
        struct {
            unsigned long m_offset, m_offset_high;
        };

        void* m_ptr;
    };

    void* m_event;
};

struct IO_handle final {
    explicit IO_handle(u64 offset);

    Overlapped* get_ol_ptr() { return &m_ol; }

    Overlapped m_ol;
};

#elif defined OS_LINUX

struct IO_handle final {
    explicit IO_handle(u64 offset);
    io_uring* m_uring;
    u64 m_id;
};

#endif

// CPU and thread related functions.
//
u32 cpus_count() noexcept; // maybe just std::thread::hardware_concurrency?
u64 cpus_avail_mask() noexcept;
void bind_thread(u64 cpu_mask) noexcept;
void print_thread_affinity() noexcept;

// I/O functions.
//
void read_file(IO_Request& io) noexcept;
void write_file(IO_Request& io) noexcept;
void update_io_state(IO_Request& io) noexcept;

// File functions.
//
void* open_file(const char* file_path, u64 flags, u64 mode);
void close_file(void* file_handle);

} // namespace os

} // namespace ums

#endif // UMS_OS_SPECIFIC_HPP

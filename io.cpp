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
#include <cstdint>
#include <memory>

#include "io_api.h"
#include "os_specific.h"
#include "worker.h"

namespace ums {

IO_Request::IO_Request(void* file_handle, IO_Buffer buffer, uint64_t offset, Type type) noexcept
    : m_file_handle{file_handle}
    , m_io_buffer{buffer}
    , m_offset{offset}
    , m_io_handle{std::make_unique<os::IO_handle>(offset)}
    , m_type{type}
    , m_state{State::init}
{
    if (m_type == Type::read)
        os::read_file(*this);
    else
        os::write_file(*this);
}

void IO_Request::update() noexcept
{
    os::update_io_state(*this);
}

void cos_read_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    this_worker->read_file(file_handle, buffer, offset);
}

void cos_write_file(void* file_handle, IO_Buffer buffer, uint64_t offset)
{
    this_worker->write_file(file_handle, buffer, offset);
}

} // namespace ums

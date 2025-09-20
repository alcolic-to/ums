#pragma once

#ifndef COS_IO_API_H
#define COS_IO_API_H

#include <cstdint>
#include <memory>

namespace ums {

// Forward declaration.
namespace os {
struct IO_handle;
}

struct IO_Buffer {
    void* m_buffer;
    uint64_t m_size;
};

class IO_Request final {
public:
    enum class Type : int { read, write };
    enum class State : int { init, error, pending, completed };

    IO_Request(void* file_handle, IO_Buffer buffer, uint64_t offset, Type type) noexcept;

    [[nodiscard]] bool completed() const noexcept { return m_state == State::completed; }

    [[nodiscard]] bool pending() const noexcept { return m_state == State::pending; }

    [[nodiscard]] bool error() const noexcept { return m_state == State::error; }

    void update() noexcept;

    void* m_file_handle;
    IO_Buffer m_io_buffer;
    uint64_t m_offset;
    std::unique_ptr<os::IO_handle> m_io_handle;
    Type m_type;
    State m_state;
};

void cos_read_file(void* file_handle, IO_Buffer buffer, uint64_t offset);
void cos_write_file(void* file_handle, IO_Buffer buffer, uint64_t offset);

} // namespace ums

#endif // COS_IO_API_H

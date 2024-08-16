#pragma once

#ifndef COS_IO_API_H
#define COS_IO_API_H

#include <cstdint>

void cos_read_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);
void cos_write_file(void* file_handle, void* buffer, uint64_t nbytes, uint64_t offset);

#endif // COS_IO_API_H

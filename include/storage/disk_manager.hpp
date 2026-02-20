#pragma once

#include <cstdlib>

constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024; // 4 MB
constexpr size_t IO_ALIGN = 4096;

constexpr inline size_t AlignUp(const size_t value, const size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

int ReadCF(const char *file_name, char *buffer, size_t size, size_t offset = 0);

int WriteCF(const char *file_name, const char *buffer, size_t size);
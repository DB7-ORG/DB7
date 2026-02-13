#pragma once

#include <cstdlib>

constexpr size_t BLOCK_SIZE = 4 * 1024 * 1024; // 4 MB
constexpr size_t IO_ALIGN = 4096;

constexpr size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

int readCF(const char *file_name, char *buffer, size_t size, size_t offset = 0);

int writeCF(const char *file_name, const char *buffer, size_t size);
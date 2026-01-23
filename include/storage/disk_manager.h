#ifndef DISK_MANAGER_H
#define DISK_MANAGER_H

#include <cstdlib>

#define BLOCK_SIZE 4 * 1024 * 1024 // 4 MB

int readCF(const char *file_name, char *buffer, size_t size, size_t offset = 0);

int writeCF(const char *file_name, const char *buffer, size_t size);

#endif
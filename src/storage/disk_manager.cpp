#include "storage/disk_manager.hpp"
#include "helper_utils.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

int ReadCF(const char *file_name, char *buffer, size_t size, size_t offset)
{
    int fd = open(file_name, O_RDONLY | O_DIRECT);
    if (fd == -1)
    {
        perror("Error opening file");
        throw std::runtime_error("error opening file");
        return 0;
    }

    size_t total = 0;
    while (total < size)
    {
        size_t bytes_to_read = min((size_t)BLOCK_SIZE, size - total);
        ssize_t n = pread(fd, buffer + total, bytes_to_read, offset + total);

        if (n < 0)
        {
            close(fd);
            perror("Error pread failed");
            throw std::runtime_error("error pread file");
            return 0;
        }

        if (n == 0) // EOF
        {
            break;
        }

        total += n;
    }

    close(fd);
    return total;
}

int WriteCF(const char *file_name, const char *buffer, size_t size)
{
    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    if (fd == -1)
    {
        perror("Error opening file");
        throw std::runtime_error("error opening file");
        return 0;
    }

    size_t total = 0;
    while (total < size)
    {
        size_t bytes_to_write = min((size_t)BLOCK_SIZE, AlignUp(size - total, IO_ALIGN));
        ssize_t n = write(fd, (char *)buffer + total, bytes_to_write);
        if (n <= 0)
        {
            close(fd);
            perror("Error read failed");
            throw std::runtime_error("error read file");
            return 0;
        }
        total += n;
    }

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        perror("Error ftruncate failed");
        throw std::runtime_error("error truncate file");
        return 0;
    }

    close(fd);
    return total;
}
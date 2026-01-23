#include "storage/disk_manager.h"
#include "../shared/helper_utils.h"
#include "storage_constants.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

int read(const char *file_name, char *buffer, size_t size, size_t offset)
{
    int fd = open(file_name, O_RDONLY | O_DIRECT);
    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
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
            return 2;
        }

        if (n == 0) // EOF
        {
            break;
        }

        total += n;
    }

    close(fd);
    return 0;
}

int write(const char *file_name, const char *buffer, size_t size)
{
    int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    if (fd == -1)
    {
        perror("Error opening file");
        return 1;
    }

    size_t total = 0;
    while (total < size)
    {
        ssize_t n = write(fd, (char *)buffer + total, BLOCK_SIZE);
        if (n <= 0)
        {
            close(fd);
            perror("Error read failed");
            return 2;
        }
        total += n;
    }

    if (ftruncate(fd, size) < 0)
    {
        close(fd);
        perror("Error ftruncate failed");
        return 3;
    }

    close(fd);
    return 0;
}
#include <iostream>
#include "storage/disk_manager.h"

int main()
{
    char *buffer = (char *)malloc(BLOCK_SIZE);
    int offset = 0;
    for (int i = 0; i < 1000; i++)
    {
        *(int *)(buffer + offset) = i;
        offset += sizeof(int);
    }

    const char *filename = "resources/some.bin";

    if (write(filename, buffer, BLOCK_SIZE))
    {
        return 1;
    }

    char *new_buffer = (char *)malloc(BLOCK_SIZE);
    if (read(filename, new_buffer, BLOCK_SIZE))
    {
        return 2;
    }

    offset = 0;
    for (int i = 0; i < 1000; i++)
    {
        std::cout << *(int *)(new_buffer + offset) << " - ";
        offset += sizeof(int);
    }

    free(buffer);
    free(new_buffer);

    return 0;
}
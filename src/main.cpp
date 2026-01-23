#include <iostream>
#include "storage/disk_manager.h"
#include "storage/compressions/compression.h"

int test_read_write()
{
    int tuple_num = 1024;
    int size = tuple_num * sizeof(int);
    int *buffer = (int *)malloc(size);

    for (int i = 0; i < tuple_num; i++)
    {
        buffer[i] = i % 5;
    }

    const char *filename = "resources/some.bin";

    if (writeCF(filename, (char *)buffer, size))
    {
        return 1;
    }

    char *new_buffer = (char *)malloc(size);
    if (readCF(filename, new_buffer, size))
    {
        return 2;
    }

    auto val = dictionaryEncode(new_buffer, size, sizeof(int));

    for (int i = 0; i < (int)val.num_items; i++)
    {
        std::cout << val.encoded[i] << " - ";
    }

    free(buffer);
    free(new_buffer);
    return 0;
}

int main()
{
    test_read_write();
    return 0;
}
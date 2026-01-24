#include <iostream>
#include "storage/disk_manager.h"
#include "storage/compressions/compression.h"
#include <random>
#include <string.h>

std::mt19937 gen(42);
std::uniform_int_distribution<> status_distribution(1, 8);

int test_read_write()
{
    const long tuple_num = 512 * 100'000;
    const auto strsize = (uint16_t)sizeof("string1");
    long size = tuple_num * (strsize + 2);
    const char *filename = "resources/some.bin";

    // char *buffer = (char *)malloc(size);
    // int offset = 0;
    // for (int i = 0; i < tuple_num; i++)
    // {
    //     std::string str = "string" + std::to_string(status_distribution(gen));
    //     memcpy(buffer + offset, &strsize, sizeof(uint16_t));
    //     memcpy(buffer + offset + 2, str.c_str(), strsize);
    //     offset += strsize + 2;
    // }

    // if (!writeCF(filename, (char *)buffer, size))
    // {
    //     return 1;
    // }

    // free(buffer);

    char *new_buffer = (char *)malloc(size);
    int read = readCF(filename, new_buffer, size);
    if (!read)
    {
        return 2;
    }

    auto val = dictionaryEncodeString((char *)new_buffer, read, tuple_num);

    for (int i = 0; i < 20; i++)
    {
        std::cout << val.encoded[i] << " - ";
    }

    free(new_buffer);
    return 0;
}

int main()
{
    test_read_write();
    return 0;
}
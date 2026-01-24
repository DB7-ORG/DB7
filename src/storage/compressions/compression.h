#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // uint32_t, uint16_t, int32_t, etc.
#include <cstddef>

struct DictEncodedRes
{
    uint32_t *encoded;
    char *strings;
    size_t string_size;
    size_t count;
};

DictEncodedRes dictionaryEncodeString(char *buffer, size_t byte_size, size_t count);

#endif
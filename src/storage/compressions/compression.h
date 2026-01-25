#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <cstdint> // uint32_t, uint16_t, int32_t, etc.
#include <cstddef>

struct DictEncodedRes
{
    uint32_t *encoded;
    uint32_t *indexes;
    char *strings;
    size_t unique_str;
    size_t count;
};

struct DictionaryEncoder
{
    static DictEncodedRes encode(char *buffer, size_t byte_size, size_t count);
    // static void decode();
};
#endif
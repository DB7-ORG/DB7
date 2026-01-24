#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <xxhash.h>
#include <string.h>

struct MapEntry
{
    uint32_t hash;
    uint32_t value;
    uint16_t key_len;
    char *key;
};

DictEncodedRes dictionaryEncodeString(char *buffer, size_t byte_size, size_t count)
{
    uint32_t *encoded = (uint32_t *)malloc(count * sizeof(uint32_t));

    char *strings = (char *)malloc(byte_size); // TODO OK but may over-allocate
    uint32_t s_off = 0;

    size_t hash_capacity = count * 2; // Use 2x for good performance
    MapEntry *entries = (MapEntry *)calloc(hash_capacity, sizeof(MapEntry));

    uint32_t offset = 0;
    for (size_t i = 0; i < count; i++)
    {
        uint16_t len = *(uint16_t *)(buffer + offset);
        offset += 2;
        char *key = buffer + offset;
        offset += len;

        uint32_t hash = XXH32(key, len, 0);
        uint32_t bucket = hash % hash_capacity;

        while (true)
        {
            MapEntry data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

            if (data.key != NULL &&
                data.hash == hash &&
                data.key_len == len &&
                memcmp(data.key, key, len) == 0)
            { // match
                encoded[i] = data.value;
                break;
            }
            else if (data.key == NULL)
            { // empty slot
                entries[bucket] = MapEntry{
                    hash,
                    s_off,
                    len,
                    key};
                memcpy(strings + s_off, key, len);
                encoded[i] = s_off;
                s_off += len;
                break;
            }
            bucket = (bucket + 1) % hash_capacity;
        }
    }

    return DictEncodedRes{
        encoded,
        strings,
        s_off,
        count};
}

// template <typename T>
// int decode()
// {
// }

#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <xxhash.h>
#include <string.h>

struct MapEntry
{
    uint32_t hash;
    uint32_t value;
    char *key;
    uint16_t key_len;
};

struct HMap
{
    MapEntry *entries;
    size_t hash_capacity;

    HMap(size_t count);
    ~HMap();
    uint32_t get_insert(char *key, uint16_t len, uint32_t value);
};

#include <sys/mman.h>
HMap::HMap(size_t count)
{
    hash_capacity = count * 2; // Use 2x for good performance
    entries = (MapEntry *)calloc(hash_capacity, sizeof(MapEntry));

    // (MapEntry *)mmap(
    //     NULL,
    //     hash_capacity * sizeof(MapEntry),
    //     PROT_READ | PROT_WRITE,
    //     MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, // Pre-fault pages
    //     -1, 0);

    // entries = (MapEntry *)malloc(hash_capacity * sizeof(MapEntry));
    // memset(entries, 0, hash_capacity * sizeof(MapEntry));
}

HMap::~HMap()
{
    free(entries);
}

inline uint32_t HMap::get_insert(char *key, uint16_t len, uint32_t value)
{
    uint32_t hash = XXH32(key, len, 0);
    uint32_t bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry &data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

        if (data.key == NULL)
        { // empty slot
            data = MapEntry{
                hash,
                value,
                key,
                len};

            return value;
        }
        else if (data.key != NULL &&
                 data.hash == hash &&
                 data.key_len == len &&
                 memcmp(data.key, key, len) == 0)
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}

DictEncodedRes DictionaryEncoder::encode(char *buffer, size_t byte_size, size_t count)
{
    uint32_t *encoded = (uint32_t *)malloc(count * sizeof(uint32_t));

    uint32_t *indexes = (uint32_t *)malloc(count * sizeof(uint32_t)); // TODO OK but may over-allocate
    indexes[0] = 0;
    uint32_t idx = 1;

    char *strings = (char *)malloc(byte_size); // TODO OK but may over-allocate

    HMap map(count);

    uint32_t offset = 0;
    for (size_t i = 0; i < count; i++)
    {
        uint16_t len = *(uint16_t *)(buffer + offset);
        offset += 2;
        char *key = buffer + offset;
        offset += len;

        uint32_t data = map.get_insert(key, len, idx);
        encoded[i] = data;
        if (data == idx)
        {
            indexes[idx] = indexes[idx - 1] + len;
            memcpy(strings + indexes[idx - 1], key, len);
            //*(uint64_t *)(strings + offset) = *(uint64_t *)key;
            idx++;
        }
    }

    return DictEncodedRes{
        encoded,
        indexes,
        strings,
        idx,
        count};
}

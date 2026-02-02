#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <xxhash.h>
#include <string.h>

struct MapEntry
{
    u32 hash;
    u32 value;
    u8 *key;
    u16 key_len;
};

struct HMap
{
    MapEntry *entries;
    size_t hash_capacity;

    HMap(size_t count);
    ~HMap();
    u32 get_insert(u8 *key, u16 len, u32 value);
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

inline u32 HMap::get_insert(u8 *key, u16 len, u32 value)
{
    u32 hash = XXH32(key, len, 0);
    u32 bucket = hash & (hash_capacity - 1);

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

DictEncodedRes DictionaryEncoder::encode(size_t count, u8 **in, size_t *lenIn, u32 *out)
{
    // u32 *encoded = (u32 *)malloc(count * sizeof(u32));

    char *strings = (char *)malloc(count * sizeof(char *));
    u32 *indexes = (u32 *)malloc(count * sizeof(u32));
    indexes[0] = 0;
    u32 idx = 1;

    HMap map(count);

    for (size_t i = 0; i < count; i++)
    {
        u16 len = lenIn[i];
        u8 *key = in[i];
        u32 data = map.get_insert(key, len, idx);
        out[i] = data;
        if (data == idx)
        {
            indexes[idx] = indexes[idx - 1] + len;
            memcpy(strings + indexes[idx - 1], key, len);
            //*(u64 *)(strings + offset) = *(u64 *)key;
            idx++;
        }
    }

    return DictEncodedRes{
        out,
        indexes,
        strings,
        idx,
        count};
}

#ifndef APPEND_HMAP_H
#define APPEND_HMAP_H

#include "common.h"
#include <xxhash.h>
#include <string.h>

template <typename ValueType>
struct MapEntry
{
    u32 hash;
    u32 value;
    ValueType key;
};

template <typename ValueType>
struct HMap
{
    MapEntry<ValueType> *entries;
    u32 hash_capacity;

    HMap(u32 count);
    ~HMap();
    u32 get_insert(ValueType key, u32 value, u16 len = sizeof(ValueType));
};

template <typename ValueType>
HMap<ValueType>::HMap(u32 count)
{
    u32 target = count * 2;
    if (target == 0)
    {
        hash_capacity = 1;
    }
    else if (target == 1)
    {
        hash_capacity = 1;
    }
    else
    {
        hash_capacity = 1u << (32 - __builtin_clz(target - 1));
    }

    entries = (MapEntry<ValueType> *)calloc(hash_capacity, sizeof(MapEntry<ValueType>));
}

template <typename ValueType>
HMap<ValueType>::~HMap()
{
    free(entries);
}

template <typename ValueType>
u32 calc_hash(ValueType key)
{
    if constexpr (sizeof(ValueType) <= 4)
    {
        // u8, u16, u32 - simple multiply hash, extremely fast
        return (u32)key * 2654435761u; // Knuth multiplicative hash
    }
    else
    {
        // u64 - mix both halves
        u64 k = (u64)key;
        return (u32)((k * 11400714819323198485ull) >> 33); // Fibonacci hashing
    }
}

template <typename ValueType>
inline u32 HMap<ValueType>::get_insert(ValueType key, u32 value, u16 len)
{
    u32 hash = calc_hash(key);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry<ValueType> &data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

        if (data.key == 0)
        { // empty slot
            data = MapEntry<ValueType>{
                hash,
                value,
                key};

            return value;
        }
        else if (data.key != 0 &&
                 data.hash == hash &&
                 data.key == key)
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}

// .
// String implementation
// .

// Specialization for strings
template <>
struct MapEntry<u8 *>
{
    u32 hash;
    u32 value;
    u8 *key;
    u16 key_len;
};

template <>
inline u32 HMap<u8 *>::get_insert(u8 *key, u32 value, u16 len)
{
    u32 hash = XXH32(key, len, 0);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry<u8 *> &data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

        if (data.key == NULL)
        { // empty slot
            data = MapEntry<u8 *>{
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

#endif
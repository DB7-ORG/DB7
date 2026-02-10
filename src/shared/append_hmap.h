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
    u32 get_insert(ValueType key, u32 value);
};

template <typename ValueType>
HMap<ValueType>::HMap(u32 count)
{
    u32 target = count * 2;
    if (target == 0 || target == 1)
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

struct StringKey
{
    u8 *ptr;
    u16 len;
};

template <typename ValueType>
inline bool key_equal(MapEntry<ValueType> data, ValueType key, u32 hash)
{
    if constexpr (std::is_same_v<ValueType, StringKey>)
        return data.hash == hash &&
               data.key.len == key.len &&
               memcmp(data.key.ptr, key.ptr, key.len) == 0;
    else
        return data.hash == hash &&
               data.key == key;
}

template <typename ValueType>
inline bool is_slot_taken(MapEntry<ValueType> data)
{
    if constexpr (std::is_same_v<ValueType, StringKey>)
        return data.key.ptr == NULL;
    else
        return data.key == 0;
}

template <typename ValueType>
u32 calc_hash(ValueType key)
{
    if constexpr (std::is_same_v<ValueType, StringKey>)
    {
        return XXH32(key.ptr, key.len, 0);
    }
    else if constexpr (sizeof(ValueType) <= 4)
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
inline u32 HMap<ValueType>::get_insert(ValueType key, u32 value)
{
    u32 hash = calc_hash(key);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        MapEntry<ValueType> &data = entries[bucket]; // TODO TEST FIX 4: Use pointer for efficiency

        if (is_slot_taken(data))
        { // empty slot
            data = MapEntry<ValueType>{
                hash,
                value,
                key};

            return value;
        }
        else if (key_equal(data, key, hash))
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}

#endif
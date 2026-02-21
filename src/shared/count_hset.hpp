#pragma once

#include "common.hpp"
#include "hash_util.hpp"
#include <xxhash.h>
#include <string.h>
#include <stdlib.h>

template <typename ValueType>
struct CountHSet
{
private:
    ValueType *entries;
    u32 capacity;
    u32 size;

public:
    CountHSet(const u32 count, const u32 memfactor = 1);
    ~CountHSet();
    bool Push(const ValueType key);
    u32 Size() const;
};

template <typename ValueType>
CountHSet<ValueType>::CountHSet(const u32 count, const u32 memfactor)
{
    u32 target = count * memfactor;
    if (target == 0 || target == 1)
    {
        capacity = 1;
    }
    else
    {
        capacity = 1u << (32 - __builtin_clz(target - 1));
    }
    size = 0;

    entries = (ValueType *)malloc(capacity * sizeof(ValueType));
    memset(entries, 0, capacity * sizeof(ValueType));
}

template <typename ValueType>
CountHSet<ValueType>::~CountHSet()
{
    free(entries);
}

template <typename ValueType>
inline bool CountHSet<ValueType>::Push(const ValueType key)
{
    const u32 hash = CalcHash(key);
    u32 bucket = hash & (capacity - 1);

    while (true)
    {
        ValueType &data = entries[bucket];
        if (data == 0)
        { // empty slot
            data = key;
            size++;
            return true;
        }
        else if (data == key)
        { // match
            return true;
        }

        bucket = (bucket + 1) & (capacity - 1);
    }
}

template <typename ValueType>
inline u32 CountHSet<ValueType>::Size() const
{
    return size;
}

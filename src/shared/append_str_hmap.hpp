#pragma once

#include "common.hpp"

struct StringKey
{
    u8 *ptr;
    u16 len;
};

struct StrEntry
{
    u32 hash;
    u32 value;
    StringKey key;
};

struct AppendOnlyStrHMap
{
private:
    StrEntry *entries;
    u32 hash_capacity;

public:
    AppendOnlyStrHMap(const u32 count, const u32 memfactor = 2);
    ~AppendOnlyStrHMap();
    u32 get_insert(StringKey key, u32 value);
};

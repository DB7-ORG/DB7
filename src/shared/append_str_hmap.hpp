#pragma once

#include <memory>
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
    std::unique_ptr<StrEntry[]> entries;
    u32 hash_capacity;

public:
    AppendOnlyStrHMap(const u32 count, const u32 memfactor = 2);
    u32 GetInsert(StringKey key, u32 value);
};


#include "append_str_hmap.h"
#include <xxhash.h>
#include <string.h>
#include <stdlib.h>
#include <type_traits>

AppendOnlyStrHMap::AppendOnlyStrHMap(const u32 count, const u32 memfactor)
{
    u32 target = count * memfactor;
    if (target == 0 || target == 1)
    {
        hash_capacity = 1;
    }
    else
    {
        hash_capacity = 1u << (32 - __builtin_clz(target - 1));
    }

    entries = (StrEntry *)calloc(hash_capacity, sizeof(StrEntry));
}

AppendOnlyStrHMap::~AppendOnlyStrHMap()
{
    free(entries);
}

u32 AppendOnlyStrHMap::get_insert(StringKey key, u32 value)
{
    u32 hash = XXH32(key.ptr, key.len, 0);
    u32 bucket = hash & (hash_capacity - 1);

    while (true)
    {
        StrEntry &data = entries[bucket];

        if (data.value == 0)
        { // empty slot
            data = StrEntry{
                hash,
                value,
                key};

            return value;
        }
        else if (data.hash == hash && data.key.len == key.len && memcmp(data.key.ptr, key.ptr, key.len) == 0)
        { // match
            return data.value;
        }

        bucket = (bucket + 1) & (hash_capacity - 1);
    }
}

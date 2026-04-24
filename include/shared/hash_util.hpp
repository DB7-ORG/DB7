#pragma once

#include "common.hpp"

namespace db7::shared
{
    struct HashUtil
    {
        static inline u32
        murmurhash32(u32 data)
        {
            u32 h = data;

            h ^= h >> 16;
            h *= 0x85ebca6b;
            h ^= h >> 13;
            h *= 0xc2b2ae35;
            h ^= h >> 16;
            return h;
        }

        /* 64-bit variant */
        static inline u64
        murmurhash64(u64 data)
        {
            u64 h = data;

            h ^= h >> 33;
            h *= 0xff51afd7ed558ccd;
            h ^= h >> 33;
            h *= 0xc4ceb9fe1a85ec53;
            h ^= h >> 33;

            return h;
        }
    };
}
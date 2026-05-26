#pragma once

#include "common.hpp"

namespace db7::access
{

    class BtreeHeader
    {
    public:
        u64 rlink;
        u32 count;
        u8 level;
        u64 max_val;

        BtreeHeader(u64 rlink, u32 count, u8 level, u64 max_val)
            : rlink(rlink), count(count), level(level), max_val(max_val) {}
    };

    BtreeHeader *CastHeader(byte *data);

    void WriteHeader(BtreeHeader *header, u64 rlink, u32 count, u8 level, u64 max_val);

    void IncrementHeaderSize(BtreeHeader *header);

}
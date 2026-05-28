#pragma once

#include "common.hpp"
#include "access/index/base_ly_header.hpp"

namespace db7::access
{
    struct NumberHeader : public BaseLyHeader
    {
    };

    // NumberHeader *CastHeader(byte *data);

    // void WriteHeader(NumberHeader *header, u64 rlink, u32 count, u8 level, u64 max_val);

    // void WriteHeader(byte *data, u64 rlink, u32 count, u8 level, u64 max_val);
}
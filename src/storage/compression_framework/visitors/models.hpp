#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"

struct StringData
{
    u8 **src;
    u32 *lenSrc;
    u32 totalLen;
    u32 nitems;
    ValidityMask *nullmap;
    u8 depth;

    StringData(u8 **src, u32 *lenSrc, u32 totalLen, u32 nitems, ValidityMask *nullmap, u8 depth = 0)
        : src(src), lenSrc(lenSrc), totalLen(totalLen), nitems(nitems), nullmap(nullmap), depth(depth) {}
};

template <typename T>
struct NumberData
{
    T *src;
    u32 nitems;
    ValidityMask *nullmap;
    u8 depth;

    NumberData(T *src, u32 nitems, ValidityMask *nullmap, u8 depth = 0)
        : src(src), nitems(nitems), nullmap(nullmap), depth(depth)
    {
    }
};

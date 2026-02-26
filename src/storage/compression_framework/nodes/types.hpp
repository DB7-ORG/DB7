#pragma once

#include "common.hpp"

constexpr u32 MAX_DEPTH = 2 * 2 - 1;

enum SchemaType
{
    Number,
    Double,
    String
};

enum SchemeAlgorythm
{
    Uncompressed,
    Bitpacking,
    Dictionary,
    FastPFor,
    Frequency,
    Fsst,
    Oneval,
    Rle
};

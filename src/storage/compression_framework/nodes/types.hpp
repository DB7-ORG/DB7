#pragma once

#include "common.hpp"

constexpr u32 MAX_DEPTH = 2;

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

#pragma once

#include "common.hpp"

enum SchemaType
{
    Number,
    Double,
    String
};

enum SchemeAlgorythm
{
    Uncompressed,
    Dictionary,
    Rle,
    Bitpacking,
    FastPFor,
    Frequency,
    Fsst,
    Oneval
};

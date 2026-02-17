#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"

struct FreqEncodedRes
{
    double *exceptions;
    u8 *bitmap;
    double topval;
};

struct FreqEncoder
{
    static void Encode(FreqEncodedRes *out, const double *in, const ValidityMask *nullmap, u32 nitems, double topval);
    static void Decode(double *out, FreqEncodedRes *in, u32 nitems);
};

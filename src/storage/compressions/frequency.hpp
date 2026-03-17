#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"

struct FreqEncodedRes
{
    double *exceptions;
    u8 *bitmap;
    double topval;
    u32 count;
};

struct FreqEncoder
{
    static void Encode(FreqEncodedRes *__restrict out, const double *__restrict in, const ValidityMask *nullmap, u32 nitems, double topval);
    static void Decode(double *__restrict out, FreqEncodedRes *__restrict in, u32 nitems);
};

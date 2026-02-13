#pragma once

#include "common.h"

struct FreqEncodedRes
{
    double *exceptions;
    u8 *bitmap;
    double topval;
};

struct FreqEncoder
{
    static void encode(FreqEncodedRes *out, const double *in, const u8 *nullmap, u32 nitems, double topval);
    static void decode(double *out, FreqEncodedRes *in, u32 nitems);
};
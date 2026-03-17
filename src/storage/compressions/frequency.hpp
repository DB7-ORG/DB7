#pragma once

#include "common.hpp"
#include "nullbitmap.hpp"
#include "roaring/roaring.hh"

template <typename T>
struct FreqEncodedRes
{
    T *exceptions;
    u8 *bitmap;
    T topval;
    u32 exception_count;
    u32 bitmap_size;
};

struct FreqEncoder
{
    template <typename T>
    static void Encode(FreqEncodedRes<T> *__restrict out, const T *__restrict in, const ValidityMask *nullmap, u32 nitems, T topval);
    template <typename T>
    static void Decode(T *__restrict out, FreqEncodedRes<T> *__restrict in, u32 nitems);
};

template <typename T>
void FreqEncoder::Encode(FreqEncodedRes<T> *__restrict out, const T *__restrict in, const ValidityMask *nullmap, u32 nitems, T topval)
{
    u8 *bitmap = out->bitmap;
    T *exceptions = out->exceptions;
    u32 offset = 0;

    roaring::Roaring exceptions_bitmap;

    bool allValid = nullmap->AllValid();

    for (u32 i = 0; i < nitems; i++)
    {
        bool isValid = allValid || nullmap->RowIsValid(i);
        if (isValid && in[i] != topval)
        {
            exceptions[offset++] = in[i];
            exceptions_bitmap.add(i);
        }
    }

    exceptions_bitmap.runOptimize();
    exceptions_bitmap.setCopyOnWrite(true);
    out->bitmap_size = exceptions_bitmap.write(reinterpret_cast<char *>(bitmap), false);
    out->topval = topval;
    out->exception_count = offset;
}

template <typename T>
void FreqEncoder::Decode(T *__restrict out, FreqEncodedRes<T> *__restrict in, u32 nitems)
{
    const roaring::Roaring exceptions_bitmap = roaring::Roaring::read(reinterpret_cast<const char *>(in->bitmap), false);
    const T *exceptions = in->exceptions;

    std::fill_n(out, nitems, in->topval);

    for (u32 index : exceptions_bitmap)
    {
        out[index] = *exceptions++;
    }
}
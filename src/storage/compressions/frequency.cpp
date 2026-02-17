#include "frequency.hpp"
#include "roaring/roaring.hh"

void FreqEncoder::Encode(FreqEncodedRes *out, const double *in, const ValidityMask *nullmap, u32 nitems, double topval)
{
    u8 *bitmap = out->bitmap;
    double *exceptions = out->exceptions;
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
    exceptions_bitmap.write(reinterpret_cast<char *>(bitmap), false);
    out->topval = topval;
}

void FreqEncoder::Decode(double *out, FreqEncodedRes *in, u32 nitems)
{
    const roaring::Roaring exceptions_bitmap = roaring::Roaring::read(reinterpret_cast<const char *>(in->bitmap), false);
    const double *exceptions = in->exceptions;

    std::fill_n(out, nitems, in->topval);

    for (u32 index : exceptions_bitmap)
    {
        out[index] = *exceptions++;
    }
}
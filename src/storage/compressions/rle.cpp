#include "compression.h"

void RleEncoder::encode(u32 *out, u32 *outLen, u32 &capacity, const u32 *in, u32 nitems)
{
    assert(nitems >= 1);

    u32 state = in[0];
    u32 count = 1;
    u32 offset = 0;

    for (u32 i = 1; i < nitems; i++)
    {
        if (state == in[i])
        {
            count++;
        }
        else
        {
            out[offset] = state;
            outLen[offset] = count;
            offset++;
            state = in[i];
            count = 1;
        }
    }

    out[offset] = state;
    outLen[offset] = count;
    capacity = ++offset;
}

void RleEncoder::decode(u32 *out, const u32 *in, const u32 *inLen, u32 &nitems, u32 capacity)
{
    u32 offset = 0;

    for (u32 i = 0; i < capacity; i++)
    {
        const u32 count = inLen[i];
        const u32 value = in[i];

        // Simple fill (compiler may optimize to SIMD or rep stosq)
        std::fill_n(out + offset, count, value);
        offset += count;
    }

    nitems = offset;
}

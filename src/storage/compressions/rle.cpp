#include "compression.h"

void encode(u32 *out, u32 *outLen, u32 &capacity, const u32 *in, u32 nitems)
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

void decode(u32 *out, const u32 *in, const u32 *inLen, u32 &nitems, u32 capacity)
{
    u32 offset = 0;
    for (u32 i = 0; i < capacity; i++)
    {
        for (u32 j = 0; j < inLen[i]; j++)
        {
            out[offset++] = in[i]; // TODO SIMD
        }
    }
    nitems = offset;
}
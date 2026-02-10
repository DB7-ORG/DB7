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

        const u32 extra = ((count >> 3) << 3);
        const u32 extraStart = offset + extra;
        const u32 remainder = count & 7;
        const u32 extraEnd = extraStart + remainder;

        const __m256i vec_value = _mm256_set1_epi32(value);

        for (u32 j = 0; j + 8 <= count; j += 8)
        {
            _mm256_storeu_si256((__m256i *)(out + offset), vec_value);
            offset += 8;
        }

        for (u32 j = extraStart; j < extraEnd; j++)
        {
            out[j] = value;
        }
        offset += remainder;
    }
    nitems = offset;
}
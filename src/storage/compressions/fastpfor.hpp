#pragma once

#include "common.hpp"
#include "bitpacking.hpp"
#include "align_utils.hpp"
#include "aligned_allocator.hpp"
#include "bit_utils.hpp"

#include <vector>
#include <stdexcept>

enum
{
    PACKSIZE = 32,
    overheadofeachexcept = 8,
    overheadduetobits = 8,
    overheadduetonmbrexcept = 8,
    BlockSize = 8 * PACKSIZE
};

static_assert(BlockSize % 256 == 0, "BlockSize is not divisible");

typedef std::vector<u32, AlignedSTLAllocator<u32, 32>> cachealignedvector;

// using ValueType = u32;

struct FastPForEncoder
{
private:
    std::vector<cachealignedvector> datatobepacked;
    std::vector<u8> bytescontainer;

    u32 *PackExceptionBlocks(u32 *out, cachealignedvector &in, u8 bit);

    template <typename ValueType>
    static void GetBestB(const ValueType *in, u8 &bestb, u8 &bestcexcept, u8 &maxb, u32 blk_size = BlockSize);

public:
    FastPForEncoder();
    FastPForEncoder(u32 nitems);
    void ResetTable();

    template <typename ValueType>
    u32 Encode(u32 *out, const ValueType *in, u32 nitems);
    template <typename ValueType>
    u32 Decode(ValueType *out, const u32 *in, u32 nitems);
    static u32 EstimateCompression(const u32 *freqs, const u32 nitems, u32 typeBits);
};

template <typename ValueType>
void FastPForEncoder::GetBestB(const ValueType *in, u8 &bestb, u8 &bestcexcept, u8 &maxb, u32 blk_size)
{
    constexpr u32 ValueTypeBits = sizeof(ValueType) * 8;

    u32 freqs[ValueTypeBits + 1];
    for (u32 k = 0; k <= ValueTypeBits; ++k)
        freqs[k] = 0;

    for (u32 k = 0; k < blk_size; ++k)
    {
        auto pos = CountBitsUsed(in[k]);
        freqs[pos]++;
    }

    bestb = ValueTypeBits;
    while (freqs[bestb] == 0)
        bestb--;

    maxb = bestb;
    u32 bestcost = bestb * blk_size;
    u32 cexcept = 0;
    bestcexcept = static_cast<u8>(cexcept);
    for (u32 b = bestb - 1; b < ValueTypeBits; --b)
    {
        cexcept += freqs[b + 1];
        u32 thiscost = cexcept * overheadofeachexcept + cexcept * (maxb - b) + b * blk_size + 8; // the  extra 8 is the cost of storing maxbits
        if (thiscost < bestcost)
        {
            bestcost = thiscost;
            bestb = static_cast<u8>(b);
            bestcexcept = static_cast<u8>(cexcept);
        }
    }
}

template <typename ValueType>
u32 FastPForEncoder::Encode(u32 *__restrict out, const ValueType *__restrict in, u32 nitems)
{
    // assert(nitems % BlockSize == 0);

    constexpr u32 ValueTypeBits = sizeof(ValueType) * 8;

    u32 *const initout = out;
    u32 *const headerout = out++;

    ResetTable();

    u8 *bc = &bytescontainer[0];

    const ValueType *const final = in + nitems;
    for (; (in + BlockSize <= final); in += BlockSize)
    {
        u8 bestb, bestcexcept, maxb;
        GetBestB(in, bestb, bestcexcept, maxb);
        *bc++ = bestb;
        *bc++ = bestcexcept;
        if (bestcexcept > 0)
        {
            *bc++ = maxb;
            auto &thisexceptioncontainer = datatobepacked[maxb - bestb];
            const ValueType maxval = 1ull << bestb;
            for (u32 k = 0; k < BlockSize; ++k) // TODO this for sure can be optimized
            {
                if (in[k] >= maxval)
                {
                    // we have an exception
                    thisexceptioncontainer.push_back(in[k] >> bestb);
                    *bc++ = static_cast<u8>(k);
                }
            }
            out = reinterpret_cast<u32 *>(BitPackEncoder<ValueType>::Encode(out, in, BlockSize, bestb)); // TODO executed once per loop
        }
        else
        {
            out = reinterpret_cast<u32 *>(BitPackEncoder<ValueType>::EncodeWithoutMask(out, in, BlockSize, bestb)); // TODO executed once per loop
        }
    }

    ///////////////

    u32 items_left = final - in;
    if (items_left != 0)
    {
        u8 bestb, bestcexcept, maxb;
        GetBestB(in, bestb, bestcexcept, maxb, items_left);
        *bc++ = bestb;
        *bc++ = bestcexcept;
        if (bestcexcept > 0)
        {
            *bc++ = maxb;
            auto &thisexceptioncontainer = datatobepacked[maxb - bestb];
            const ValueType maxval = 1ull << bestb;
            for (u32 k = 0; k < items_left; ++k) // TODO this for sure can be optimized
            {
                if (in[k] >= maxval)
                {
                    // we have an exception
                    thisexceptioncontainer.push_back(in[k] >> bestb);
                    *bc++ = static_cast<u8>(k);
                }
            }
        }
        out = reinterpret_cast<u32 *>(BitPackScalarEncoder<ValueType>::Encode((ValueType *)out, in, items_left, bestb));
    }

    ///////////////

    headerout[0] = static_cast<u32>(out - headerout);
    const u32 bytescontainersize = static_cast<u32>(bc - &bytescontainer[0]);
    *(out++) = bytescontainersize;

    memcpy(out, &bytescontainer[0], bytescontainersize);

    u8 *pad8 = reinterpret_cast<u8 *>(out + bytescontainersize);
    out += RoundUp(bytescontainersize, sizeof(u32));

    while (pad8 < reinterpret_cast<u8 *>(out))
        *pad8++ = 0;

    ValueType bitmap = 0;
    for (u32 k = 2; k <= ValueTypeBits; ++k)
    {
        if (datatobepacked[k].size() != 0)
            bitmap |= (1ull << (k - 1));
    }

    if constexpr (std::is_same_v<ValueType, u64>)
    {
        *(out++) = static_cast<u32>(bitmap & 0xFFFFFFFF); // this always assumes 64bits
        *(out++) = static_cast<u32>(bitmap >> 32);
    }
    else if constexpr (std::is_same_v<ValueType, u32> || std::is_same_v<ValueType, u16>)
    {
        *(out++) = static_cast<u32>(bitmap);
    }

    for (u32 k = 2; k <= ValueTypeBits; ++k) // TODO this is awful change it so there is no padding between Exception blocks
    {
        if (datatobepacked[k].size() > 0)
        {
            out = PackExceptionBlocks(out, datatobepacked[k], k);
        }
    }

    return out - initout;
}

template <typename ValueType>
u32 FastPForEncoder::Decode(ValueType *__restrict out, const u32 *__restrict in, u32 nitems)
{
    constexpr u32 ValueTypeBits = sizeof(ValueType) * 8;

    ValueType *const initout = out;

    ResetTable();

    const u32 *const headerin = in++;
    const u32 wheremeta = headerin[0];
    const u32 *inexcept = headerin + wheremeta;
    const u32 bytesize = *inexcept++;
    const u8 *bytep = reinterpret_cast<const u8 *>(inexcept);
    inexcept += RoundUp(bytesize, sizeof(u32));

    ValueType bitmap;
    if constexpr (std::is_same_v<ValueType, u64>)
    {
        u32 lo = *(inexcept++);
        u32 hi = *(inexcept++);
        bitmap = ((u64)hi << 32) | lo;
    }
    else if constexpr (std::is_same_v<ValueType, u32> || std::is_same_v<ValueType, u16>)
    {
        bitmap = *(inexcept++);
    }

    for (u32 k = 2; k <= ValueTypeBits; ++k)
    {
        if ((bitmap & (1ull << (k - 1))) != 0)
        {
            u32 size = *(inexcept++);
            datatobepacked[k].resize(size);
            // inexcept = BitPackScalarEncoder<u32>::Decode(datatobepacked[k].data(), inexcept, size, k);
            for (u32 i = 0; i < size; i++)
            {
                datatobepacked[k][i] = *(inexcept++);
            }
        }
    }

    cachealignedvector::const_iterator unpackpointers[ValueTypeBits + 1];
    for (u32 k = 2; k <= ValueTypeBits; ++k)
    {
        unpackpointers[k] = datatobepacked[k].begin();
    }

    for (u32 run = 0; run < nitems / BlockSize; ++run)
    {
        const u8 b = *bytep++;
        const u8 cexcept = *bytep++;
        auto newOut = BitPackEncoder<ValueType>::Decode(out, in, BlockSize, b);
        in += 8 * b * BlockSize / 256;

        if (cexcept > 0)
        {
            const u8 maxbits = *bytep++;
            if (maxbits - b == 1)
            {
                for (u32 k = 0; k < cexcept; ++k)
                {
                    const u8 pos = *(bytep++);
                    out[pos] |= static_cast<ValueType>(1) << b;
                }
            }
            else
            {
                cachealignedvector::const_iterator &exceptionsptr = unpackpointers[maxbits - b];
                for (u32 k = 0; k < cexcept; ++k)
                {
                    const u8 pos = *(bytep++);
                    out[pos] |= (*(exceptionsptr++)) << b;
                }
            }
        }
        out = newOut;
    }

    u32 items_left = nitems - (nitems / BlockSize) * BlockSize;
    if (items_left != 0)
    {
        const u8 b = *bytep++;
        const u8 cexcept = *bytep++;
        auto newOut = BitPackScalarEncoder<ValueType>::Decode(out, (ValueType *)in, items_left, b);

        if (cexcept > 0)
        {
            const u8 maxbits = *bytep++;
            if (maxbits - b == 1)
            {
                for (u32 k = 0; k < cexcept; ++k)
                {
                    const u8 pos = *(bytep++);
                    out[pos] |= static_cast<ValueType>(1) << b;
                }
            }
            else
            {
                cachealignedvector::const_iterator &exceptionsptr = unpackpointers[maxbits - b];
                for (u32 k = 0; k < cexcept; ++k)
                {
                    const u8 pos = *(bytep++);
                    out[pos] |= (*(exceptionsptr++)) << b;
                }
            }
        }
        out = newOut;
    }

    return out - initout;
}

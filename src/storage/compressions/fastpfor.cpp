#include "fastpfor.hpp"
#include "helper_utils.hpp"

#include <immintrin.h>
#include <iostream>
#include <unistd.h>
#include <cstring>

// TODO template
u32 FastPForEncoder::EstimateCompression(const u32 *freqs, const u32 nitems)
{
    const u32 numOfBlocks = nitems / BlockSize;
    const u32 numExcBlocks = numOfBlocks;      // TODO should be better aprox
    const u32 metaDataSize = +16 * numOfBlocks // bestcexcept and bestb in bytescontainer
                             + 2 * 32          // len prefixes for packed data and bytescontainer
                             + 3 * 8           // len of padding (max 3 bytes)
                             + 32;             // bitmap

    u32 bestb = 32;
    while (freqs[bestb] == 0)
        bestb--;

    u32 cexcept = 0;
    u32 nonZeroCount = 0;
    u32 cpackExcept = 0;

    u32 bestcost = bestb * nitems + metaDataSize;

    for (u32 b = bestb - 1; b < 32; --b)
    {
        cexcept += freqs[b + 1];
        nonZeroCount += (freqs[b + 1] != 0);
        cpackExcept += ((freqs[b + 1] * (b + 1) + 63) / 64) * 64; // TODO chnage scalar compression

        // if (freqs[b + 1] != 0)
        // {
        //     nonZeroCount++;
        //     cpackExcept += ((freqs[b + 1] * (b + 1) + 63) / 64) * 64;
        // }

        u32 thiscost = cexcept * overheadofeachexcept // overhead of and index that points to an exception
                       + cpackExcept                  // calculation for packing exceptions
                       + b * nitems                   // packed data with best bit size
                       + 8 * numExcBlocks             // maxb stored for each block that contains exceptions
                       + nonZeroCount * 32            // size prefix when padding exceptions
                       + metaDataSize;                // metadata

        bestcost = std::min(thiscost, bestcost);
    }

    return bestcost;
}

u32 *FastPForEncoder::PackExceptionBlocks(u32 *out, cachealignedvector &in, u8 bit)
{
    const u32 size = static_cast<u32>(in.size());
    *out++ = size;

    out = BitPackScalarEncoder<u32>::Encode(out, in.data(), in.size(), bit);

    return out;
}

FastPForEncoder::FastPForEncoder()
{
    std::cout << "unsafe pfor!!!" << std::endl;
    datatobepacked.resize(65);
    bytescontainer.resize(1024 * 1000);
}

FastPForEncoder::FastPForEncoder(u32 nitems)
{
    datatobepacked.resize(65);
    bytescontainer.resize(3 * (nitems / BlockSize) + 2 * nitems); // TODO this should be optimized
}

void FastPForEncoder::ResetTable()
{
    for (u32 k = 0; k < 64 + 1; ++k)
        datatobepacked[k].clear();
}

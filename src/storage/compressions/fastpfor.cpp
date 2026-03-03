#include "fastpfor.hpp"
#include "helper_utils.hpp"

#include <immintrin.h>
#include <iostream>
#include <unistd.h>
#include <cstring>

u32 *FastPForEncoder::PackExceptionBlocks(u32 *out, cachealignedvector &in, u8 bit)
{
    (void)bit; // TODO not used

    const u32 size = static_cast<u32>(in.size());
    *out++ = size;

    // out = BitPackScalarEncoder<u32>::Encode(out, in.data(), in.size(), bit);

    for (u32 i = 0; i < size; i++)
    {
        out[i] = in[i];
    }

    return out + size;
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

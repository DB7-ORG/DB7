#pragma once

#include "common.hpp"
#include "bitpacking.hpp"

#include <vector>
#include <stdexcept>
#include "align_utils.hpp"

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

struct FastPForEncoder
{
    BitPackEncoder bitpackEncoder;
    std::vector<cachealignedvector> datatobepacked;
    std::vector<u8> bytescontainer;

    FastPForEncoder();
    FastPForEncoder(u32 nitems);
    u32 Encode(u32 *out, const u32 *in, u32 nitems);
    u32 Decode(u32 *out, const u32 *in, u32 nitems);
    static u32 EstimateCompression(u32 *freqs, u32 size);
    void ResetTable();
};

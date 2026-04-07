#pragma once

#include "common.hpp"
#include "src_type.hpp"

enum SchemaType
{
    Number,
    Double,
    String
};

enum SchemeAlgorithm : u8
{
    Uncompressed,
    Dictionary,
    Rle,
    Bitpacking,
    FastPFor,
    Frequency,
    Fsst,
    Oneval
};

inline void PrintScheme(SchemeAlgorithm *applied, u32 size)
{
    for (u32 i = 0; i < size; i++)
    {
        switch (applied[i])
        {
        case SchemeAlgorithm::Uncompressed:
            printf("[%u] Uncompressed\n", i);
            break;
        case SchemeAlgorithm::Dictionary:
            printf("[%u] Dictionary\n", i);
            break;
        case SchemeAlgorithm::Rle:
            printf("[%u] Rle\n", i);
            break;
        case SchemeAlgorithm::Bitpacking:
            printf("[%u] Bitpacking\n", i);
            break;
        case SchemeAlgorithm::FastPFor:
            printf("[%u] FastPFor\n", i);
            break;
        case SchemeAlgorithm::Frequency:
            printf("[%u] Frequency\n", i);
            break;
        case SchemeAlgorithm::Fsst:
            printf("[%u] Fsst\n", i);
            break;
        case SchemeAlgorithm::Oneval:
            printf("[%u] Oneval\n", i);
            break;
        }
    }
}

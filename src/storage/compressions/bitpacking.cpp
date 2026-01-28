#include "compression.h"

#include <unistd.h>
#include <iostream>
#include <xxhash.h>
#include <string.h>
#include <cstdint>
#include <immintrin.h>

struct BitPackEncoder
{
};

void encode(uint32_t *inOut, uint32_t nitems)
{
    uint64_t *out = (uint64_t *)inOut;
    uint32_t notUsedBits = __builtin_clz(nitems);
    uint32_t usedBits = 32 - notUsedBits;
    uint32_t mask = UINT32_MAX >> notUsedBits;
    auto offset = 0;
    for (auto i = 0; i < nitems; i++) // lets assume its 3
    {
        // clang-format off
        uint64_t pack=((uint64_t)inOut[i]&usedBits)<<64-3
        | ((uint64_t)inOut[i]&usedBits)<<(64-6)
        | ((uint64_t)inOut[i+1]&usedBits)<<(64-9)
        | ((uint64_t)inOut[i+2]&usedBits)<<(64-12)
        | ((uint64_t)inOut[i+3]&usedBits)<<(64-15)
        | ((uint64_t)inOut[i+4]&usedBits)<<(64-18)
        | ((uint64_t)inOut[i+5]&usedBits)<<(64-21)
        | ((uint64_t)inOut[i+6]&usedBits)<<(64-24)
        | ((uint64_t)inOut[i+7]&usedBits)<<(64-27)
        | ((uint64_t)inOut[i+8]&usedBits)<<(64-30)
        | ((uint64_t)inOut[i+9]&usedBits)<<(64-33)
        | ((uint64_t)inOut[i+10]&usedBits)<<(64-36)
        | ((uint64_t)inOut[i+11]&usedBits)<<(64-39)
        | ((uint64_t)inOut[i+12]&usedBits)<<(64-42)
        | ((uint64_t)inOut[i+13]&usedBits)<<(64-45)
        | ((uint64_t)inOut[i+14]&usedBits)<<(64-48)
        | ((uint64_t)inOut[i+15]&usedBits)<<(64-51)
        | ((uint64_t)inOut[i+16]&usedBits)<<(64-54)
        | ((uint64_t)inOut[i+17]&usedBits)<<(64-57)
        | ((uint64_t)inOut[i+18]&usedBits)<<(64-60)
        | ((uint64_t)inOut[i+19]&usedBits)<<(64-63);
        // clang-format on

        // offset+=usedBits;
    }
}
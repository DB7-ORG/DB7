#include "compression.h"
#include "../../shared/helper_utils.h"

#include <cstdint>
#include <immintrin.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <xxhash.h>

void print_binary(uint64_t bytes)
{
    for (int i = 63; i >= 0; i--)
    {
        printf("%ld", (bytes >> i) & 1);
    }
    printf("\n");
}

static inline int scalar_encode(uint64_t *out, uint32_t *in, uint32_t nitems, uint32_t ndistinct)
{
    uint32_t notUsedBits = __builtin_clz(ndistinct - 1);
    uint32_t usedBits = 32 - notUsedBits;
    uint32_t offset = 0;
    uint32_t shift = 0;
    uint64_t pack = 0;
    for (uint32_t i = 0; i < nitems; i++)
    {
        pack |= ((uint64_t)in[i]) << (shift);
        shift += usedBits;
        if (shift > 64 - usedBits)
        {
            out[offset++] = pack;
            shift = 0;
            pack = 0;
        }
    }

    if (shift != 0)
    {
        out[offset++] = pack;
    }

    return offset;
}

static inline int scalar_decode(uint32_t *out, uint64_t *in, uint32_t nitems, uint32_t ndistinct)
{
    uint32_t notUsedBits = __builtin_clz(ndistinct - 1);
    uint32_t usedBits = 32 - notUsedBits;

    uint64_t mask = UINT32_MAX >> notUsedBits;
    uint32_t shift = 0;
    uint32_t offset = 0;

    for (uint32_t i = 0; i < nitems; i++)
    {
        out[i] = (in[offset] >> shift) & mask;
        shift += usedBits;
        if (shift > 64 - usedBits)
        {
            shift = 0;
            offset++;
        }
    }

    return offset;
}

#ifdef __AVX2__
static inline int avx2_encode(uint64_t *out, uint32_t *in, uint32_t nitems, uint32_t ndistinct)
{
    uint32_t notUsedBits = __builtin_clz(ndistinct - 1);
    uint32_t usedBits = 32 - notUsedBits;

    const __m256i *inSimd = (const __m256i *)in;
    __m256i *outSimd = (__m256i *)out;

    uint32_t valuesPerWord = 32 / usedBits;
    uint32_t blocksNeeded = nitems / (8 * valuesPerWord);

    for (uint32_t i = 0; i < blocksNeeded; i++)
    {
        __m256i w0 = _mm256_setzero_si256();

        for (uint32_t j = 0; j < valuesPerWord; j++)
        {
            w0 = _mm256_or_si256(w0, _mm256_slli_epi32(_mm256_lddqu_si256(inSimd + i * valuesPerWord + j), j * usedBits));
        }
        _mm256_storeu_si256(outSimd + i, w0);
    }

    return 0;
}

static inline int avx2_decode(uint32_t *out, uint64_t *in, uint32_t nitems, uint32_t ndistinct)
{
    uint32_t notUsedBits = __builtin_clz(ndistinct - 1);
    uint32_t usedBits = 32 - notUsedBits;

    const __m256i *inSimd = (const __m256i *)in;
    __m256i *outSimd = (__m256i *)out;

    uint32_t valuesPerWord = 32 / usedBits;
    uint32_t blocksNeeded = nitems / (8 * valuesPerWord);

    __m256i mask = _mm256_set1_epi32((1U << usedBits) - 1);

    for (uint32_t i = 0; i < blocksNeeded; i++)
    {
        __m256i w0 = _mm256_loadu_si256(inSimd + i);

        for (uint32_t j = 0; j < valuesPerWord; j++)
        {
            __m256i extracted = _mm256_and_si256(_mm256_srli_epi32(w0, j * usedBits), mask);
            _mm256_storeu_si256(outSimd + i * valuesPerWord + j, extracted);
        }
    }

    return nitems;
}
#endif

int BitPackEncoder::encode(uint64_t *out, uint32_t *in, uint32_t nitems, uint32_t ndistinct)
{
#ifdef __AVX2__
    return avx2_encode(out, in, nitems, ndistinct);
#else
    return scalar_encode(out, in, nitems, ndistinct);
#endif
}

int BitPackEncoder::decode(uint32_t *out, uint64_t *in, uint32_t nitems,
                           uint32_t ndistinct)
{
#ifdef __AVX2__
    return avx2_decode(out, in, nitems, ndistinct);
#else
    return scalar_decode(out, in, nitems, ndistinct);
#endif
}

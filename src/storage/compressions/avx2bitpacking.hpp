#pragma once

// TODO replace w common
#include <cstdint>
#include <cassert>
#include <stdexcept>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

#ifndef __AVX2__
#error This code requires AVX2 support (available on Intel processors made since ~2013)
#endif

#ifdef _MSC_VER
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
#else
/* Pretty much anything else. */
#include <x86intrin.h>
#endif

#include <cstring>
#include <cstdint>

template <typename T>
struct avx_traits;

template <>
struct avx_traits<u16>
{
    static constexpr int element_bits = 16;
    static constexpr int elements_per_vector = 16; // 256 bits / 16 bits
    static constexpr int num_vectors = 256 / 16;   // 16 vectors for 256 values

    static __m256i shift_left(__m256i v, int bits)
    {
        return _mm256_slli_epi16(v, bits);
    }

    static __m256i shift_right(__m256i v, int bits)
    {
        return _mm256_srli_epi16(v, bits);
    }
};

template <>
struct avx_traits<u32>
{
    static constexpr int element_bits = 32;
    static constexpr int elements_per_vector = 8; // 256 bits / 32 bits
    static constexpr int num_vectors = 256 / 8;   // 32 vectors for 256 values

    static __m256i shift_left(__m256i v, int bits)
    {
        return _mm256_slli_epi32(v, bits);
    }

    static __m256i shift_right(__m256i v, int bits)
    {
        return _mm256_srli_epi32(v, bits);
    }
};

template <>
struct avx_traits<u64>
{
    static constexpr int element_bits = 64;
    static constexpr int elements_per_vector = 4; // 256 bits / 64 bits
    static constexpr int num_vectors = 256 / 4;   // 64 vectors for 256 values

    static __m256i shift_left(__m256i v, int bits)
    {
        return _mm256_slli_epi64(v, bits);
    }

    static __m256i shift_right(__m256i v, int bits)
    {
        return _mm256_srli_epi64(v, bits);
    }
};

// Main template function that works for all types
template <typename T, int BITS, bool USE_MASK = false>
static void avxpackblock(const T *pin, __m256i *compressed)
{
    if constexpr (BITS == 0)
    {
        (void)compressed;
        (void)pin;
        return;
    }

    using traits = avx_traits<T>;
    constexpr int ELEMENT_BITS = traits::element_bits;
    constexpr int NUM_VECTORS = traits::num_vectors;

    __m256i mask;
    if constexpr (USE_MASK && BITS < ELEMENT_BITS)
    {
        constexpr T mask_value = (T(1) << BITS) - 1;
        if constexpr (std::is_same_v<T, u16>)
        {
            mask = _mm256_set1_epi16(mask_value);
        }
        else if constexpr (std::is_same_v<T, u32>)
        {
            mask = _mm256_set1_epi32(mask_value);
        }
        else if constexpr (std::is_same_v<T, u64>)
        {
            mask = _mm256_set1_epi64x(mask_value);
        }
    }

    const __m256i *in = (const __m256i *)pin;
    __m256i w0 = _mm256_setzero_si256();
    __m256i w1 = _mm256_setzero_si256();
    __m256i tmp;

    int out_idx = 0;
    int bit_offset = 0;
    __m256i *current_word = &w0;

#pragma GCC unroll 256
    for (int i = 0; i < NUM_VECTORS; ++i)
    {
        __m256i data = _mm256_lddqu_si256(in + i);

        if constexpr (USE_MASK && BITS < ELEMENT_BITS)
        {
            data = _mm256_and_si256(mask, data);
        }

        if (bit_offset + BITS <= ELEMENT_BITS)
        {
            if (bit_offset == 0)
            {
                *current_word = data;
            }
            else
            {
                *current_word = _mm256_or_si256(*current_word,
                                                traits::shift_left(data, bit_offset));
            }
            bit_offset += BITS;

            if (bit_offset == ELEMENT_BITS)
            {
                _mm256_storeu_si256(compressed + out_idx++, *current_word);
                current_word = (current_word == &w0) ? &w1 : &w0;
                *current_word = _mm256_setzero_si256();
                bit_offset = 0;
            }
        }
        else
        {
            int bits_in_first = ELEMENT_BITS - bit_offset;
            int bits_in_second = BITS - bits_in_first;

            tmp = data;
            *current_word = _mm256_or_si256(*current_word,
                                            traits::shift_left(tmp, bit_offset));
            _mm256_storeu_si256(compressed + out_idx++, *current_word);

            current_word = (current_word == &w0) ? &w1 : &w0;
            *current_word = traits::shift_right(tmp, bits_in_first);
            bit_offset = bits_in_second;
        }
    }
}

// Main template decode function that works for all types
template <typename T, int BITS>
static void avxunpackblock(const __m256i *compressed, T *pout)
{
    if constexpr (BITS == 0)
    {
        memset(pout, 0, 256 * sizeof(T));
        return;
    }

    using traits = avx_traits<T>;
    constexpr int ELEMENT_BITS = traits::element_bits;
    constexpr int NUM_VECTORS = traits::num_vectors;

    constexpr T mask_value = (BITS == ELEMENT_BITS) ? T(~T(0)) : (T(1) << BITS) - 1;
    __m256i mask;
    if constexpr (std::is_same_v<T, u16>)
    {
        mask = _mm256_set1_epi16(mask_value);
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        mask = _mm256_set1_epi32(mask_value);
    }
    else if constexpr (std::is_same_v<T, u64>)
    {
        mask = _mm256_set1_epi64x(mask_value);
    }

    __m256i *out = (__m256i *)pout;
    __m256i w0 = _mm256_lddqu_si256(compressed);
    __m256i w1;

    int in_idx = 0;
    int bit_offset = 0;
    __m256i *current_word = &w0;

#pragma GCC unroll 256
    for (int i = 0; i < NUM_VECTORS; ++i)
    {
        __m256i result;

        if (bit_offset + BITS <= ELEMENT_BITS)
        {
            if (bit_offset == 0 && BITS == ELEMENT_BITS)
            {
                result = *current_word;
            }
            else if (bit_offset == 0)
            {
                result = _mm256_and_si256(mask, *current_word);
            }
            else if (bit_offset + BITS == ELEMENT_BITS)
            {
                result = traits::shift_right(*current_word, bit_offset);
            }
            else
            {
                result = _mm256_and_si256(mask, traits::shift_right(*current_word, bit_offset));
            }

            bit_offset += BITS;

            if (bit_offset == ELEMENT_BITS)
            {
                in_idx++;
                bit_offset = 0;

                if (i + 1 < NUM_VECTORS)
                {
                    current_word = (current_word == &w0) ? &w1 : &w0;
                    *current_word = _mm256_lddqu_si256(compressed + in_idx);
                }
            }
        }
        else
        {
            int bits_in_first = ELEMENT_BITS - bit_offset;
            int bits_in_second = BITS - bits_in_first;

            __m256i low_bits = traits::shift_right(*current_word, bit_offset);

            in_idx++;
            current_word = (current_word == &w0) ? &w1 : &w0;
            *current_word = _mm256_lddqu_si256(compressed + in_idx);

            __m256i high_bits = traits::shift_left(*current_word, bits_in_first);

            result = _mm256_and_si256(mask, _mm256_or_si256(low_bits, high_bits));

            bit_offset = bits_in_second;
        }

        _mm256_storeu_si256(out + i, result);
    }
}

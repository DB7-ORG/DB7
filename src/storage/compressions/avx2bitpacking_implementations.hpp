#pragma once

// TODO replace w common
#include <cstdint>
#include <cassert>
#include <stdexcept>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i8 = int8_t;

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
    static constexpr u32 element_bits = 16;
    static constexpr u32 elements_per_vector = 16; // 256 bits / 16 bits
    static constexpr u32 num_vectors = 256 / 16;   // 16 vectors for 256 values

    static inline __m256i shift_left(__m256i v, u32 bits)
    {
        return _mm256_slli_epi16(v, bits);
    }

    static inline __m256i shift_right(__m256i v, u32 bits)
    {
        return _mm256_srli_epi16(v, bits);
    }
};

template <>
struct avx_traits<u32>
{
    static constexpr u32 element_bits = 32;
    static constexpr u32 elements_per_vector = 8; // 256 bits / 32 bits
    static constexpr u32 num_vectors = 256 / 8;   // 32 vectors for 256 values

    static inline __m256i shift_left(__m256i v, u32 bits)
    {
        return _mm256_slli_epi32(v, bits);
    }

    static inline __m256i shift_right(__m256i v, u32 bits)
    {
        return _mm256_srli_epi32(v, bits);
    }
};

template <>
struct avx_traits<u64>
{
    static constexpr u32 element_bits = 64;
    static constexpr u32 elements_per_vector = 4; // 256 bits / 64 bits
    static constexpr u32 num_vectors = 256 / 4;   // 64 vectors for 256 values

    static inline __m256i shift_left(__m256i v, u32 bits)
    {
        return _mm256_slli_epi64(v, bits);
    }

    static inline __m256i shift_right(__m256i v, u32 bits)
    {
        return _mm256_srli_epi64(v, bits);
    }
};

template <typename T, u32 BITS, u32 ELEMENT_BITS>
static constexpr __m256i GetMask()
{
    constexpr T mask_value = (BITS == ELEMENT_BITS) ? T(~T(0)) : (T(1) << BITS) - 1;
    if constexpr (std::is_same_v<T, u16>)
    {
        return _mm256_set1_epi16(mask_value);
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return _mm256_set1_epi32(mask_value);
    }
    else if constexpr (std::is_same_v<T, u64>)
    {
        return _mm256_set1_epi64x(mask_value);
    }
}

// Main template function that works for all types
// Better approach is recursive templating because we have more control on i (in the for loop)
// and that creates a cleaner looking code (but compilers are smart so i dont care)
template <typename T, u32 BITS, bool USE_MASK = false>
static void AvxPackBlock(const T *__restrict pin, __m256i *__restrict compressed)
{
    if constexpr (BITS == 0)
    {
        (void)compressed;
        (void)pin;
        return;
    }

    using traits = avx_traits<T>;
    constexpr u32 ELEMENT_BITS = traits::element_bits;
    constexpr u32 NUM_VECTORS = traits::num_vectors;

    __m256i mask;
    if constexpr (USE_MASK && BITS < ELEMENT_BITS)
    {
        mask = GetMask<T, BITS, ELEMENT_BITS>();
    }

    const __m256i *in = reinterpret_cast<const __m256i *>(pin);
    __m256i w0 = _mm256_setzero_si256();
    __m256i w1 = _mm256_setzero_si256();
    __m256i tmp;

    u32 out_idx = 0;
    u32 bit_offset = 0;
    __m256i *current_word = &w0;

#pragma GCC unroll 64
    for (u32 i = 0; i < NUM_VECTORS; ++i)
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
            u32 bits_in_first = ELEMENT_BITS - bit_offset;
            u32 bits_in_second = BITS - bits_in_first;

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
// Better approach is recursive templating because we have more control on i (in the for loop)
// and that creates a cleaner looking code (but compilers are smart so i dont care)
template <typename T, u32 BITS>
static void AvxUnPackBlock(const __m256i *__restrict compressed, T *__restrict pout)
{
    if constexpr (BITS == 0)
    {
        memset(pout, 0, 256 * sizeof(T));
        return;
    }

    using traits = avx_traits<T>;
    constexpr u32 ELEMENT_BITS = traits::element_bits;
    constexpr u32 NUM_VECTORS = traits::num_vectors;

    __m256i mask = GetMask<T, BITS, ELEMENT_BITS>();

    __m256i *out = reinterpret_cast<__m256i *>(pout);
    __m256i w[2];
    w[0] = _mm256_lddqu_si256(compressed + 0);

#pragma GCC unroll 64
    for (u32 i = 0; i < NUM_VECTORS; ++i)
    {
        u32 bit_offset = (i * BITS) % ELEMENT_BITS;
        u32 in_idx = (i * BITS) / ELEMENT_BITS;
        u32 slot = in_idx & 1;
        u32 prev_in_idx = (i == 0) ? 0 : ((i - 1) * BITS) / ELEMENT_BITS;

        if (i > 0 && in_idx != prev_in_idx)
            w[slot] = _mm256_lddqu_si256(compressed + in_idx);

        __m256i result;
        if (bit_offset + BITS <= ELEMENT_BITS)
        {
            if (bit_offset == 0 && BITS == ELEMENT_BITS)
                result = w[slot];
            else if (bit_offset == 0)
                result = _mm256_and_si256(mask, w[slot]);
            else if (bit_offset + BITS == ELEMENT_BITS)
                result = traits::shift_right(w[slot], bit_offset);
            else
                result = _mm256_and_si256(mask, traits::shift_right(w[slot], bit_offset));
        }
        else
        {
            u32 bits_in_first = ELEMENT_BITS - bit_offset;
            u32 next_slot = (in_idx + 1) & 1;

            w[next_slot] = _mm256_lddqu_si256(compressed + in_idx + 1);

            __m256i low_bits = traits::shift_right(w[slot], bit_offset);
            __m256i high_bits = traits::shift_left(w[next_slot], bits_in_first);
            result = _mm256_and_si256(mask, _mm256_or_si256(low_bits, high_bits));
        }

        _mm256_storeu_si256(out + i, result);
    }
}

template <typename T, u32 Bits>
static T UnPackSingle(const T *compressed, const u32 idx)
{
    static_assert(Bits >= 0, "Bits must be >= 0");

    if constexpr (Bits == 0)
    {
        return 0;
    }
    else
    {
        using traits = avx_traits<T>;
        constexpr u32 ELEMENT_BITS = traits::element_bits;
        constexpr u32 ELEMENTS_PER_VECTOR = traits::elements_per_vector;
        constexpr T mask = (Bits == ELEMENT_BITS) ? T(~T(0)) : (T(1) << Bits) - 1;

        // Which 256-element block
        const u32 block = idx / 256; // idx / 256

        // Offset to start of this block (each block uses Bits * ELEMENTS_PER_VECTOR words)
        auto in = compressed + block * Bits * ELEMENTS_PER_VECTOR;

        // Lane and position within vector
        const u32 lane = (idx / ELEMENTS_PER_VECTOR) % ELEMENT_BITS; // Which of 32 lanes (0-31)
        const u32 pos = idx % ELEMENTS_PER_VECTOR;                   // Position within lane (0-7)

        // Calculate bit position for this lane
        const u32 bitpos = lane * Bits;

        // Which word and bit offset within that word
        const u32 word = bitpos >> (ELEMENT_BITS == 64 ? 6 : ELEMENT_BITS == 32 ? 5
                                                                                : 4); // div by ELEMENT_BITS
        const u32 shift = bitpos & (ELEMENT_BITS - 1);                                // mod ELEMENT_BITS

        // Read value from vectorized layout: word * ELEMENTS_PER_VECTOR + pos
        T val = in[word * ELEMENTS_PER_VECTOR + pos] >> shift;

        if constexpr (ELEMENT_BITS % Bits != 0)
        {
            constexpr u32 threshold = ELEMENT_BITS - Bits;
            T hi = in[(word + 1) * ELEMENTS_PER_VECTOR + pos] << (ELEMENT_BITS - shift);
            val |= (shift > threshold) ? hi : 0;
        }

        return val & mask;
    }
}

// template <typename T, u32 Bits, u32 IDX>
// static inline T UnPackSingleTemp(const T *compressed)
// {
//     static_assert(Bits >= 0, "Bits must be >= 0");

//     if constexpr (Bits == 0)
//     {
//         return 0;
//     }
//     else
//     {
//         using traits = avx_traits<T>;
//         constexpr u32 ELEMENT_BITS = traits::element_bits;
//         constexpr u32 ELEMENTS_PER_VECTOR = traits::elements_per_vector;
//         constexpr T mask = (Bits == ELEMENT_BITS) ? T(~T(0)) : (T(1) << Bits) - 1;

//         // Which 256-element block
//         constexpr u32 block = IDX / 256; // idx / 256

//         // Offset to start of this block (each block uses Bits * ELEMENTS_PER_VECTOR words)
//         auto in = compressed + block * Bits * ELEMENTS_PER_VECTOR;

//         // Lane and position within vector
//         constexpr u32 lane = (IDX / ELEMENTS_PER_VECTOR) % ELEMENT_BITS; // Which of 32 lanes (0-31)
//         constexpr u32 pos = IDX % ELEMENTS_PER_VECTOR;                   // Position within lane (0-7)

//         // Calculate bit position for this lane
//         constexpr u32 bitpos = lane * Bits;

//         // Which word and bit offset within that word
//         constexpr u32 word = bitpos >> (ELEMENT_BITS == 64 ? 6 : ELEMENT_BITS == 32 ? 5
//                                                                                     : 4); // div by ELEMENT_BITS
//         constexpr u32 shift = bitpos & (ELEMENT_BITS - 1);                                // mod ELEMENT_BITS

//         // Read value from vectorized layout: word * ELEMENTS_PER_VECTOR + pos
//         T val = in[word * ELEMENTS_PER_VECTOR + pos] >> shift;

//         if constexpr (ELEMENT_BITS % Bits != 0)
//         {
//             constexpr u32 threshold = ELEMENT_BITS - Bits;
//             if constexpr (shift > threshold) // <-- use constexpr if since shift is constexpr
//             {
//                 T hi = in[(word + 1) * ELEMENTS_PER_VECTOR + pos] << (ELEMENT_BITS - shift);
//                 val |= hi;
//             }
//         }

//         return val & mask;
//     }
// }

// template <typename T, u32 BITS, u32 IDX = 0>
// static void AvxUnPackBlockTemp(const T *compressed, T *pout)
// {
//     if constexpr (IDX < 256)
//     {
//         pout[IDX] = UnPackSingleTemp<T, BITS, IDX>(compressed);
//         AvxUnPackBlockTemp<T, BITS, IDX + 1>(compressed, pout);
//     }
// }

template <typename T, u32 Bits>
T *ScalarPackDef(T *__restrict out, const T *__restrict in, const u32 nitems)
{
    using ptype = u64;

    constexpr u32 MAX_USED_BITS = sizeof(ptype) * 8;
    static_assert(Bits <= MAX_USED_BITS);

    if constexpr (Bits == MAX_USED_BITS)
    {
        memcpy(out, in, nitems * sizeof(T));
        return out + nitems;
    }
    else if constexpr (Bits == 0)
    {
        // skip
        return out;
    }
    else
    {
        const ptype mask = (ptype(1) << Bits) - 1;
        ptype *result = reinterpret_cast<ptype *>(out);
        i8 shift = MAX_USED_BITS - Bits;
        ptype pack = 0;
        for (u32 i = 0; i < nitems; i++, shift -= Bits)
        {
            ptype item = ((ptype)in[i]) & mask;

            if (shift < 0)
            {
                i8 pos_shift = -1 * shift;
                ptype hi = item >> (pos_shift);
                ptype lo = item & ((1ull << pos_shift) - 1);

                pack |= hi;
                *(result++) = pack;

                shift = MAX_USED_BITS - pos_shift;
                pack = lo << shift;
                continue;
            }

            pack |= item << shift;
        }

        *(result++) = pack;

        return reinterpret_cast<T *>(result);
    }
}

template <typename T, u32 Bits>
T *ScalarUnPackDef(T *__restrict out, const T *__restrict in, const u32 nitems)
{
    using ptype = u64;

    constexpr u32 MAX_USED_BITS = sizeof(ptype) * 8;
    static_assert(Bits <= MAX_USED_BITS);

    if constexpr (Bits == MAX_USED_BITS)
    {
        memcpy(out, in, nitems * sizeof(T));
        return out + nitems;
    }
    else if constexpr (Bits == 0)
    {
        memset(out, 0, nitems * sizeof(T));
        return out + nitems;
    }
    else
    {
        const ptype *in_u64 = (const ptype *)in;
        constexpr ptype mask = (1ull << Bits) - 1;
        i8 shift = MAX_USED_BITS - Bits;
        u32 offset = 0;

        for (u32 i = 0; i < nitems; i++, shift -= Bits)
        {

            if (shift == -i8(Bits))
            {
                shift = MAX_USED_BITS - Bits;
                offset++;
            }
            else if (shift < 0)
            {
                i8 pos_shift = -1 * shift;

                ptype hi = in_u64[offset] & ((1ull << (Bits - pos_shift)) - 1);
                ptype lo = in_u64[offset + 1] >> (MAX_USED_BITS - pos_shift);
                out[i] = (hi << pos_shift) | (lo);

                offset++;
                shift = MAX_USED_BITS - pos_shift;
                continue;
            }

            out[i] = (in_u64[offset] >> shift) & mask;
        }

        return out + nitems;
    }
}

// template <typename T, u32 BITS, u32 IDX>
// T ScalarUnPackSingle(const T *in)
// {
//     using traits = avx_traits<T>;
//     constexpr u32 ELEMENT_BITS = traits::element_bits;
//     constexpr u32 pos = (IDX * BITS) / ELEMENT_BITS;
//     constexpr u32 offset = (IDX * BITS) % ELEMENT_BITS;
//     constexpr u32 shift = BITS + offset - ELEMENT_BITS;
//     constexpr u32 mask = (1u << BITS) - 1;

//     if constexpr (ELEMENT_BITS - offset < BITS)
//     {
//         return (in[pos] << shift) | (in[pos + 1] >> (ELEMENT_BITS - shift));
//     }
//     else
//     {
//         return (in[pos] >> shift) & mask;
//     }
// }

// template <typename T, u32 BITS, u32 IDX = 0>
// T *ScalarUnPackDef2(T *__restrict out, const T *__restrict in, const u32 nitems)
// {
//     using traits = avx_traits<T>;
//     constexpr u32 ELEMENT_BITS = traits::element_bits;
//     constexpr u32 PERIOD = 64;

//     out[IDX] = ScalarUnPackSingle<T, BITS, IDX>(in);

//     if constexpr (IDX + 1 < PERIOD)
//         return ScalarUnPackDef2<T, BITS, IDX + 1>(out, in, nitems);
//     else
//         return out + PERIOD;
// }
#pragma once

#include "avx2bitpacking.hpp"

static void (*const avx_pack_functions_u16[17])(const u16 *, __m256i *) = {
    avxpackblock<u16, 0>,
    avxpackblock<u16, 1>,
    avxpackblock<u16, 2>,
    avxpackblock<u16, 3>,
    avxpackblock<u16, 4>,
    avxpackblock<u16, 5>,
    avxpackblock<u16, 6>,
    avxpackblock<u16, 7>,
    avxpackblock<u16, 8>,
    avxpackblock<u16, 9>,
    avxpackblock<u16, 10>,
    avxpackblock<u16, 11>,
    avxpackblock<u16, 12>,
    avxpackblock<u16, 13>,
    avxpackblock<u16, 14>,
    avxpackblock<u16, 15>,
    avxpackblock<u16, 16>};

static void (*const avx_pack_functions_mask_u16[17])(const u16 *, __m256i *) = {
    avxpackblock<u16, 0, true>,
    avxpackblock<u16, 1, true>,
    avxpackblock<u16, 2, true>,
    avxpackblock<u16, 3, true>,
    avxpackblock<u16, 4, true>,
    avxpackblock<u16, 5, true>,
    avxpackblock<u16, 6, true>,
    avxpackblock<u16, 7, true>,
    avxpackblock<u16, 8, true>,
    avxpackblock<u16, 9, true>,
    avxpackblock<u16, 10, true>,
    avxpackblock<u16, 11, true>,
    avxpackblock<u16, 12, true>,
    avxpackblock<u16, 13, true>,
    avxpackblock<u16, 14, true>,
    avxpackblock<u16, 15, true>,
    avxpackblock<u16, 16, true>};

static void (*const avx_unpack_functions_u16[17])(const __m256i *, u16 *) = {
    avxunpackblock<u16, 0>,
    avxunpackblock<u16, 1>,
    avxunpackblock<u16, 2>,
    avxunpackblock<u16, 3>,
    avxunpackblock<u16, 4>,
    avxunpackblock<u16, 5>,
    avxunpackblock<u16, 6>,
    avxunpackblock<u16, 7>,
    avxunpackblock<u16, 8>,
    avxunpackblock<u16, 9>,
    avxunpackblock<u16, 10>,
    avxunpackblock<u16, 11>,
    avxunpackblock<u16, 12>,
    avxunpackblock<u16, 13>,
    avxunpackblock<u16, 14>,
    avxunpackblock<u16, 15>,
    avxunpackblock<u16, 16>};

static u16 (*const avx_unpack_single_functions_u16[17])(const u16 *compressed, u32 idx) = {
    UnPackSingle<u16, 0>,
    UnPackSingle<u16, 1>,
    UnPackSingle<u16, 2>,
    UnPackSingle<u16, 3>,
    UnPackSingle<u16, 4>,
    UnPackSingle<u16, 5>,
    UnPackSingle<u16, 6>,
    UnPackSingle<u16, 7>,
    UnPackSingle<u16, 8>,
    UnPackSingle<u16, 9>,
    UnPackSingle<u16, 10>,
    UnPackSingle<u16, 11>,
    UnPackSingle<u16, 12>,
    UnPackSingle<u16, 13>,
    UnPackSingle<u16, 14>,
    UnPackSingle<u16, 15>,
    UnPackSingle<u16, 16>};

static void (*const avx_pack_functions_u32[33])(const u32 *, __m256i *) = {
    avxpackblock<u32, 0>,
    avxpackblock<u32, 1>,
    avxpackblock<u32, 2>,
    avxpackblock<u32, 3>,
    avxpackblock<u32, 4>,
    avxpackblock<u32, 5>,
    avxpackblock<u32, 6>,
    avxpackblock<u32, 7>,
    avxpackblock<u32, 8>,
    avxpackblock<u32, 9>,
    avxpackblock<u32, 10>,
    avxpackblock<u32, 11>,
    avxpackblock<u32, 12>,
    avxpackblock<u32, 13>,
    avxpackblock<u32, 14>,
    avxpackblock<u32, 15>,
    avxpackblock<u32, 16>,
    avxpackblock<u32, 17>,
    avxpackblock<u32, 18>,
    avxpackblock<u32, 19>,
    avxpackblock<u32, 20>,
    avxpackblock<u32, 21>,
    avxpackblock<u32, 22>,
    avxpackblock<u32, 23>,
    avxpackblock<u32, 24>,
    avxpackblock<u32, 25>,
    avxpackblock<u32, 26>,
    avxpackblock<u32, 27>,
    avxpackblock<u32, 28>,
    avxpackblock<u32, 29>,
    avxpackblock<u32, 30>,
    avxpackblock<u32, 31>,
    avxpackblock<u32, 32>};

static void (*const avx_pack_functions_mask_u32[33])(const u32 *, __m256i *) = {
    avxpackblock<u32, 0, true>,
    avxpackblock<u32, 1, true>,
    avxpackblock<u32, 2, true>,
    avxpackblock<u32, 3, true>,
    avxpackblock<u32, 4, true>,
    avxpackblock<u32, 5, true>,
    avxpackblock<u32, 6, true>,
    avxpackblock<u32, 7, true>,
    avxpackblock<u32, 8, true>,
    avxpackblock<u32, 9, true>,
    avxpackblock<u32, 10, true>,
    avxpackblock<u32, 11, true>,
    avxpackblock<u32, 12, true>,
    avxpackblock<u32, 13, true>,
    avxpackblock<u32, 14, true>,
    avxpackblock<u32, 15, true>,
    avxpackblock<u32, 16, true>,
    avxpackblock<u32, 17, true>,
    avxpackblock<u32, 18, true>,
    avxpackblock<u32, 19, true>,
    avxpackblock<u32, 20, true>,
    avxpackblock<u32, 21, true>,
    avxpackblock<u32, 22, true>,
    avxpackblock<u32, 23, true>,
    avxpackblock<u32, 24, true>,
    avxpackblock<u32, 25, true>,
    avxpackblock<u32, 26, true>,
    avxpackblock<u32, 27, true>,
    avxpackblock<u32, 28, true>,
    avxpackblock<u32, 29, true>,
    avxpackblock<u32, 30, true>,
    avxpackblock<u32, 31, true>,
    avxpackblock<u32, 32, true>};

static void (*const avx_unpack_functions_u32[33])(const __m256i *, u32 *) = {
    avxunpackblock<u32, 0>,
    avxunpackblock<u32, 1>,
    avxunpackblock<u32, 2>,
    avxunpackblock<u32, 3>,
    avxunpackblock<u32, 4>,
    avxunpackblock<u32, 5>,
    avxunpackblock<u32, 6>,
    avxunpackblock<u32, 7>,
    avxunpackblock<u32, 8>,
    avxunpackblock<u32, 9>,
    avxunpackblock<u32, 10>,
    avxunpackblock<u32, 11>,
    avxunpackblock<u32, 12>,
    avxunpackblock<u32, 13>,
    avxunpackblock<u32, 14>,
    avxunpackblock<u32, 15>,
    avxunpackblock<u32, 16>,
    avxunpackblock<u32, 17>,
    avxunpackblock<u32, 18>,
    avxunpackblock<u32, 19>,
    avxunpackblock<u32, 20>,
    avxunpackblock<u32, 21>,
    avxunpackblock<u32, 22>,
    avxunpackblock<u32, 23>,
    avxunpackblock<u32, 24>,
    avxunpackblock<u32, 25>,
    avxunpackblock<u32, 26>,
    avxunpackblock<u32, 27>,
    avxunpackblock<u32, 28>,
    avxunpackblock<u32, 29>,
    avxunpackblock<u32, 30>,
    avxunpackblock<u32, 31>,
    avxunpackblock<u32, 32>};

static u32 (*const avx_unpack_single_functions_u32[33])(const u32 *compressed, u32 idx) = {
    UnPackSingle<u32, 0>,
    UnPackSingle<u32, 1>,
    UnPackSingle<u32, 2>,
    UnPackSingle<u32, 3>,
    UnPackSingle<u32, 4>,
    UnPackSingle<u32, 5>,
    UnPackSingle<u32, 6>,
    UnPackSingle<u32, 7>,
    UnPackSingle<u32, 8>,
    UnPackSingle<u32, 9>,
    UnPackSingle<u32, 10>,
    UnPackSingle<u32, 11>,
    UnPackSingle<u32, 12>,
    UnPackSingle<u32, 13>,
    UnPackSingle<u32, 14>,
    UnPackSingle<u32, 15>,
    UnPackSingle<u32, 16>,
    UnPackSingle<u32, 17>,
    UnPackSingle<u32, 18>,
    UnPackSingle<u32, 19>,
    UnPackSingle<u32, 20>,
    UnPackSingle<u32, 21>,
    UnPackSingle<u32, 22>,
    UnPackSingle<u32, 23>,
    UnPackSingle<u32, 24>,
    UnPackSingle<u32, 25>,
    UnPackSingle<u32, 26>,
    UnPackSingle<u32, 27>,
    UnPackSingle<u32, 28>,
    UnPackSingle<u32, 29>,
    UnPackSingle<u32, 30>,
    UnPackSingle<u32, 31>,
    UnPackSingle<u32, 32>};

static void (*const avx_pack_functions_u64[65])(const u64 *, __m256i *) = {
    avxpackblock<u64, 0>,
    avxpackblock<u64, 1>,
    avxpackblock<u64, 2>,
    avxpackblock<u64, 3>,
    avxpackblock<u64, 4>,
    avxpackblock<u64, 5>,
    avxpackblock<u64, 6>,
    avxpackblock<u64, 7>,
    avxpackblock<u64, 8>,
    avxpackblock<u64, 9>,
    avxpackblock<u64, 10>,
    avxpackblock<u64, 11>,
    avxpackblock<u64, 12>,
    avxpackblock<u64, 13>,
    avxpackblock<u64, 14>,
    avxpackblock<u64, 15>,
    avxpackblock<u64, 16>,
    avxpackblock<u64, 17>,
    avxpackblock<u64, 18>,
    avxpackblock<u64, 19>,
    avxpackblock<u64, 20>,
    avxpackblock<u64, 21>,
    avxpackblock<u64, 22>,
    avxpackblock<u64, 23>,
    avxpackblock<u64, 24>,
    avxpackblock<u64, 25>,
    avxpackblock<u64, 26>,
    avxpackblock<u64, 27>,
    avxpackblock<u64, 28>,
    avxpackblock<u64, 29>,
    avxpackblock<u64, 30>,
    avxpackblock<u64, 31>,
    avxpackblock<u64, 32>,
    avxpackblock<u64, 33>,
    avxpackblock<u64, 34>,
    avxpackblock<u64, 35>,
    avxpackblock<u64, 36>,
    avxpackblock<u64, 37>,
    avxpackblock<u64, 38>,
    avxpackblock<u64, 39>,
    avxpackblock<u64, 40>,
    avxpackblock<u64, 41>,
    avxpackblock<u64, 42>,
    avxpackblock<u64, 43>,
    avxpackblock<u64, 44>,
    avxpackblock<u64, 45>,
    avxpackblock<u64, 46>,
    avxpackblock<u64, 47>,
    avxpackblock<u64, 48>,
    avxpackblock<u64, 49>,
    avxpackblock<u64, 50>,
    avxpackblock<u64, 51>,
    avxpackblock<u64, 52>,
    avxpackblock<u64, 53>,
    avxpackblock<u64, 54>,
    avxpackblock<u64, 55>,
    avxpackblock<u64, 56>,
    avxpackblock<u64, 57>,
    avxpackblock<u64, 58>,
    avxpackblock<u64, 59>,
    avxpackblock<u64, 60>,
    avxpackblock<u64, 61>,
    avxpackblock<u64, 62>,
    avxpackblock<u64, 63>,
    avxpackblock<u64, 64>};

static void (*const avx_pack_functions_mask_u64[65])(const u64 *, __m256i *) = {
    avxpackblock<u64, 0, true>,
    avxpackblock<u64, 1, true>,
    avxpackblock<u64, 2, true>,
    avxpackblock<u64, 3, true>,
    avxpackblock<u64, 4, true>,
    avxpackblock<u64, 5, true>,
    avxpackblock<u64, 6, true>,
    avxpackblock<u64, 7, true>,
    avxpackblock<u64, 8, true>,
    avxpackblock<u64, 9, true>,
    avxpackblock<u64, 10, true>,
    avxpackblock<u64, 11, true>,
    avxpackblock<u64, 12, true>,
    avxpackblock<u64, 13, true>,
    avxpackblock<u64, 14, true>,
    avxpackblock<u64, 15, true>,
    avxpackblock<u64, 16, true>,
    avxpackblock<u64, 17, true>,
    avxpackblock<u64, 18, true>,
    avxpackblock<u64, 19, true>,
    avxpackblock<u64, 20, true>,
    avxpackblock<u64, 21, true>,
    avxpackblock<u64, 22, true>,
    avxpackblock<u64, 23, true>,
    avxpackblock<u64, 24, true>,
    avxpackblock<u64, 25, true>,
    avxpackblock<u64, 26, true>,
    avxpackblock<u64, 27, true>,
    avxpackblock<u64, 28, true>,
    avxpackblock<u64, 29, true>,
    avxpackblock<u64, 30, true>,
    avxpackblock<u64, 31, true>,
    avxpackblock<u64, 32, true>,
    avxpackblock<u64, 33, true>,
    avxpackblock<u64, 34, true>,
    avxpackblock<u64, 35, true>,
    avxpackblock<u64, 36, true>,
    avxpackblock<u64, 37, true>,
    avxpackblock<u64, 38, true>,
    avxpackblock<u64, 39, true>,
    avxpackblock<u64, 40, true>,
    avxpackblock<u64, 41, true>,
    avxpackblock<u64, 42, true>,
    avxpackblock<u64, 43, true>,
    avxpackblock<u64, 44, true>,
    avxpackblock<u64, 45, true>,
    avxpackblock<u64, 46, true>,
    avxpackblock<u64, 47, true>,
    avxpackblock<u64, 48, true>,
    avxpackblock<u64, 49, true>,
    avxpackblock<u64, 50, true>,
    avxpackblock<u64, 51, true>,
    avxpackblock<u64, 52, true>,
    avxpackblock<u64, 53, true>,
    avxpackblock<u64, 54, true>,
    avxpackblock<u64, 55, true>,
    avxpackblock<u64, 56, true>,
    avxpackblock<u64, 57, true>,
    avxpackblock<u64, 58, true>,
    avxpackblock<u64, 59, true>,
    avxpackblock<u64, 60, true>,
    avxpackblock<u64, 61, true>,
    avxpackblock<u64, 62, true>,
    avxpackblock<u64, 63, true>,
    avxpackblock<u64, 64, true>};

static void (*const avx_unpack_functions_u64[65])(const __m256i *, u64 *) = {
    avxunpackblock<u64, 0>,
    avxunpackblock<u64, 1>,
    avxunpackblock<u64, 2>,
    avxunpackblock<u64, 3>,
    avxunpackblock<u64, 4>,
    avxunpackblock<u64, 5>,
    avxunpackblock<u64, 6>,
    avxunpackblock<u64, 7>,
    avxunpackblock<u64, 8>,
    avxunpackblock<u64, 9>,
    avxunpackblock<u64, 10>,
    avxunpackblock<u64, 11>,
    avxunpackblock<u64, 12>,
    avxunpackblock<u64, 13>,
    avxunpackblock<u64, 14>,
    avxunpackblock<u64, 15>,
    avxunpackblock<u64, 16>,
    avxunpackblock<u64, 17>,
    avxunpackblock<u64, 18>,
    avxunpackblock<u64, 19>,
    avxunpackblock<u64, 20>,
    avxunpackblock<u64, 21>,
    avxunpackblock<u64, 22>,
    avxunpackblock<u64, 23>,
    avxunpackblock<u64, 24>,
    avxunpackblock<u64, 25>,
    avxunpackblock<u64, 26>,
    avxunpackblock<u64, 27>,
    avxunpackblock<u64, 28>,
    avxunpackblock<u64, 29>,
    avxunpackblock<u64, 30>,
    avxunpackblock<u64, 31>,
    avxunpackblock<u64, 32>,
    avxunpackblock<u64, 33>,
    avxunpackblock<u64, 34>,
    avxunpackblock<u64, 35>,
    avxunpackblock<u64, 36>,
    avxunpackblock<u64, 37>,
    avxunpackblock<u64, 38>,
    avxunpackblock<u64, 39>,
    avxunpackblock<u64, 40>,
    avxunpackblock<u64, 41>,
    avxunpackblock<u64, 42>,
    avxunpackblock<u64, 43>,
    avxunpackblock<u64, 44>,
    avxunpackblock<u64, 45>,
    avxunpackblock<u64, 46>,
    avxunpackblock<u64, 47>,
    avxunpackblock<u64, 48>,
    avxunpackblock<u64, 49>,
    avxunpackblock<u64, 50>,
    avxunpackblock<u64, 51>,
    avxunpackblock<u64, 52>,
    avxunpackblock<u64, 53>,
    avxunpackblock<u64, 54>,
    avxunpackblock<u64, 55>,
    avxunpackblock<u64, 56>,
    avxunpackblock<u64, 57>,
    avxunpackblock<u64, 58>,
    avxunpackblock<u64, 59>,
    avxunpackblock<u64, 60>,
    avxunpackblock<u64, 61>,
    avxunpackblock<u64, 62>,
    avxunpackblock<u64, 63>,
    avxunpackblock<u64, 64>};

static u64 (*const avx_unpack_single_functions_u64[65])(const u64 *compressed, u32 idx) = {
    UnPackSingle<u64, 0>,
    UnPackSingle<u64, 1>,
    UnPackSingle<u64, 2>,
    UnPackSingle<u64, 3>,
    UnPackSingle<u64, 4>,
    UnPackSingle<u64, 5>,
    UnPackSingle<u64, 6>,
    UnPackSingle<u64, 7>,
    UnPackSingle<u64, 8>,
    UnPackSingle<u64, 9>,
    UnPackSingle<u64, 10>,
    UnPackSingle<u64, 11>,
    UnPackSingle<u64, 12>,
    UnPackSingle<u64, 13>,
    UnPackSingle<u64, 14>,
    UnPackSingle<u64, 15>,
    UnPackSingle<u64, 16>,
    UnPackSingle<u64, 17>,
    UnPackSingle<u64, 18>,
    UnPackSingle<u64, 19>,
    UnPackSingle<u64, 20>,
    UnPackSingle<u64, 21>,
    UnPackSingle<u64, 22>,
    UnPackSingle<u64, 23>,
    UnPackSingle<u64, 24>,
    UnPackSingle<u64, 25>,
    UnPackSingle<u64, 26>,
    UnPackSingle<u64, 27>,
    UnPackSingle<u64, 28>,
    UnPackSingle<u64, 29>,
    UnPackSingle<u64, 30>,
    UnPackSingle<u64, 31>,
    UnPackSingle<u64, 32>,
    UnPackSingle<u64, 33>,
    UnPackSingle<u64, 34>,
    UnPackSingle<u64, 35>,
    UnPackSingle<u64, 36>,
    UnPackSingle<u64, 37>,
    UnPackSingle<u64, 38>,
    UnPackSingle<u64, 39>,
    UnPackSingle<u64, 40>,
    UnPackSingle<u64, 41>,
    UnPackSingle<u64, 42>,
    UnPackSingle<u64, 43>,
    UnPackSingle<u64, 44>,
    UnPackSingle<u64, 45>,
    UnPackSingle<u64, 46>,
    UnPackSingle<u64, 47>,
    UnPackSingle<u64, 48>,
    UnPackSingle<u64, 49>,
    UnPackSingle<u64, 50>,
    UnPackSingle<u64, 51>,
    UnPackSingle<u64, 52>,
    UnPackSingle<u64, 53>,
    UnPackSingle<u64, 54>,
    UnPackSingle<u64, 55>,
    UnPackSingle<u64, 56>,
    UnPackSingle<u64, 57>,
    UnPackSingle<u64, 58>,
    UnPackSingle<u64, 59>,
    UnPackSingle<u64, 60>,
    UnPackSingle<u64, 61>,
    UnPackSingle<u64, 62>,
    UnPackSingle<u64, 63>,
    UnPackSingle<u64, 64>};

template <typename T, bool USE_MASK = false>
static inline auto GetPackFunction(u32 bit) -> void (*)(const T *, __m256i *)
{
    static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>,
                  "AvxPack only supports u16, u32, and u64 types");

    if constexpr (std::is_same_v<T, u16>)
    {
        return (USE_MASK ? avx_pack_functions_mask_u16 : avx_pack_functions_u16)[bit];
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return (USE_MASK ? avx_pack_functions_mask_u32 : avx_pack_functions_u32)[bit];
    }
    else
    {
        return (USE_MASK ? avx_pack_functions_mask_u64 : avx_pack_functions_u64)[bit];
    }
}

template <typename T>
static inline auto GetUnPackFunction(u32 bit) -> void (*)(const __m256i *, T *)
{
    static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>,
                  "AvxPack only supports u16, u32, and u64 types");

    if constexpr (std::is_same_v<T, u16>)
    {
        return avx_unpack_functions_u16[bit];
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return avx_unpack_functions_u32[bit];
    }
    else
    {
        return avx_unpack_functions_u64[bit];
    }
}

template <typename T>
static inline auto GetUnPackSingleFunction(u32 bit) -> T (*)(const T *, u32)
{
    static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>,
                  "AvxPack only supports u16, u32, and u64 types");

    if constexpr (std::is_same_v<T, u16>)
    {
        return avx_unpack_single_functions_u16[bit];
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return avx_unpack_single_functions_u32[bit];
    }
    else
    {
        return avx_unpack_single_functions_u64[bit];
    }
}

template <typename T, bool USE_MASK = false>
inline T *AvxPack(const T *in, __m256i *out, const u32 number, const u32 bit)
{
    assert(reinterpret_cast<uintptr_t>(in) % alignof(T) == 0 && "Input not aligned");
    assert(reinterpret_cast<uintptr_t>(out) % 4 == 0 && "Output not 4-byte aligned");

    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in pack");

    auto func = GetPackFunction<T, USE_MASK>(bit);

    for (u32 i = 0; i < number / 256; ++i)
    {
        func(in + i * 256, out);
        out += bit;
    }
    return (T *)out;
}

template <typename T>
inline T *AvxUnPack(const __m256i *in, T *out, const u32 number, const u32 bit)
{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack");

    auto func = GetUnPackFunction<T>(bit);

    for (u32 i = 0; i < number / 256; ++i)
    {
        func(in + i * bit, out);
        out += 256;
    }
    return (T *)out;
}

template <typename T>
inline auto AvxUnPackSingleFun(const u32 bit) -> T (*)(const T *, u32)
{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack single");

    return GetUnPackSingleFunction<T>(bit);
}
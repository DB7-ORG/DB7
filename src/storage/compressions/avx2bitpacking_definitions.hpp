#pragma once

#include "avx2bitpacking_functions.hpp"

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

template <typename T>
static inline auto GetScalarPackDef(u32 bit) -> T *(*)(T * out, const T *in, const u32 nitems)
{
    static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>,
                  "AvxPack only supports u16, u32, and u64 types");

    if constexpr (std::is_same_v<T, u16>)
    {
        return pack_scalar_functions_u16[bit];
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return pack_scalar_functions_u32[bit];
    }
    else
    {
        return pack_scalar_functions_u64[bit];
    }
}

template <typename T>
static inline auto GetScalarUnPackDef(u32 bit) -> T *(*)(T * out, const T *in, const u32 nitems)
{
    static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>,
                  "AvxPack only supports u16, u32, and u64 types");

    if constexpr (std::is_same_v<T, u16>)
    {
        return unpack_scalar_functions_u16[bit];
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return unpack_scalar_functions_u32[bit];
    }
    else
    {
        return unpack_scalar_functions_u64[bit];
    }
}

template <typename T, bool USE_MASK = false>
inline T *AvxPack(const T *__restrict in, __m256i *__restrict out, const u32 number, const u32 bit)
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
inline T *AvxUnPack(const __m256i *__restrict in, T *__restrict out, const u32 number, const u32 bit)
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

template <typename T>
inline T *ScalarPack(T *__restrict out, const T *__restrict in, const u32 nitems, const u32 bit)

{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack single");

    auto func = GetScalarPackDef<T>(bit);
    return func(out, in, nitems);
}

template <typename T>
inline T *ScalarUnPack(T *__restrict out, const T *__restrict in, const u32 nitems, const u32 bit)
{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack single");

    auto func = GetScalarUnPackDef<T>(bit);
    return func(out, in, nitems);
}
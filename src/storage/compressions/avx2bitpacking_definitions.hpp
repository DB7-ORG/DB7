#pragma once

#include "avx2bitpacking.hpp"

static void (*const avx_pack_functions_u16[17])(const u16 *, __m256i *) = {
    AvxPackBlock<u16, 0>,
    AvxPackBlock<u16, 1>,
    AvxPackBlock<u16, 2>,
    AvxPackBlock<u16, 3>,
    AvxPackBlock<u16, 4>,
    AvxPackBlock<u16, 5>,
    AvxPackBlock<u16, 6>,
    AvxPackBlock<u16, 7>,
    AvxPackBlock<u16, 8>,
    AvxPackBlock<u16, 9>,
    AvxPackBlock<u16, 10>,
    AvxPackBlock<u16, 11>,
    AvxPackBlock<u16, 12>,
    AvxPackBlock<u16, 13>,
    AvxPackBlock<u16, 14>,
    AvxPackBlock<u16, 15>,
    AvxPackBlock<u16, 16>};

static void (*const avx_pack_functions_mask_u16[17])(const u16 *, __m256i *) = {
    AvxPackBlock<u16, 0, true>,
    AvxPackBlock<u16, 1, true>,
    AvxPackBlock<u16, 2, true>,
    AvxPackBlock<u16, 3, true>,
    AvxPackBlock<u16, 4, true>,
    AvxPackBlock<u16, 5, true>,
    AvxPackBlock<u16, 6, true>,
    AvxPackBlock<u16, 7, true>,
    AvxPackBlock<u16, 8, true>,
    AvxPackBlock<u16, 9, true>,
    AvxPackBlock<u16, 10, true>,
    AvxPackBlock<u16, 11, true>,
    AvxPackBlock<u16, 12, true>,
    AvxPackBlock<u16, 13, true>,
    AvxPackBlock<u16, 14, true>,
    AvxPackBlock<u16, 15, true>,
    AvxPackBlock<u16, 16, true>};

static void (*const avx_unpack_functions_u16[17])(const __m256i *, u16 *) = {
    AvxUnPackBlock<u16, 0>,
    AvxUnPackBlock<u16, 1>,
    AvxUnPackBlock<u16, 2>,
    AvxUnPackBlock<u16, 3>,
    AvxUnPackBlock<u16, 4>,
    AvxUnPackBlock<u16, 5>,
    AvxUnPackBlock<u16, 6>,
    AvxUnPackBlock<u16, 7>,
    AvxUnPackBlock<u16, 8>,
    AvxUnPackBlock<u16, 9>,
    AvxUnPackBlock<u16, 10>,
    AvxUnPackBlock<u16, 11>,
    AvxUnPackBlock<u16, 12>,
    AvxUnPackBlock<u16, 13>,
    AvxUnPackBlock<u16, 14>,
    AvxUnPackBlock<u16, 15>,
    AvxUnPackBlock<u16, 16>};

static u16 (*const avx_unpack_single_functions_u16[17])(const u16 *, u32) = {
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
    AvxPackBlock<u32, 0>,
    AvxPackBlock<u32, 1>,
    AvxPackBlock<u32, 2>,
    AvxPackBlock<u32, 3>,
    AvxPackBlock<u32, 4>,
    AvxPackBlock<u32, 5>,
    AvxPackBlock<u32, 6>,
    AvxPackBlock<u32, 7>,
    AvxPackBlock<u32, 8>,
    AvxPackBlock<u32, 9>,
    AvxPackBlock<u32, 10>,
    AvxPackBlock<u32, 11>,
    AvxPackBlock<u32, 12>,
    AvxPackBlock<u32, 13>,
    AvxPackBlock<u32, 14>,
    AvxPackBlock<u32, 15>,
    AvxPackBlock<u32, 16>,
    AvxPackBlock<u32, 17>,
    AvxPackBlock<u32, 18>,
    AvxPackBlock<u32, 19>,
    AvxPackBlock<u32, 20>,
    AvxPackBlock<u32, 21>,
    AvxPackBlock<u32, 22>,
    AvxPackBlock<u32, 23>,
    AvxPackBlock<u32, 24>,
    AvxPackBlock<u32, 25>,
    AvxPackBlock<u32, 26>,
    AvxPackBlock<u32, 27>,
    AvxPackBlock<u32, 28>,
    AvxPackBlock<u32, 29>,
    AvxPackBlock<u32, 30>,
    AvxPackBlock<u32, 31>,
    AvxPackBlock<u32, 32>};

static void (*const avx_pack_functions_mask_u32[33])(const u32 *, __m256i *) = {
    AvxPackBlock<u32, 0, true>,
    AvxPackBlock<u32, 1, true>,
    AvxPackBlock<u32, 2, true>,
    AvxPackBlock<u32, 3, true>,
    AvxPackBlock<u32, 4, true>,
    AvxPackBlock<u32, 5, true>,
    AvxPackBlock<u32, 6, true>,
    AvxPackBlock<u32, 7, true>,
    AvxPackBlock<u32, 8, true>,
    AvxPackBlock<u32, 9, true>,
    AvxPackBlock<u32, 10, true>,
    AvxPackBlock<u32, 11, true>,
    AvxPackBlock<u32, 12, true>,
    AvxPackBlock<u32, 13, true>,
    AvxPackBlock<u32, 14, true>,
    AvxPackBlock<u32, 15, true>,
    AvxPackBlock<u32, 16, true>,
    AvxPackBlock<u32, 17, true>,
    AvxPackBlock<u32, 18, true>,
    AvxPackBlock<u32, 19, true>,
    AvxPackBlock<u32, 20, true>,
    AvxPackBlock<u32, 21, true>,
    AvxPackBlock<u32, 22, true>,
    AvxPackBlock<u32, 23, true>,
    AvxPackBlock<u32, 24, true>,
    AvxPackBlock<u32, 25, true>,
    AvxPackBlock<u32, 26, true>,
    AvxPackBlock<u32, 27, true>,
    AvxPackBlock<u32, 28, true>,
    AvxPackBlock<u32, 29, true>,
    AvxPackBlock<u32, 30, true>,
    AvxPackBlock<u32, 31, true>,
    AvxPackBlock<u32, 32, true>};

static void (*const avx_unpack_functions_u32[33])(const __m256i *, u32 *) = {
    AvxUnPackBlock<u32, 0>,
    AvxUnPackBlock<u32, 1>,
    AvxUnPackBlock<u32, 2>,
    AvxUnPackBlock<u32, 3>,
    AvxUnPackBlock<u32, 4>,
    AvxUnPackBlock<u32, 5>,
    AvxUnPackBlock<u32, 6>,
    AvxUnPackBlock<u32, 7>,
    AvxUnPackBlock<u32, 8>,
    AvxUnPackBlock<u32, 9>,
    AvxUnPackBlock<u32, 10>,
    AvxUnPackBlock<u32, 11>,
    AvxUnPackBlock<u32, 12>,
    AvxUnPackBlock<u32, 13>,
    AvxUnPackBlock<u32, 14>,
    AvxUnPackBlock<u32, 15>,
    AvxUnPackBlock<u32, 16>,
    AvxUnPackBlock<u32, 17>,
    AvxUnPackBlock<u32, 18>,
    AvxUnPackBlock<u32, 19>,
    AvxUnPackBlock<u32, 20>,
    AvxUnPackBlock<u32, 21>,
    AvxUnPackBlock<u32, 22>,
    AvxUnPackBlock<u32, 23>,
    AvxUnPackBlock<u32, 24>,
    AvxUnPackBlock<u32, 25>,
    AvxUnPackBlock<u32, 26>,
    AvxUnPackBlock<u32, 27>,
    AvxUnPackBlock<u32, 28>,
    AvxUnPackBlock<u32, 29>,
    AvxUnPackBlock<u32, 30>,
    AvxUnPackBlock<u32, 31>,
    AvxUnPackBlock<u32, 32>};

static u32 (*const avx_unpack_single_functions_u32[33])(const u32 *, u32) = {
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
    AvxPackBlock<u64, 0>,
    AvxPackBlock<u64, 1>,
    AvxPackBlock<u64, 2>,
    AvxPackBlock<u64, 3>,
    AvxPackBlock<u64, 4>,
    AvxPackBlock<u64, 5>,
    AvxPackBlock<u64, 6>,
    AvxPackBlock<u64, 7>,
    AvxPackBlock<u64, 8>,
    AvxPackBlock<u64, 9>,
    AvxPackBlock<u64, 10>,
    AvxPackBlock<u64, 11>,
    AvxPackBlock<u64, 12>,
    AvxPackBlock<u64, 13>,
    AvxPackBlock<u64, 14>,
    AvxPackBlock<u64, 15>,
    AvxPackBlock<u64, 16>,
    AvxPackBlock<u64, 17>,
    AvxPackBlock<u64, 18>,
    AvxPackBlock<u64, 19>,
    AvxPackBlock<u64, 20>,
    AvxPackBlock<u64, 21>,
    AvxPackBlock<u64, 22>,
    AvxPackBlock<u64, 23>,
    AvxPackBlock<u64, 24>,
    AvxPackBlock<u64, 25>,
    AvxPackBlock<u64, 26>,
    AvxPackBlock<u64, 27>,
    AvxPackBlock<u64, 28>,
    AvxPackBlock<u64, 29>,
    AvxPackBlock<u64, 30>,
    AvxPackBlock<u64, 31>,
    AvxPackBlock<u64, 32>,
    AvxPackBlock<u64, 33>,
    AvxPackBlock<u64, 34>,
    AvxPackBlock<u64, 35>,
    AvxPackBlock<u64, 36>,
    AvxPackBlock<u64, 37>,
    AvxPackBlock<u64, 38>,
    AvxPackBlock<u64, 39>,
    AvxPackBlock<u64, 40>,
    AvxPackBlock<u64, 41>,
    AvxPackBlock<u64, 42>,
    AvxPackBlock<u64, 43>,
    AvxPackBlock<u64, 44>,
    AvxPackBlock<u64, 45>,
    AvxPackBlock<u64, 46>,
    AvxPackBlock<u64, 47>,
    AvxPackBlock<u64, 48>,
    AvxPackBlock<u64, 49>,
    AvxPackBlock<u64, 50>,
    AvxPackBlock<u64, 51>,
    AvxPackBlock<u64, 52>,
    AvxPackBlock<u64, 53>,
    AvxPackBlock<u64, 54>,
    AvxPackBlock<u64, 55>,
    AvxPackBlock<u64, 56>,
    AvxPackBlock<u64, 57>,
    AvxPackBlock<u64, 58>,
    AvxPackBlock<u64, 59>,
    AvxPackBlock<u64, 60>,
    AvxPackBlock<u64, 61>,
    AvxPackBlock<u64, 62>,
    AvxPackBlock<u64, 63>,
    AvxPackBlock<u64, 64>};

static void (*const avx_pack_functions_mask_u64[65])(const u64 *, __m256i *) = {
    AvxPackBlock<u64, 0, true>,
    AvxPackBlock<u64, 1, true>,
    AvxPackBlock<u64, 2, true>,
    AvxPackBlock<u64, 3, true>,
    AvxPackBlock<u64, 4, true>,
    AvxPackBlock<u64, 5, true>,
    AvxPackBlock<u64, 6, true>,
    AvxPackBlock<u64, 7, true>,
    AvxPackBlock<u64, 8, true>,
    AvxPackBlock<u64, 9, true>,
    AvxPackBlock<u64, 10, true>,
    AvxPackBlock<u64, 11, true>,
    AvxPackBlock<u64, 12, true>,
    AvxPackBlock<u64, 13, true>,
    AvxPackBlock<u64, 14, true>,
    AvxPackBlock<u64, 15, true>,
    AvxPackBlock<u64, 16, true>,
    AvxPackBlock<u64, 17, true>,
    AvxPackBlock<u64, 18, true>,
    AvxPackBlock<u64, 19, true>,
    AvxPackBlock<u64, 20, true>,
    AvxPackBlock<u64, 21, true>,
    AvxPackBlock<u64, 22, true>,
    AvxPackBlock<u64, 23, true>,
    AvxPackBlock<u64, 24, true>,
    AvxPackBlock<u64, 25, true>,
    AvxPackBlock<u64, 26, true>,
    AvxPackBlock<u64, 27, true>,
    AvxPackBlock<u64, 28, true>,
    AvxPackBlock<u64, 29, true>,
    AvxPackBlock<u64, 30, true>,
    AvxPackBlock<u64, 31, true>,
    AvxPackBlock<u64, 32, true>,
    AvxPackBlock<u64, 33, true>,
    AvxPackBlock<u64, 34, true>,
    AvxPackBlock<u64, 35, true>,
    AvxPackBlock<u64, 36, true>,
    AvxPackBlock<u64, 37, true>,
    AvxPackBlock<u64, 38, true>,
    AvxPackBlock<u64, 39, true>,
    AvxPackBlock<u64, 40, true>,
    AvxPackBlock<u64, 41, true>,
    AvxPackBlock<u64, 42, true>,
    AvxPackBlock<u64, 43, true>,
    AvxPackBlock<u64, 44, true>,
    AvxPackBlock<u64, 45, true>,
    AvxPackBlock<u64, 46, true>,
    AvxPackBlock<u64, 47, true>,
    AvxPackBlock<u64, 48, true>,
    AvxPackBlock<u64, 49, true>,
    AvxPackBlock<u64, 50, true>,
    AvxPackBlock<u64, 51, true>,
    AvxPackBlock<u64, 52, true>,
    AvxPackBlock<u64, 53, true>,
    AvxPackBlock<u64, 54, true>,
    AvxPackBlock<u64, 55, true>,
    AvxPackBlock<u64, 56, true>,
    AvxPackBlock<u64, 57, true>,
    AvxPackBlock<u64, 58, true>,
    AvxPackBlock<u64, 59, true>,
    AvxPackBlock<u64, 60, true>,
    AvxPackBlock<u64, 61, true>,
    AvxPackBlock<u64, 62, true>,
    AvxPackBlock<u64, 63, true>,
    AvxPackBlock<u64, 64, true>};

static void (*const avx_unpack_functions_u64[65])(const __m256i *, u64 *) = {
    AvxUnPackBlock<u64, 0>,
    AvxUnPackBlock<u64, 1>,
    AvxUnPackBlock<u64, 2>,
    AvxUnPackBlock<u64, 3>,
    AvxUnPackBlock<u64, 4>,
    AvxUnPackBlock<u64, 5>,
    AvxUnPackBlock<u64, 6>,
    AvxUnPackBlock<u64, 7>,
    AvxUnPackBlock<u64, 8>,
    AvxUnPackBlock<u64, 9>,
    AvxUnPackBlock<u64, 10>,
    AvxUnPackBlock<u64, 11>,
    AvxUnPackBlock<u64, 12>,
    AvxUnPackBlock<u64, 13>,
    AvxUnPackBlock<u64, 14>,
    AvxUnPackBlock<u64, 15>,
    AvxUnPackBlock<u64, 16>,
    AvxUnPackBlock<u64, 17>,
    AvxUnPackBlock<u64, 18>,
    AvxUnPackBlock<u64, 19>,
    AvxUnPackBlock<u64, 20>,
    AvxUnPackBlock<u64, 21>,
    AvxUnPackBlock<u64, 22>,
    AvxUnPackBlock<u64, 23>,
    AvxUnPackBlock<u64, 24>,
    AvxUnPackBlock<u64, 25>,
    AvxUnPackBlock<u64, 26>,
    AvxUnPackBlock<u64, 27>,
    AvxUnPackBlock<u64, 28>,
    AvxUnPackBlock<u64, 29>,
    AvxUnPackBlock<u64, 30>,
    AvxUnPackBlock<u64, 31>,
    AvxUnPackBlock<u64, 32>,
    AvxUnPackBlock<u64, 33>,
    AvxUnPackBlock<u64, 34>,
    AvxUnPackBlock<u64, 35>,
    AvxUnPackBlock<u64, 36>,
    AvxUnPackBlock<u64, 37>,
    AvxUnPackBlock<u64, 38>,
    AvxUnPackBlock<u64, 39>,
    AvxUnPackBlock<u64, 40>,
    AvxUnPackBlock<u64, 41>,
    AvxUnPackBlock<u64, 42>,
    AvxUnPackBlock<u64, 43>,
    AvxUnPackBlock<u64, 44>,
    AvxUnPackBlock<u64, 45>,
    AvxUnPackBlock<u64, 46>,
    AvxUnPackBlock<u64, 47>,
    AvxUnPackBlock<u64, 48>,
    AvxUnPackBlock<u64, 49>,
    AvxUnPackBlock<u64, 50>,
    AvxUnPackBlock<u64, 51>,
    AvxUnPackBlock<u64, 52>,
    AvxUnPackBlock<u64, 53>,
    AvxUnPackBlock<u64, 54>,
    AvxUnPackBlock<u64, 55>,
    AvxUnPackBlock<u64, 56>,
    AvxUnPackBlock<u64, 57>,
    AvxUnPackBlock<u64, 58>,
    AvxUnPackBlock<u64, 59>,
    AvxUnPackBlock<u64, 60>,
    AvxUnPackBlock<u64, 61>,
    AvxUnPackBlock<u64, 62>,
    AvxUnPackBlock<u64, 63>,
    AvxUnPackBlock<u64, 64>};

static u64 (*const avx_unpack_single_functions_u64[65])(const u64 *, u32) = {
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

static u64 *(*const avx_pack_scalar_single_functions_u32[65])(u64 *, const u64 *, const u32) = {
    ScalarPack<u64, 0>,
    ScalarPack<u64, 1>,
    ScalarPack<u64, 2>,
    ScalarPack<u64, 3>,
    ScalarPack<u64, 4>,
    ScalarPack<u64, 5>,
    ScalarPack<u64, 6>,
    ScalarPack<u64, 7>,
    ScalarPack<u64, 8>,
    ScalarPack<u64, 9>,
    ScalarPack<u64, 10>,
    ScalarPack<u64, 11>,
    ScalarPack<u64, 12>,
    ScalarPack<u64, 13>,
    ScalarPack<u64, 14>,
    ScalarPack<u64, 15>,
    ScalarPack<u64, 16>,
    ScalarPack<u64, 17>,
    ScalarPack<u64, 18>,
    ScalarPack<u64, 19>,
    ScalarPack<u64, 20>,
    ScalarPack<u64, 21>,
    ScalarPack<u64, 22>,
    ScalarPack<u64, 23>,
    ScalarPack<u64, 24>,
    ScalarPack<u64, 25>,
    ScalarPack<u64, 26>,
    ScalarPack<u64, 27>,
    ScalarPack<u64, 28>,
    ScalarPack<u64, 29>,
    ScalarPack<u64, 30>,
    ScalarPack<u64, 31>,
    ScalarPack<u64, 32>,
    ScalarPack<u64, 33>,
    ScalarPack<u64, 34>,
    ScalarPack<u64, 35>,
    ScalarPack<u64, 36>,
    ScalarPack<u64, 37>,
    ScalarPack<u64, 38>,
    ScalarPack<u64, 39>,
    ScalarPack<u64, 40>,
    ScalarPack<u64, 41>,
    ScalarPack<u64, 42>,
    ScalarPack<u64, 43>,
    ScalarPack<u64, 44>,
    ScalarPack<u64, 45>,
    ScalarPack<u64, 46>,
    ScalarPack<u64, 47>,
    ScalarPack<u64, 48>,
    ScalarPack<u64, 49>,
    ScalarPack<u64, 50>,
    ScalarPack<u64, 51>,
    ScalarPack<u64, 52>,
    ScalarPack<u64, 53>,
    ScalarPack<u64, 54>,
    ScalarPack<u64, 55>,
    ScalarPack<u64, 56>,
    ScalarPack<u64, 57>,
    ScalarPack<u64, 58>,
    ScalarPack<u64, 59>,
    ScalarPack<u64, 60>,
    ScalarPack<u64, 61>,
    ScalarPack<u64, 62>,
    ScalarPack<u64, 63>,
    ScalarPack<u64, 64>};

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
static inline auto GetScalarPack(u32 bit) -> void (*)(T *out, const T *in, const u32 nitems)
{
    static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32> || std::is_same_v<T, u64>,
                  "AvxPack only supports u16, u32, and u64 types");

    if constexpr (std::is_same_v<T, u16>)
    {
        return avx_pack_scalar_single_functions_u16[bit];
    }
    else if constexpr (std::is_same_v<T, u32>)
    {
        return avx_pack_scalar_single_functions_u32[bit];
    }
    else
    {
        return avx_pack_scalar_single_functions_u64[bit];
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

template <typename T>
inline T *ScalarPack(const u32 bit)
{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack single");

    return GetScalarPack<T>(bit);
}
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

static u16 *(*const pack_scalar_functions_u16[17])(u16 *, const u16 *, const u32) = {
    ScalarPackDef<u16, 0>,
    ScalarPackDef<u16, 1>,
    ScalarPackDef<u16, 2>,
    ScalarPackDef<u16, 3>,
    ScalarPackDef<u16, 4>,
    ScalarPackDef<u16, 5>,
    ScalarPackDef<u16, 6>,
    ScalarPackDef<u16, 7>,
    ScalarPackDef<u16, 8>,
    ScalarPackDef<u16, 9>,
    ScalarPackDef<u16, 10>,
    ScalarPackDef<u16, 11>,
    ScalarPackDef<u16, 12>,
    ScalarPackDef<u16, 13>,
    ScalarPackDef<u16, 14>,
    ScalarPackDef<u16, 15>,
    ScalarPackDef<u16, 16>};

static u16 *(*const unpack_scalar_functions_u16[17])(u16 *, const u16 *, const u32) = {
    ScalarUnPackDef<u16, 0>,
    ScalarUnPackDef<u16, 1>,
    ScalarUnPackDef<u16, 2>,
    ScalarUnPackDef<u16, 3>,
    ScalarUnPackDef<u16, 4>,
    ScalarUnPackDef<u16, 5>,
    ScalarUnPackDef<u16, 6>,
    ScalarUnPackDef<u16, 7>,
    ScalarUnPackDef<u16, 8>,
    ScalarUnPackDef<u16, 9>,
    ScalarUnPackDef<u16, 10>,
    ScalarUnPackDef<u16, 11>,
    ScalarUnPackDef<u16, 12>,
    ScalarUnPackDef<u16, 13>,
    ScalarUnPackDef<u16, 14>,
    ScalarUnPackDef<u16, 15>,
    ScalarUnPackDef<u16, 16>};

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

static u32 *(*const pack_scalar_functions_u32[33])(u32 *, const u32 *, const u32) = {
    ScalarPackDef<u32, 0>,
    ScalarPackDef<u32, 1>,
    ScalarPackDef<u32, 2>,
    ScalarPackDef<u32, 3>,
    ScalarPackDef<u32, 4>,
    ScalarPackDef<u32, 5>,
    ScalarPackDef<u32, 6>,
    ScalarPackDef<u32, 7>,
    ScalarPackDef<u32, 8>,
    ScalarPackDef<u32, 9>,
    ScalarPackDef<u32, 10>,
    ScalarPackDef<u32, 11>,
    ScalarPackDef<u32, 12>,
    ScalarPackDef<u32, 13>,
    ScalarPackDef<u32, 14>,
    ScalarPackDef<u32, 15>,
    ScalarPackDef<u32, 16>,
    ScalarPackDef<u32, 17>,
    ScalarPackDef<u32, 18>,
    ScalarPackDef<u32, 19>,
    ScalarPackDef<u32, 20>,
    ScalarPackDef<u32, 21>,
    ScalarPackDef<u32, 22>,
    ScalarPackDef<u32, 23>,
    ScalarPackDef<u32, 24>,
    ScalarPackDef<u32, 25>,
    ScalarPackDef<u32, 26>,
    ScalarPackDef<u32, 27>,
    ScalarPackDef<u32, 28>,
    ScalarPackDef<u32, 29>,
    ScalarPackDef<u32, 30>,
    ScalarPackDef<u32, 31>,
    ScalarPackDef<u32, 32>};

static u32 *(*const unpack_scalar_functions_u32[33])(u32 *, const u32 *, const u32) = {
    ScalarUnPackDef<u32, 0>,
    ScalarUnPackDef<u32, 1>,
    ScalarUnPackDef<u32, 2>,
    ScalarUnPackDef<u32, 3>,
    ScalarUnPackDef<u32, 4>,
    ScalarUnPackDef<u32, 5>,
    ScalarUnPackDef<u32, 6>,
    ScalarUnPackDef<u32, 7>,
    ScalarUnPackDef<u32, 8>,
    ScalarUnPackDef<u32, 9>,
    ScalarUnPackDef<u32, 10>,
    ScalarUnPackDef<u32, 11>,
    ScalarUnPackDef<u32, 12>,
    ScalarUnPackDef<u32, 13>,
    ScalarUnPackDef<u32, 14>,
    ScalarUnPackDef<u32, 15>,
    ScalarUnPackDef<u32, 16>,
    ScalarUnPackDef<u32, 17>,
    ScalarUnPackDef<u32, 18>,
    ScalarUnPackDef<u32, 19>,
    ScalarUnPackDef<u32, 20>,
    ScalarUnPackDef<u32, 21>,
    ScalarUnPackDef<u32, 22>,
    ScalarUnPackDef<u32, 23>,
    ScalarUnPackDef<u32, 24>,
    ScalarUnPackDef<u32, 25>,
    ScalarUnPackDef<u32, 26>,
    ScalarUnPackDef<u32, 27>,
    ScalarUnPackDef<u32, 28>,
    ScalarUnPackDef<u32, 29>,
    ScalarUnPackDef<u32, 30>,
    ScalarUnPackDef<u32, 31>,
    ScalarUnPackDef<u32, 32>};

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

static u64 *(*const pack_scalar_functions_u64[65])(u64 *, const u64 *, const u32) = {
    ScalarPackDef<u64, 0>,
    ScalarPackDef<u64, 1>,
    ScalarPackDef<u64, 2>,
    ScalarPackDef<u64, 3>,
    ScalarPackDef<u64, 4>,
    ScalarPackDef<u64, 5>,
    ScalarPackDef<u64, 6>,
    ScalarPackDef<u64, 7>,
    ScalarPackDef<u64, 8>,
    ScalarPackDef<u64, 9>,
    ScalarPackDef<u64, 10>,
    ScalarPackDef<u64, 11>,
    ScalarPackDef<u64, 12>,
    ScalarPackDef<u64, 13>,
    ScalarPackDef<u64, 14>,
    ScalarPackDef<u64, 15>,
    ScalarPackDef<u64, 16>,
    ScalarPackDef<u64, 17>,
    ScalarPackDef<u64, 18>,
    ScalarPackDef<u64, 19>,
    ScalarPackDef<u64, 20>,
    ScalarPackDef<u64, 21>,
    ScalarPackDef<u64, 22>,
    ScalarPackDef<u64, 23>,
    ScalarPackDef<u64, 24>,
    ScalarPackDef<u64, 25>,
    ScalarPackDef<u64, 26>,
    ScalarPackDef<u64, 27>,
    ScalarPackDef<u64, 28>,
    ScalarPackDef<u64, 29>,
    ScalarPackDef<u64, 30>,
    ScalarPackDef<u64, 31>,
    ScalarPackDef<u64, 32>,
    ScalarPackDef<u64, 33>,
    ScalarPackDef<u64, 34>,
    ScalarPackDef<u64, 35>,
    ScalarPackDef<u64, 36>,
    ScalarPackDef<u64, 37>,
    ScalarPackDef<u64, 38>,
    ScalarPackDef<u64, 39>,
    ScalarPackDef<u64, 40>,
    ScalarPackDef<u64, 41>,
    ScalarPackDef<u64, 42>,
    ScalarPackDef<u64, 43>,
    ScalarPackDef<u64, 44>,
    ScalarPackDef<u64, 45>,
    ScalarPackDef<u64, 46>,
    ScalarPackDef<u64, 47>,
    ScalarPackDef<u64, 48>,
    ScalarPackDef<u64, 49>,
    ScalarPackDef<u64, 50>,
    ScalarPackDef<u64, 51>,
    ScalarPackDef<u64, 52>,
    ScalarPackDef<u64, 53>,
    ScalarPackDef<u64, 54>,
    ScalarPackDef<u64, 55>,
    ScalarPackDef<u64, 56>,
    ScalarPackDef<u64, 57>,
    ScalarPackDef<u64, 58>,
    ScalarPackDef<u64, 59>,
    ScalarPackDef<u64, 60>,
    ScalarPackDef<u64, 61>,
    ScalarPackDef<u64, 62>,
    ScalarPackDef<u64, 63>,
    ScalarPackDef<u64, 64>};

static u64 *(*const unpack_scalar_functions_u64[65])(u64 *, const u64 *, const u32) = {
    ScalarUnPackDef<u64, 0>,
    ScalarUnPackDef<u64, 1>,
    ScalarUnPackDef<u64, 2>,
    ScalarUnPackDef<u64, 3>,
    ScalarUnPackDef<u64, 4>,
    ScalarUnPackDef<u64, 5>,
    ScalarUnPackDef<u64, 6>,
    ScalarUnPackDef<u64, 7>,
    ScalarUnPackDef<u64, 8>,
    ScalarUnPackDef<u64, 9>,
    ScalarUnPackDef<u64, 10>,
    ScalarUnPackDef<u64, 11>,
    ScalarUnPackDef<u64, 12>,
    ScalarUnPackDef<u64, 13>,
    ScalarUnPackDef<u64, 14>,
    ScalarUnPackDef<u64, 15>,
    ScalarUnPackDef<u64, 16>,
    ScalarUnPackDef<u64, 17>,
    ScalarUnPackDef<u64, 18>,
    ScalarUnPackDef<u64, 19>,
    ScalarUnPackDef<u64, 20>,
    ScalarUnPackDef<u64, 21>,
    ScalarUnPackDef<u64, 22>,
    ScalarUnPackDef<u64, 23>,
    ScalarUnPackDef<u64, 24>,
    ScalarUnPackDef<u64, 25>,
    ScalarUnPackDef<u64, 26>,
    ScalarUnPackDef<u64, 27>,
    ScalarUnPackDef<u64, 28>,
    ScalarUnPackDef<u64, 29>,
    ScalarUnPackDef<u64, 30>,
    ScalarUnPackDef<u64, 31>,
    ScalarUnPackDef<u64, 32>,
    ScalarUnPackDef<u64, 33>,
    ScalarUnPackDef<u64, 34>,
    ScalarUnPackDef<u64, 35>,
    ScalarUnPackDef<u64, 36>,
    ScalarUnPackDef<u64, 37>,
    ScalarUnPackDef<u64, 38>,
    ScalarUnPackDef<u64, 39>,
    ScalarUnPackDef<u64, 40>,
    ScalarUnPackDef<u64, 41>,
    ScalarUnPackDef<u64, 42>,
    ScalarUnPackDef<u64, 43>,
    ScalarUnPackDef<u64, 44>,
    ScalarUnPackDef<u64, 45>,
    ScalarUnPackDef<u64, 46>,
    ScalarUnPackDef<u64, 47>,
    ScalarUnPackDef<u64, 48>,
    ScalarUnPackDef<u64, 49>,
    ScalarUnPackDef<u64, 50>,
    ScalarUnPackDef<u64, 51>,
    ScalarUnPackDef<u64, 52>,
    ScalarUnPackDef<u64, 53>,
    ScalarUnPackDef<u64, 54>,
    ScalarUnPackDef<u64, 55>,
    ScalarUnPackDef<u64, 56>,
    ScalarUnPackDef<u64, 57>,
    ScalarUnPackDef<u64, 58>,
    ScalarUnPackDef<u64, 59>,
    ScalarUnPackDef<u64, 60>,
    ScalarUnPackDef<u64, 61>,
    ScalarUnPackDef<u64, 62>,
    ScalarUnPackDef<u64, 63>,
    ScalarUnPackDef<u64, 64>};

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
inline T *ScalarPack(T *out, const T *in, const u32 nitems, const u32 bit)

{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack single");

    auto func = GetScalarPackDef<T>(bit);
    return func(out, in, nitems);
}

template <typename T>
inline T *ScalarUnPack(T *out, const T *in, const u32 nitems, const u32 bit)
{
    constexpr int MAX_BITS = sizeof(T) * 8;

    if (bit > MAX_BITS)
        throw std::runtime_error("invalid bit size in unpack single");

    auto func = GetScalarUnPackDef<T>(bit);
    return func(out, in, nitems);
}
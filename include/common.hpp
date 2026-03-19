#pragma once

#include <cstdint>
#include <cassert>
#include <stdexcept>
#include <iostream>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

constexpr u64 STANDARD_VECTOR_SIZE = 2048;
constexpr u32 MAX_COMPRESSION_DEPTH = 3 - 1;
constexpr u32 MAX_HIST_SIZE = 65;
constexpr u32 UNCOMPRESSED_FAVOR = 80;
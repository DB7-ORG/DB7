#pragma once

#include "common.hpp"

#include "nodes/types.hpp"
#include "stats/number_stats.hpp"
#include "stats/string_stats.hpp"
#include "../compressions/compression.hpp"

#include <vector>
#include <cstring>
#include <unordered_set>
#include <array>

// static u32 EstimateCompression(SchemaType type, NumberStats<T> &stats)
// {
//     switch (type)
//     {
//     case Uncompressed:
//         return EstimateUncompressed(stats.nitems);
//     case Bitpacking:
//         return BitPackEncoder<T>::EstimateCompression(stats.max, stats.nitems);
//     case Dictionary:
//         return DictionaryValueEncoder<T>::EstimateCompression(stats.distinct_values.Size(), stats.nitems);
//     case FastPFor:
//         return FastPForEncoder::EstimateCompression<T>(stats.bitFreq, stats.total_size);
//     case Oneval:
//         return OneValEncoder<T>::EstimateCompression(stats.distinct_values.Size());
//     case Rle:
//         return RleEncoder<T>::EstimateCompression(stats.count_run_len);
//     default:
//         throw std::runtime_error("Unsupported type in CompressSample");
//     }
// }

enum struct UnknownStats
{
    NumItems,
    BitFreq,
    CountRunLen,
    CountDistinct,
    Min,
    Max
};

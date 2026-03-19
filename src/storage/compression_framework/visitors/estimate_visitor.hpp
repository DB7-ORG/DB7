#pragma once

#include "common.hpp"
#include "../nodes/types.hpp"
#include "../nodes/nodes.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include "nullbitmap.hpp"
#include "slab_arena.hpp"
#include "align_utils.hpp"
#include "../../compressions/compression.hpp"
#include "fixed_dequeue.hpp"
#include "models.hpp"

#include <cstring>
#include <span>

template <typename T>
inline u32 EstimateNext(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
{
    if constexpr (std::is_floating_point_v<T>)
        return EstimateDouble(data, arena, queue);
    else if constexpr (std::is_integral_v<T>)
        return EstimateInteger(data, arena, queue);
    else
        throw std::runtime_error("type doesnt exist");
}

template <typename T>
u32 EstimateInteger(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
{
    if (data.depth > MAX_COMPRESSION_DEPTH)
    {
        return data.nitems * sizeof(T);
    }

    data.depth++;

    u32 dict = EstimateDictionary(data, arena, queue);
    u32 rle = EstimateRle(data, arena, queue);
    u32 bp = EstimateBp(data);
    u32 raw = data.nitems * sizeof(T);

    u32 best = std::min({dict, rle, bp, raw * UNCOMPRESSED_FAVOR / 100});

    if (best == dict)
        queue->Push(SchemeAlgorithm::Dictionary);
    else if (best == rle)
        queue->Push(SchemeAlgorithm::Rle);
    else if (best == bp)
        queue->Push(SchemeAlgorithm::Bitpacking);
    else
        queue->Push(SchemeAlgorithm::Uncompressed);

    return best;
}

template <typename T>
u32 EstimateDictionary(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
{

    DictionaryValueEncodedRes<T> result{
        .codes = arena->Alloc<u32>(data.nitems),
        .values = arena->Alloc<T>(data.nitems),
        .valCount = 0};
    DictionaryValueEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

    auto codes = NumberData(result.codes, data.nitems, data.nullmap, data.depth);

    u32 val1 = EstimateNext(codes, arena, queue);

    auto values = NumberData(result.values, result.valCount, data.nullmap, data.depth);

    u32 val2 = EstimateNext(values, arena, queue);

    return val1 + val2;
}

template <typename T>
u32 EstimateRle(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
{
    RleEncodedRes<T> result{
        .values = arena->Alloc<T>(data.nitems),
        .counts = arena->Alloc<u16>(data.nitems),
        .count = 0};
    RleEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

    auto values = NumberData(result.values, result.count, data.nullmap, data.depth);

    u32 val1 = EstimateNext(values, arena, queue);

    auto counts = NumberData(result.counts, result.count, data.nullmap, data.depth);

    u32 val2 = EstimateNext(counts, arena, queue);

    return val1 + val2;
}

template <typename T>
u32 EstimateBp(NumberData<T> data)
{
    if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T, u8>)
    {
        T max = 0;
        for (u32 i = 0; i < data.nitems; i++)
            max |= data.src[i];

        return BitPackEncoder<T>::EstimateCompression(max, data.nitems);
    }
    else
        throw std::runtime_error("bitpack floating point err");
}

template <typename T>
u32 EstimateDouble(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
{
    if (data.depth > MAX_COMPRESSION_DEPTH)
    {
        return data.nitems * sizeof(T);
    }

    data.depth++;

    u32 dict = EstimateDictionary(data, arena, queue);
    u32 rle = EstimateRle(data, arena, queue);
    u32 freq = UINT32_MAX; // EstimateFrequency(data, arena, queue);
    u32 raw = data.nitems * sizeof(T);

    u32 best = std::min({dict, rle, freq, raw * UNCOMPRESSED_FAVOR / 100});

    if (best == dict)
        queue->Push(SchemeAlgorithm::Dictionary);
    else if (best == rle)
        queue->Push(SchemeAlgorithm::Rle);
    // else if (best == freq)
    //     queue->Push(SchemeAlgorithm::Frequency);
    else
        queue->Push(SchemeAlgorithm::Uncompressed);

    return best;
}

template <typename T>
u32 EstimateFrequency(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
{ // TODO this is omega slow

    AppendOnlyHMap<T> dict(data.nitems, 2);

    T topVal = data.src[0];
    u32 topCount = 0;

    for (u32 i = 1; i < data.nitems; i++)
    {
        u32 res = dict.Inc(data.src[i]);
        if (res > topCount) // TODO do this properly
        {
            topCount++;
            topVal = data.src[i];
        }
    }

    auto result = FreqEncodedRes<T>{
        .exceptions = arena->Alloc<T>(data.nitems),
        .bitmap = arena->Alloc<u8>(data.nitems),
        .topval = topVal,
        .exception_count = 0,
        .bitmap_size = 0};
    FreqEncoder::Encode(&result, data.src, data.nullmap, data.nitems, topVal);

    auto exception = NumberData(result.exceptions, result.exception_count, data.nullmap, data.depth);

    u32 val1 = EstimateNext(exception, arena, queue);

    return val1 + result.bitmap_size;
}

u32 EstimateString(StringData data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue);

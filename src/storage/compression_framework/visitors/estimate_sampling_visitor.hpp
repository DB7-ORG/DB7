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

#include <cstring>
#include <span>

struct AppliedSchemesQueue
{
    std::unique_ptr<SchemeAlgorithm[]> applied;
    SchemeAlgorithm *rawPtr;
    u32 appliedIdx;

    AppliedSchemesQueue(u32 size)
    {
        applied = std::make_unique<SchemeAlgorithm[]>(size);
        rawPtr = applied.get();
        appliedIdx = 0;
    }

    void Push(SchemeAlgorithm alg)
    {
        applied[appliedIdx++] = alg;
    }

    SchemeAlgorithm Pop()
    {
        return applied[--appliedIdx];
    }

    u32 Size()
    {
        return appliedIdx;
    }

    void Print()
    {
        for (u32 i = 0; i < appliedIdx; i++)
        {
            switch (applied[i])
            {
            case SchemeAlgorithm::Uncompressed:
                printf("[%u] Uncompressed\n", i);
                break;
            case SchemeAlgorithm::Dictionary:
                printf("[%u] Dictionary\n", i);
                break;
            case SchemeAlgorithm::Rle:
                printf("[%u] Rle\n", i);
                break;
            case SchemeAlgorithm::Bitpacking:
                printf("[%u] Bitpacking\n", i);
                break;
            case SchemeAlgorithm::FastPFor:
                printf("[%u] FastPFor\n", i);
                break;
            case SchemeAlgorithm::Frequency:
                printf("[%u] Frequency\n", i);
                break;
            case SchemeAlgorithm::Fsst:
                printf("[%u] Fsst\n", i);
                break;
            case SchemeAlgorithm::Oneval:
                printf("[%u] Oneval\n", i);
                break;
            }
        }
    }
};

template <typename T>
struct EstimateData
{
    T *src;
    u32 nitems;
    ValidityMask *nullmap;
    u8 depth;

    EstimateData(T *src, u32 nitems, ValidityMask *nullmap, u8 depth = 0)
        : src(src), nitems(nitems), nullmap(nullmap), depth(depth)
    {
    }
};

template <typename T>
inline u32 EstimateNext(EstimateData<T> data, SlabArena *arena, AppliedSchemesQueue *queue)
{
    if constexpr (std::is_floating_point_v<T>)
        return EstimateDouble(data, arena, queue);
    else if constexpr (std::is_integral_v<T>)
        return EstimateInteger(data, arena, queue);
    else
        throw std::runtime_error("type doesnt exist");
}

template <typename T>
u32 EstimateInteger(EstimateData<T> data, SlabArena *arena, AppliedSchemesQueue *queue)
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
u32 EstimateDictionary(EstimateData<T> data, SlabArena *arena, AppliedSchemesQueue *queue)
{

    DictionaryValueEncodedRes<T> result{
        .codes = arena->Alloc<u32>(data.nitems),
        .values = arena->Alloc<T>(data.nitems),
        .valCount = 0};
    DictionaryValueEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

    auto codes = EstimateData(result.codes, data.nitems, data.nullmap, data.depth);

    u32 val1 = EstimateNext(codes, arena, queue);

    auto values = EstimateData(result.values, result.valCount, data.nullmap, data.depth);

    u32 val2 = EstimateNext(values, arena, queue);

    return val1 + val2;
}

template <typename T>
u32 EstimateRle(EstimateData<T> data, SlabArena *arena, AppliedSchemesQueue *queue)
{
    RleEncodedRes<T> result{
        .values = arena->Alloc<T>(data.nitems),
        .counts = arena->Alloc<u16>(data.nitems),
        .count = 0};
    RleEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

    auto values = EstimateData(result.values, result.count, data.nullmap, data.depth);

    u32 val1 = EstimateNext(values, arena, queue);

    auto counts = EstimateData(result.counts, result.count, data.nullmap, data.depth);

    u32 val2 = EstimateNext(counts, arena, queue);

    return val1 + val2;
}

template <typename T>
u32 EstimateBp(EstimateData<T> data)
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
u32 EstimateDouble(EstimateData<T> data, SlabArena *arena, AppliedSchemesQueue *queue)
{
    if (data.depth > MAX_COMPRESSION_DEPTH)
    {
        return data.nitems * sizeof(T);
    }

    data.depth++;

    u32 dict = EstimateDictionary(data, arena, queue);
    u32 rle = EstimateRle(data, arena, queue);
    u32 freq = EstimateFrequency(data, arena, queue);
    u32 raw = data.nitems * sizeof(T);

    u32 best = std::min({dict, rle, freq, raw * UNCOMPRESSED_FAVOR / 100});

    if (best == dict)
        queue->Push(SchemeAlgorithm::Dictionary);
    else if (best == rle)
        queue->Push(SchemeAlgorithm::Rle);
    else if (best == freq)
        queue->Push(SchemeAlgorithm::Frequency);
    else
        queue->Push(SchemeAlgorithm::Uncompressed);

    return best;
}

template <typename T>
u32 EstimateFrequency(EstimateData<T> data, SlabArena *arena, AppliedSchemesQueue *queue)
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

    auto exception = EstimateData(result.exceptions, result.exception_count, data.nullmap, data.depth);

    u32 val1 = EstimateNext(exception, arena, queue);

    return val1 + result.bitmap_size;
}

struct EstimateStringData
{
    u8 **src;
    u32 *lenSrc;
    u32 totalLen;
    u32 nitems;
    ValidityMask *nullmap;
    u8 depth;

    EstimateStringData(u8 **src, u32 *lenSrc, u32 totalLen, u32 nitems, ValidityMask *nullmap, u8 depth = 0)
        : src(src), lenSrc(lenSrc), totalLen(totalLen), nitems(nitems), nullmap(nullmap), depth(depth) {}
};

u32 EstimateString(EstimateStringData data, SlabArena *arena, AppliedSchemesQueue *queue);

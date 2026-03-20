#pragma once

#include "common.hpp"
#include "../nodes/types.hpp"
#include "../nodes/tree_nodes.hpp"
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

struct EstimateVisitorState
{
    void *src;
    void *lenSrc;
    ValidityMask *nullmap;
    u32 nitems;
    u32 totalLen;

    EstimateVisitorState(void *src, u32 nitems, u32 totalLen = 0, void *lenSrc = nullptr)
        : src(src), lenSrc(lenSrc), nitems(nitems), totalLen(totalLen) {}
};

struct EstimateVisitor
{
    SlabArena *arena;
    void *src;
    void *lenSrc;
    ValidityMask *nullmap;
    u32 nitems;
    u32 totalLen;

    EstimateVisitor(SlabArena *arena, void *src, ValidityMask *nullmap, u32 nitems, u32 totalLen = 0, void *lenSrc = nullptr)
        : arena(arena), src(src), lenSrc(lenSrc), nullmap(nullmap), nitems(nitems), totalLen(totalLen) {}

    void RestoreState(EstimateVisitorState &state)
    {
        src = state.src;
        lenSrc = state.lenSrc;
        nitems = state.nitems;
        totalLen = state.totalLen;
    }

    void Modify(void *src, u32 nitems, u32 totalLen = 0, void *lenSrc = nullptr)
    {
        this->src = src;
        this->lenSrc = lenSrc;
        this->nitems = nitems;
        this->totalLen = totalLen;
    }

    template <typename T>
    u32 Visit(IntegerNode<T> &node)
    {
        std::cout << "integer\n";

        auto best = node.unc->Accept(*this);
        node.best_alg = SchemeAlgorithm::Uncompressed;

        auto try_better = [&](u32 val, SchemeAlgorithm alg) { //* UNCOMPRESSED_FAVOR / 100
            if (val < best)
            {
                best = val;
                node.best_alg = alg;
            }
        };

        if (node.dict)
            try_better(node.dict->Accept(*this), SchemeAlgorithm::Dictionary);
        if (node.rle)
            try_better(node.rle->Accept(*this), SchemeAlgorithm::Rle);
        if (node.bp)
            try_better(node.bp->Accept(*this), SchemeAlgorithm::Bitpacking);

        std::cout << "ret\n";
        return best;
    }

    template <typename T>
    u32 Visit(UncompressedNode<T> &)
    {
        std::cout << "uncom\n";

        return sizeof(T) * nitems; // TODO string
    }

    template <typename T>
    u32 Visit(DictionaryNode<T> &node)
    {
        std::cout << "dict\n";

        if constexpr (std::is_same_v<T, u8 *>)
        {
            auto state = EstimateVisitorState(src, nitems, totalLen, lenSrc);

            auto result = DictionaryStringEncodedRes{
                .codes = arena->Alloc<u32>(nitems),
                .indexes = arena->Alloc<u32>(nitems + 1),
                .stringBuf = arena->Alloc<u8>(totalLen),
                .totalStrLen = 0,
                .strCount = 0};
            DictionaryStringEncoder::Encode(&result, (u8 **)src, (u32 *)lenSrc, nullmap, nitems);

            Modify(result.codes, state.nitems, state.totalLen, state.lenSrc);

            auto val1 = node.codes_node->Accept(*this);

            u8 **strPtrs = arena->Alloc<u8 *>(result.strCount); // TODO test one block w single len
            u32 *lens = arena->Alloc<u32>(result.strCount);
            TransformStrings(result, strPtrs, lens);

            Modify(strPtrs, result.strCount, result.totalStrLen, lens);

            auto val2 = node.values_node->Accept(*this);

            RestoreState(state);

            return val1 + val2;
        }
        else
        {
            auto state = EstimateVisitorState(src, nitems);

            DictionaryValueEncodedRes<T> result{
                .codes = arena->Alloc<u32>(nitems),
                .values = arena->Alloc<T>(nitems),
                .valCount = 0};
            DictionaryValueEncoder::Encode(&result, (T *)src, nullmap, nitems);

            Modify(result.codes, state.nitems);

            auto val1 = node.codes_node->Accept(*this);

            Modify(result.values, result.valCount);

            auto val2 = node.values_node->Accept(*this);

            RestoreState(state);

            return val1 + val2;
        }
    }

    void TransformStrings(DictionaryStringEncodedRes result, u8 **strPtrs, u32 *lens)
    {
        u8 *cur = result.stringBuf;
        for (u32 i = 0; i < result.strCount - 1; i++)
        {
            lens[i] = result.indexes[i + 1] - result.indexes[i];
            strPtrs[i] = cur;
            cur += lens[i];
        }
    }

    template <typename T>
    u32 Visit(RleNode<T> &node)
    {
        std::cout << "rle\n";

        auto state = EstimateVisitorState(src, nitems);

        RleEncodedRes<T> result{
            .values = arena->Alloc<T>(nitems),
            .counts = arena->Alloc<u16>(nitems),
            .count = 0};
        RleEncoder::Encode(&result, (T *)src, nullmap, nitems);

        Modify(result.values, result.count);

        auto val1 = node.values_node->Accept(*this);

        Modify(result.counts, result.count);

        auto val2 = node.lens_node->Accept(*this);

        RestoreState(state);

        return val1 + val2;
    }

    template <typename T>
    u32 Visit(BitpackNode<T> &)
    {
        std::cout << "bp\n";

        if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T, u8>)
        {
            T max = 0;
            T *data = (T *)src;
            for (u32 i = 0; i < nitems; i++)
                max |= data[i];

            return BitPackEncoder<T>::EstimateCompression(max, nitems);
        }
        else
            throw std::runtime_error("bitpack floating point err");
    }
};

// template <typename T>
// inline u32 EstimateNext(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {
//     if constexpr (std::is_floating_point_v<T>)
//         return EstimateDouble(data, arena, queue);
//     else if constexpr (std::is_integral_v<T>)
//         return EstimateInteger(data, arena, queue);
//     else
//         throw std::runtime_error("type doesnt exist");
// }

// template <typename T>
// u32 EstimateInteger(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {
//     if (data.depth > MAX_COMPRESSION_DEPTH)
//     {
//         return data.nitems * sizeof(T);
//     }

//     data.depth++;

//     u32 dict = EstimateDictionary(data, arena, queue);
//     u32 rle = EstimateRle(data, arena, queue);
//     u32 bp = EstimateBp(data);
//     u32 raw = data.nitems * sizeof(T);

//     u32 best = std::min({dict, rle, bp, raw * UNCOMPRESSED_FAVOR / 100});

//     if (best == dict)
//         queue->Push(SchemeAlgorithm::Dictionary);
//     else if (best == rle)
//         queue->Push(SchemeAlgorithm::Rle);
//     else if (best == bp)
//         queue->Push(SchemeAlgorithm::Bitpacking);
//     else
//         queue->Push(SchemeAlgorithm::Uncompressed);

//     return best;
// }

// template <typename T>
// u32 EstimateDictionary(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {

//     DictionaryValueEncodedRes<T> result{
//         .codes = arena->Alloc<u32>(data.nitems),
//         .values = arena->Alloc<T>(data.nitems),
//         .valCount = 0};
//     DictionaryValueEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

//     auto codes = NumberData(result.codes, data.nitems, data.nullmap, data.depth);

//     u32 val1 = EstimateNext(codes, arena, queue);

//     auto values = NumberData(result.values, result.valCount, data.nullmap, data.depth);

//     u32 val2 = EstimateNext(values, arena, queue);

//     return val1 + val2;
// }

// template <typename T>
// u32 EstimateRle(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {
//     RleEncodedRes<T> result{
//         .values = arena->Alloc<T>(data.nitems),
//         .counts = arena->Alloc<u16>(data.nitems),
//         .count = 0};
//     RleEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

//     auto values = NumberData(result.values, result.count, data.nullmap, data.depth);

//     u32 val1 = EstimateNext(values, arena, queue);

//     auto counts = NumberData(result.counts, result.count, data.nullmap, data.depth);

//     u32 val2 = EstimateNext(counts, arena, queue);

//     return val1 + val2;
// }

// template <typename T>
// u32 EstimateBp(NumberData<T> data)
// {
//     if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T, u8>)
//     {
//         T max = 0;
//         for (u32 i = 0; i < data.nitems; i++)
//             max |= data.src[i];

//         return BitPackEncoder<T>::EstimateCompression(max, data.nitems);
//     }
//     else
//         throw std::runtime_error("bitpack floating point err");
// }

// template <typename T>
// u32 EstimateDouble(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// {
//     if (data.depth > MAX_COMPRESSION_DEPTH)
//     {
//         return data.nitems * sizeof(T);
//     }

//     data.depth++;

//     u32 dict = EstimateDictionary(data, arena, queue);
//     u32 rle = EstimateRle(data, arena, queue);
//     u32 freq = UINT32_MAX; // EstimateFrequency(data, arena, queue);
//     u32 raw = data.nitems * sizeof(T);

//     u32 best = std::min({dict, rle, freq, raw * UNCOMPRESSED_FAVOR / 100});

//     if (best == dict)
//         queue->Push(SchemeAlgorithm::Dictionary);
//     else if (best == rle)
//         queue->Push(SchemeAlgorithm::Rle);
//     // else if (best == freq)
//     //     queue->Push(SchemeAlgorithm::Frequency);
//     else
//         queue->Push(SchemeAlgorithm::Uncompressed);

//     return best;
// }

// template <typename T>
// u32 EstimateFrequency(NumberData<T> data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue)
// { // TODO this is omega slow

//     AppendOnlyHMap<T> dict(data.nitems, 2);

//     T topVal = data.src[0];
//     u32 topCount = 0;

//     for (u32 i = 1; i < data.nitems; i++)
//     {
//         u32 res = dict.Inc(data.src[i]);
//         if (res > topCount) // TODO do this properly
//         {
//             topCount++;
//             topVal = data.src[i];
//         }
//     }

//     auto result = FreqEncodedRes<T>{
//         .exceptions = arena->Alloc<T>(data.nitems),
//         .bitmap = arena->Alloc<u8>(data.nitems),
//         .topval = topVal,
//         .exception_count = 0,
//         .bitmap_size = 0};
//     FreqEncoder::Encode(&result, data.src, data.nullmap, data.nitems, topVal);

//     auto exception = NumberData(result.exceptions, result.exception_count, data.nullmap, data.depth);

//     u32 val1 = EstimateNext(exception, arena, queue);

//     return val1 + result.bitmap_size;
// }

// u32 EstimateString(StringData data, SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue);

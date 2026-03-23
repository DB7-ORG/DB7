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
    u32 skipFlags;
    u32 nitems;
    u32 totalLen;

    EstimateVisitorState(void *src, ValidityMask *nullmap, u32 nitems, u32 skipFlags, u32 totalLen = 0, void *lenSrc = nullptr)
        : src(src), lenSrc(lenSrc), nullmap(nullmap), skipFlags(skipFlags), nitems(nitems), totalLen(totalLen) {}

    EstimateVisitorState(const EstimateVisitorState &other) = default;
};

struct EstimateVisitor
{
    SlabArena *arena;
    EstimateVisitorState mainState;

    EstimateVisitor(SlabArena *arena, void *src, ValidityMask *nullmap, u32 skipFlags, u32 nitems, u32 totalLen = 0, void *lenSrc = nullptr)
        : arena(arena), mainState(src, nullmap, nitems, skipFlags, totalLen, lenSrc) {}

    void RestoreState(EstimateVisitorState &state)
    {
        mainState.src = state.src;
        mainState.lenSrc = state.lenSrc;
        mainState.skipFlags = state.skipFlags;
        mainState.nitems = state.nitems;
        mainState.totalLen = state.totalLen;
    }

    void Modify(void *src, u32 nitems, u32 skipFlags, u32 totalLen = 0, void *lenSrc = nullptr)
    {
        mainState.src = src;
        mainState.lenSrc = lenSrc;
        mainState.skipFlags = skipFlags;
        mainState.nitems = nitems;
        mainState.totalLen = totalLen;
    }

    bool ReadSkipFlags(SchemeAlgorithm alg)
    {
        return ((mainState.skipFlags >> alg) & 1) == 0;
    }

    void WriteSkipFlags(SchemeAlgorithm alg)
    {
        mainState.skipFlags |= (u32(1) << alg);
    }

    template <typename T>
    u32 Visit(IntegerNode<T> &node)
    {
        // std::cout << "integer\n";

        auto unc = node.unc->Accept(*this);
        auto best = unc;
        node.best_alg = SchemeAlgorithm::Uncompressed;

        auto try_better = [&](u32 val, SchemeAlgorithm alg)
        {
            if (val < best)
            {
                best = val;
                node.best_alg = alg;
            }
        };

        if (node.dict && ReadSkipFlags(SchemeAlgorithm::Dictionary))
            try_better(node.dict->Accept(*this), SchemeAlgorithm::Dictionary);
        if (node.rle && ReadSkipFlags(SchemeAlgorithm::Rle))
            try_better(node.rle->Accept(*this), SchemeAlgorithm::Rle);
        if (node.bp && ReadSkipFlags(SchemeAlgorithm::Bitpacking))
            try_better(node.bp->Accept(*this), SchemeAlgorithm::Bitpacking);

        if (unc * UNCOMPRESSED_FAVOR / 100 <= best)
            node.best_alg = SchemeAlgorithm::Uncompressed;

        // std::cout << "ret\n";

        return best;
    }

    template <typename T>
    u32 Visit(DoubleNode<T> &node)
    {
        // std::cout << "double\n";

        auto unc = node.unc->Accept(*this);
        auto best = unc;
        node.best_alg = SchemeAlgorithm::Uncompressed;

        auto try_better = [&](u32 val, SchemeAlgorithm alg)
        {
            if (val < best)
            {
                best = val;
                node.best_alg = alg;
            }
        };

        if (node.dict && ReadSkipFlags(SchemeAlgorithm::Dictionary))
            try_better(node.dict->Accept(*this), SchemeAlgorithm::Dictionary);
        if (node.rle && ReadSkipFlags(SchemeAlgorithm::Rle))
            try_better(node.rle->Accept(*this), SchemeAlgorithm::Rle);
        if (node.freq && ReadSkipFlags(SchemeAlgorithm::Frequency))
            try_better(node.freq->Accept(*this), SchemeAlgorithm::Frequency);

        if (unc * UNCOMPRESSED_FAVOR / 100 <= best)
            node.best_alg = SchemeAlgorithm::Uncompressed;

        // std::cout << "ret\n";

        return best;
    }

    template <typename T>
    u32 Visit(StringNode<T> &node)
    {
        // std::cout << "string\n";

        auto unc = node.unc->Accept(*this);
        auto best = unc;
        node.best_alg = SchemeAlgorithm::Uncompressed;

        auto try_better = [&](u32 val, SchemeAlgorithm alg)
        {
            if (val < best)
            {
                best = val;
                node.best_alg = alg;
            }
        };

        if (node.dict && ReadSkipFlags(SchemeAlgorithm::Dictionary))
            try_better(node.dict->Accept(*this), SchemeAlgorithm::Dictionary);
        if (node.fsst && ReadSkipFlags(SchemeAlgorithm::Fsst))
            try_better(node.fsst->Accept(*this), SchemeAlgorithm::Fsst);

        if (unc * UNCOMPRESSED_FAVOR / 100 <= best)
            node.best_alg = SchemeAlgorithm::Uncompressed;

        // std::cout << "ret\n";

        return best;
    }

    template <typename T>
    u32 Visit(UncompressedNode<T> &)
    {
        // std::cout << "uncom\n";

        if constexpr (std::is_same_v<T, u8 *>)
            return mainState.totalLen;
        else
            return sizeof(T) * mainState.nitems;
    }

    template <typename T>
    u32 Visit(DictionaryNode<T> &node)
    {
        // std::cout << "dict\n";

        if constexpr (std::is_same_v<T, u8 *>)
        {
            auto state = EstimateVisitorState(mainState);

            auto result = DictionaryStringEncodedRes{
                .codes = arena->Alloc<u32>(state.nitems),
                .indexes = arena->Alloc<u32>(state.nitems + 1),
                .stringBuf = arena->Alloc<u8>(state.totalLen),
                .totalStrLen = 0,
                .strCount = 0};
            DictionaryStringEncoder::Encode(&result, (u8 **)state.src, (u32 *)state.lenSrc, state.nullmap, state.nitems);

            WriteSkipFlags(SchemeAlgorithm::Dictionary);

            Modify(result.codes, state.nitems, mainState.skipFlags, state.totalLen, state.lenSrc);

            auto val1 = node.codes_node->Accept(*this);

            u8 **strPtrs = arena->Alloc<u8 *>(result.strCount); // TODO test one block w single len
            u32 *lens = arena->Alloc<u32>(result.strCount);
            TransformStrings(result, strPtrs, lens);

            Modify(strPtrs, result.strCount, mainState.skipFlags, result.totalStrLen, lens);

            auto val2 = node.values_node->Accept(*this);

            RestoreState(state);

            return val1 + val2;
        }
        else
        {
            auto state = EstimateVisitorState(mainState);

            DictionaryValueEncodedRes<T> result{
                .codes = arena->Alloc<u32>(state.nitems),
                .values = arena->Alloc<T>(state.nitems),
                .valCount = 0};
            DictionaryValueEncoder::Encode(&result, (T *)state.src, state.nullmap, state.nitems);

            WriteSkipFlags(SchemeAlgorithm::Dictionary);

            Modify(result.codes, state.nitems, mainState.skipFlags);

            auto val1 = node.codes_node->Accept(*this);

            Modify(result.values, result.valCount, mainState.skipFlags);

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
        // std::cout << "rle\n";

        auto state = EstimateVisitorState(mainState);

        RleEncodedRes<T> result{
            .values = arena->Alloc<T>(state.nitems),
            .counts = arena->Alloc<u16>(state.nitems),
            .count = 0};
        RleEncoder::Encode(&result, (T *)state.src, state.nullmap, state.nitems);

        WriteSkipFlags(SchemeAlgorithm::Rle);

        Modify(result.values, result.count, mainState.skipFlags);

        auto val1 = node.values_node->Accept(*this);

        Modify(result.counts, result.count, state.skipFlags);

        auto val2 = node.lens_node->Accept(*this);

        RestoreState(state);

        return val1 + val2;
    }

    template <typename T>
    u32 Visit(BitpackNode<T> &)
    {
        // std::cout << "bp\n";

        if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T, u8>)
        {
            T max = 0;
            T *data = (T *)mainState.src;
            for (u32 i = 0; i < mainState.nitems; i++)
                max |= data[i];

            return BitPackEncoder<T>::EstimateCompression(max, mainState.nitems);
        }
        else
            throw std::runtime_error("bitpack floating point err");
    }

    template <typename T>
    u32 Visit(FsstNode<T> &)
    {
        // std::cout << "fsst\n";

        size_t *lens64 = arena->Alloc<size_t>(mainState.nitems); // TODO this is a hack
        auto lens = (u32 *)mainState.lenSrc;
        for (u32 i = 0; i < mainState.nitems; i++)
            lens64[i] = lens[i];

        auto newsrc = (const u8 **)mainState.src;

        auto encoder = fsst_create(mainState.nitems, lens64, newsrc, 0);

        u32 outSize = 7 + 4 * mainState.totalLen;
        auto strBuffer = arena->Alloc<u8>(outSize);
        auto strLens = arena->Alloc<size_t>(outSize);
        auto strings = arena->Alloc<u8 *>(outSize);

        u32 nstrings = fsst_compress(encoder, mainState.nitems, lens64, newsrc, outSize, strBuffer, strLens, strings);
        if (nstrings != mainState.nitems)
        {
            throw std::runtime_error("fsst failed");
        }

        u32 totalSize = 0;
        for (u32 i = 0; i < nstrings; i++)
            totalSize += strLens[i];

        return totalSize;
    }

    template <typename T>
    u32 Visit(FrequencyNode<T> &)
    {
        return UINT32_MAX;
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

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
#include "../nodes/types.hpp"
#include "fixed_dequeue.hpp"
#include "models.hpp"

#include <cstring>

struct CompressVisitorState
{
    void *src;
    void *lenSrc;
    ValidityMask *nullmap;
    u32 nitems;
    u32 totalLen;

    CompressVisitorState(void *src, ValidityMask *nullmap, u32 nitems, u32 totalLen = 0, void *lenSrc = nullptr)
        : src(src), lenSrc(lenSrc), nullmap(nullmap), nitems(nitems), totalLen(totalLen) {}

    CompressVisitorState(const CompressVisitorState &other) = default;
};

struct CompressVisitor
{
    SlabArena *arena;
    CompressVisitorResult res;
    CompressVisitorState mainState;

    CompressVisitor(SlabArena *arena, CompressVisitorResult res, CompressVisitorState mainState)
        : arena(arena), res(res), mainState(mainState) {}

    u32 Write(u32 size)
    {
        memcpy(res.out, mainState.src, size);
        res.out += size;
        *(res.offsets++) = size;
        return size;
    }

    u32 Write(void *newOut)
    {
        auto temp = reinterpret_cast<u8 *>(newOut);
        u32 size = (temp - res.out);
        *(res.offsets++) = size;
        res.out = (u8 *)temp;
        return size;
    }

    u32 Write(u8 **strings, u32 *indexes, u32 nstrings, u32 totalLen)
    {
        u8 *prev = res.out;
        memcpy(res.out, &nstrings, sizeof(u32));
        res.out += sizeof(u32);
        memcpy(res.out, indexes, (nstrings + 1) * sizeof(u32));
        res.out += (nstrings + 1) * sizeof(u32);
        memcpy(res.out, strings, totalLen);
        res.out += totalLen;
        u32 size = res.out - prev;
        *(res.offsets++) = size;
        return size;
    }

    void PushScheme(SchemeAlgorithm alg)
    {
        *(res.schemes++) = alg;
    }

    void RestoreState(CompressVisitorState &state)
    {
        mainState.src = state.src;
        mainState.lenSrc = state.lenSrc;
        mainState.nitems = state.nitems;
        mainState.totalLen = state.totalLen;
    }

    void Modify(void *src, u32 nitems, u32 totalLen = 0, void *lenSrc = nullptr)
    {
        mainState.src = src;
        mainState.lenSrc = lenSrc;
        mainState.nitems = nitems;
        mainState.totalLen = totalLen;
    }

    template <typename T>
    u32 Visit(IntegerNode<T> &node)
    {
        switch (node.best_alg)
        {
        case SchemeAlgorithm::Uncompressed:
            return node.unc->Accept(*this);
        case SchemeAlgorithm::Dictionary:
            return node.dict->Accept(*this);
        case SchemeAlgorithm::Rle:
            return node.rle->Accept(*this);
        case SchemeAlgorithm::Bitpacking:
            return node.bp->Accept(*this);
        default:
            throw std::runtime_error("unsupported compression scheme");
        }
    }

    template <typename T>
    u32 Visit(DoubleNode<T> &node)
    {
        switch (node.best_alg)
        {
        case SchemeAlgorithm::Uncompressed:
            return node.unc->Accept(*this);
        case SchemeAlgorithm::Dictionary:
            return node.dict->Accept(*this);
        case SchemeAlgorithm::Rle:
            return node.rle->Accept(*this);
        case SchemeAlgorithm::Frequency:
            return node.freq->Accept(*this);
        default:
            throw std::runtime_error("unsupported compression scheme");
        }
    }

    template <typename T>
    u32 Visit(StringNode<T> &node)
    {
        switch (node.best_alg)
        {
        case SchemeAlgorithm::Uncompressed:
            return node.unc->Accept(*this);
        case SchemeAlgorithm::Dictionary:
            return node.dict->Accept(*this);
        case SchemeAlgorithm::Fsst:
            return node.fsst->Accept(*this);
        default:
            throw std::runtime_error("unsupported compression scheme");
        }
    }

    template <typename T>
    u32 Visit(UncompressedNode<T> &)
    {
        // std::cout << "uncom\n";

        PushScheme(SchemeAlgorithm::Uncompressed);

        if constexpr (std::is_same_v<T, u8 *>)
        {
            u32 align = GetAlignment<T>(res.out);
            res.out += align;
            return Write((u8 **)mainState.src, (u32 *)mainState.lenSrc, mainState.nitems, mainState.totalLen);
        }
        else
        {
            u32 size = sizeof(T) * mainState.nitems;
            u32 align = GetAlignment<T>(res.out);
            res.out += align;
            return Write(size) + align;
        }
    }

    template <typename T>
    u32 Visit(DictionaryNode<T> &node)
    {
        // std::cout << "dict\n";

        PushScheme(SchemeAlgorithm::Dictionary);

        if constexpr (std::is_same_v<T, u8 *>)
        {
            auto state = CompressVisitorState(mainState);

            auto result = DictionaryStringEncodedRes{
                .codes = arena->Alloc<u32>(state.nitems),
                .indexes = arena->Alloc<u32>(state.nitems + 1),
                .stringBuf = arena->Alloc<u8>(state.totalLen),
                .totalStrLen = 0,
                .strCount = 0};
            DictionaryStringEncoder::Encode(&result, (u8 **)state.src, (u32 *)state.lenSrc, state.nullmap, state.nitems);

            Modify(result.codes, state.nitems, state.totalLen, state.lenSrc);

            auto val1 = node.codes_node->Accept(*this);

            u8 **strPtrs = arena->Alloc<u8 *>(result.strCount); // TODO test one block w single len
            u32 *lens = arena->Alloc<u32>(result.strCount);
            TransformStrings(result, strPtrs, lens);

            Modify(strPtrs, result.strCount, result.totalStrLen, lens); // TODO this must be changed because ill have to dop inverse transform also later which is stupid

            auto val2 = node.values_node->Accept(*this);

            RestoreState(state);

            return val1 + val2;
        }
        else
        {
            auto state = CompressVisitorState(mainState);

            DictionaryValueEncodedRes<T> result{
                .codes = arena->Alloc<u32>(state.nitems),
                .values = arena->Alloc<T>(state.nitems),
                .valCount = 0};
            DictionaryValueEncoder::Encode(&result, (T *)state.src, state.nullmap, state.nitems);

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
        // std::cout << "rle\n";

        PushScheme(SchemeAlgorithm::Rle);

        auto state = CompressVisitorState(mainState);

        RleEncodedRes<T> result{
            .values = arena->Alloc<T>(state.nitems),
            .counts = arena->Alloc<u16>(state.nitems),
            .count = 0};
        RleEncoder::Encode(&result, (T *)state.src, state.nullmap, state.nitems);

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
        // std::cout << "bp\n";

        PushScheme(SchemeAlgorithm::Bitpacking);

        if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T, u8>)
        {
            T max = 0;
            T *data = (T *)mainState.src;
            for (u32 i = 0; i < mainState.nitems; i++)
                max |= data[i];

            u32 usedBits = CountBitsUsed(max);

            u32 align = GetAlignment<T>(res.out);
            res.out += align;

            memcpy(res.out, &usedBits, sizeof(T));
            res.out += sizeof(T);

            T *newOut = BitPackCombinedEncoder<T>::Encode(res.out, (T *)mainState.src, mainState.nitems, usedBits);

            return Write(newOut) + align + sizeof(T);
        }
        else
            throw std::runtime_error("bitpack floating point err");
    }

    template <typename T>
    u32 Visit(FsstNode<T> &)
    {
        // std::cout << "fsst\n";

        PushScheme(SchemeAlgorithm::Fsst);

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
        auto strLens2 = arena->Alloc<u32>(nstrings);
        for (u32 i = 0; i < nstrings; i++)
        {
            strLens2[i] = strLens[i];
            totalSize += strLens[i];
        }

        return Write(strings, strLens2, nstrings, totalSize);
    }

    template <typename T>
    u32 Visit(FrequencyNode<T> &)
    {
        PushScheme(SchemeAlgorithm::Frequency);

        return UINT32_MAX;
    }
};
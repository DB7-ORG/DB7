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

// struct CompressVisitor
// {
//     SlabArena *arena;
//     FixedDeque<SchemeAlgorithm> *queue;
//     u8 *out;

//     CompressVisitor(SlabArena *arena, FixedDeque<SchemeAlgorithm> *queue, u8 *out)
//         : arena(arena), queue(queue), out(out) {}

//     inline void TransformStrings(DictionaryStringEncodedRes result, u8 **strPtrs, u32 *lens)
//     {
//         u8 *cur = result.stringBuf;
//         for (u32 i = 0; i < result.strCount - 1; i++)
//         {
//             lens[i] = result.indexes[i + 1] - result.indexes[i];
//             strPtrs[i] = cur;
//             cur += lens[i];
//         }
//     }

//     u32 CompressFsst(StringData data)
//     {
//         size_t *lens64 = arena->Alloc<size_t>(data.nitems); // TODO this is a hack
//         for (u32 i = 0; i < data.nitems; i++)
//             lens64[i] = data.lenSrc[i];

//         auto encoder = fsst_create(data.nitems, lens64, (const u8 **)data.src, 0);

//         u32 outSize = 7 + 4 * data.totalLen;
//         auto strBuffer = arena->Alloc<u8>(outSize);
//         auto strLens = arena->Alloc<size_t>(outSize);
//         auto strings = arena->Alloc<u8 *>(outSize);
//         auto src = (const u8 **)data.src;

//         u32 nstrings = fsst_compress(encoder, data.nitems, lens64, src, outSize, strBuffer, strLens, strings);
//         if (nstrings != data.nitems)
//         {
//             throw std::runtime_error("fsst failed");
//         }

//         u32 totalSize = 0;
//         for (u32 i = 0; i < nstrings; i++)
//             totalSize += strLens[i];

//         return totalSize;
//     }

//     u32 CompressStringDictionary(StringData data)
//     {
//         auto result = DictionaryStringEncodedRes{
//             .codes = arena->Alloc<u32>(data.nitems),
//             .indexes = arena->Alloc<u32>(data.nitems + 1),
//             .stringBuf = arena->Alloc<u8>(data.totalLen),
//             .totalStrLen = 0,
//             .strCount = 0};
//         DictionaryStringEncoder::Encode(&result, data.src, data.lenSrc, data.nullmap, data.nitems);

//         auto codes = NumberData(result.codes, data.nitems, data.nullmap, data.depth);

//         u32 val1 = CompressNext(codes);

//         u8 **strPtrs = arena->Alloc<u8 *>(result.strCount); // TODO test one block w single len
//         u32 *lens = arena->Alloc<u32>(result.strCount);
//         TransformStrings(result, strPtrs, lens);
//         auto strings = StringData(strPtrs, lens, result.totalStrLen, result.strCount, data.nullmap, data.depth);

//         u32 val2 = CompressNext(strings);

//         return val1 + val2;
//     }

//     template <typename T>
//     u32 CompressDictionary(NumberData<T> data)
//     {

//         DictionaryValueEncodedRes<T> result{
//             .codes = arena->Alloc<u32>(data.nitems),
//             .values = arena->Alloc<T>(data.nitems),
//             .valCount = 0};
//         DictionaryValueEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

//         auto codes = NumberData(result.codes, data.nitems, data.nullmap, data.depth);

//         u32 val1 = CompressNext(codes);

//         auto values = NumberData(result.values, result.valCount, data.nullmap, data.depth);

//         u32 val2 = CompressNext(values);

//         return val1 + val2;
//     }

//     template <typename T>
//     u32 CompressRle(NumberData<T> data)
//     {
//         RleEncodedRes<T> result{
//             .values = arena->Alloc<T>(data.nitems),
//             .counts = arena->Alloc<u16>(data.nitems),
//             .count = 0};
//         RleEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

//         auto values = NumberData(result.values, result.count, data.nullmap, data.depth);

//         u32 val1 = CompressNext(values);

//         auto counts = NumberData(result.counts, result.count, data.nullmap, data.depth);

//         u32 val2 = CompressNext(counts);

//         return val1 + val2;
//     }

//     template <typename T>
//     u32 CompressBp(NumberData<T> data)
//     {
//         if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T, u8>)
//         {
//             T max = 0;
//             for (u32 i = 0; i < data.nitems; i++)
//                 max |= data.src[i];

//             u32 usedBits = CountBitsUsed(max);

//             out += GetAlignment<T>(out);

//             T *newOut = BitPackCombinedEncoder<T>::Encode(out, data.src, data.nitems, usedBits);

//             auto temp = reinterpret_cast<u8 *>(newOut);

//             u32 size = (temp - out);

//             out = (u8 *)temp;

//             return size;
//         }
//         else
//             throw std::runtime_error("bitpack floating point err");
//     }

//     template <typename T>
//     u32 CompressFrequncy(NumberData<T> data)
//     { // TODO this is omega slow

//         AppendOnlyHMap<T> dict(data.nitems, 2);

//         T topVal = data.src[0];
//         u32 topCount = 0;

//         for (u32 i = 1; i < data.nitems; i++)
//         {
//             u32 res = dict.Inc(data.src[i]);
//             if (res > topCount) // TODO do this properly
//             {
//                 topCount++;
//                 topVal = data.src[i];
//             }
//         }

//         auto result = FreqEncodedRes<T>{
//             .exceptions = arena->Alloc<T>(data.nitems),
//             .bitmap = arena->Alloc<u8>(data.nitems),
//             .topval = topVal,
//             .exception_count = 0,
//             .bitmap_size = 0};
//         FreqEncoder::Encode(&result, data.src, data.nullmap, data.nitems, topVal);

//         auto exception = NumberData(result.exceptions, result.exception_count, data.nullmap, data.depth);

//         u32 val1 = CompressNext(exception);

//         return val1 + result.bitmap_size;
//     }

//     template <typename T>
//     u32 Uncompressed(NumberData<T> data)
//     {
//         out += GetAlignment<T>(out);
//         u32 size = data.nitems * sizeof(T);
//         memcpy(out, data.src, size);
//         out += size;
//         return size;
//     }

//     u32 Uncompressed(StringData data)
//     {
//         out += GetAlignment<u32>(out);
//         u32 size = data.nitems;
//         memcpy(out, data.src, size);
//         out += size;
//         return size;
//     }

//     u32 CompressNext(StringData data)
//     {
//         if (data.depth > MAX_COMPRESSION_DEPTH)
//         {
//             return Uncompressed(data);
//         }

//         data.depth++;

//         SchemeAlgorithm scheme = queue->Pop();
//         switch (scheme)
//         {
//         case SchemeAlgorithm::Uncompressed:
//             return data.totalLen;
//         case SchemeAlgorithm::Dictionary:
//             return CompressStringDictionary(data);
//         case SchemeAlgorithm::Fsst:
//             return CompressFsst(data);
//         default:
//             throw std::runtime_error("failed compression");
//         }
//     }

//     template <typename T>
//     u32 CompressNext(NumberData<T> data)
//     {
//         if constexpr (std::is_floating_point_v<T>)
//             return CompressDouble(data);
//         else if constexpr (std::is_integral_v<T>)
//             return CompressInteger(data);
//         else
//             throw std::runtime_error("type doesnt exist");
//     }

//     template <typename T>
//     u32 CompressInteger(NumberData<T> data)
//     {
//         if (data.depth > MAX_COMPRESSION_DEPTH)
//         {
//             return Uncompressed(data);
//         }

//         data.depth++;

//         SchemeAlgorithm scheme = queue->Pop();
//         switch (scheme)
//         {
//         case SchemeAlgorithm::Uncompressed:
//             return Uncompressed(data);
//         case SchemeAlgorithm::Dictionary:
//             return CompressDictionary(data);
//         case SchemeAlgorithm::Rle:
//             return CompressRle(data);
//         case SchemeAlgorithm::Bitpacking:
//             return CompressBp(data);
//         default:
//             throw std::runtime_error("failed compression");
//         }
//     }

//     template <typename T>
//     u32 CompressDouble(NumberData<T> data)
//     {
//         if (data.depth > MAX_COMPRESSION_DEPTH)
//         {
//             return Uncompressed(data);
//         }

//         data.depth++;

//         SchemeAlgorithm scheme = queue->Pop();
//         switch (scheme)
//         {
//         case SchemeAlgorithm::Uncompressed:
//             return Uncompressed(data);
//         case SchemeAlgorithm::Dictionary:
//             return CompressDictionary(data);
//         case SchemeAlgorithm::Rle:
//             return CompressRle(data);
//         default:
//             throw std::runtime_error("failed compression");
//         }
//     }
// };

// } struct CompressVisitorState
// {
//     const void *src;
//     u32 nitems;
//     SrcType src_type;
// };

// struct CompressVisitor : IVisitor
// {
//     const IStats *stats;
//     const void *src;
//     SrcType src_type;
//     ValidityMask *nullmap;
//     u32 nitems;

//     u8 *init_data;
//     u8 *data;

//     u8 *init_header;
//     u8 *header;

//     u32 *init_offsets;
//     u32 *offsets;

//     SlabArena *arena;

//     CompressVisitor(IStats * stats, SrcType src_type, void *src, ValidityMask *nullmap, u32 nitems, u8 *out, SlabArena *arena)
//         : stats(stats),
//           src(src),
//           src_type(src_type),
//           nullmap(nullmap),
//           nitems(nitems),
//           init_data(out),
//           data(out),
//           arena(arena)
//     { // TODO make values scale based on depth
//         init_header = arena->Alloc<u8>(64);
//         header = init_header;
//         init_offsets = arena->Alloc<u32>(32);
//         offsets = init_offsets;
//     }

//     inline void Write(const void *buf, u32 size)
//     { // TODO align data
//         memcpy(data, buf, size);
//         data += size;
//     }

//     inline void PushOffset(u32 off)
//     {
//         *(offsets++) = off;
//     }

//     inline void PushHeader(u8 val)
//     {
//         *(header++) = val;
//     }

//     inline CompressVisitorState SaveState()
//     {
//         return {src, nitems, src_type};
//     }

//     inline void RestoreState(const CompressVisitorState &state)
//     {
//         src = state.src;
//         src_type = state.src_type;
//         nitems = state.nitems;
//     }

//     inline void PrepState(const void *new_src, SrcType new_type, u32 new_nitems)
//     {
//         src = new_src;
//         src_type = new_type;
//         nitems = new_nitems;
//     }

//     u32 Visit(IntegerNode & node) override
//     {
//         // std::cout << "int visited" << std::endl;

//         INode *cur = node.children[node.best_node_idx];

//         PushHeader(node.best_node_idx);

//         return cur->Accept(*this);
//     }

//     u32 Visit(DoubleNode & node) override
//     {
//         // std::cout << "dbl visited" << std::endl;

//         INode *cur = node.children[node.best_node_idx];

//         PushHeader(node.best_node_idx);

//         return cur->Accept(*this);
//     }

//     u32 Visit(StringNode & node) override
//     {
//         // std::cout << "dbl visited" << std::endl;

//         INode *cur = node.children[node.best_node_idx];

//         PushHeader(node.best_node_idx);

//         return cur->Accept(*this);
//     }

//     u32 Visit(UncompressedNode &) override
//     {
//         // std::cout << "uncom visited" << std::endl;

//         u32 size = SizeOfBuffer(src_type, nitems);

//         DispatchType(src_type, [&]<typename T>() { //
//             data += GetAlignment<T>(data);
//             Write(src, size);
//         });

//         PushOffset(data - init_data);

//         return size;
//     }

//     template <typename T>
//     void DictEncodeTemplated(u32 * codes, T * values, u32 & valCount)
//     {
//         DictionaryValueEncodedRes<T> result{
//             .codes = codes,
//             .values = values,
//             .valCount = 0};
//         auto castSrc = reinterpret_cast<const T *>(src);
//         DictionaryValueEncoder::Encode(&result, castSrc, nullmap, nitems);
//         valCount = result.valCount;
//     }

//     u32 Visit(DictionaryNode & node) override
//     {
//         // std::cout << "dict visited" << std::endl;

//         if (src_type == SrcType::STR)
//         {
//             // TODO add str compress
//             return 0;
//         }
//         else
//         {
//             u32 valCount;
//             void *codes, *values;
//             DispatchType(src_type, [&]<typename T>() { //
//                 codes = arena->Alloc<u32>(nitems);
//                 values = arena->Alloc<T>(nitems);
//                 DictEncodeTemplated((u32 *)codes, (T *)values, valCount);
//             });

//             PushOffset(valCount);

//             // Collect stats again

//             // Compare w estimated stats

//             auto state = SaveState();

//             PrepState(codes, SrcType::U32, state.nitems);

//             u32 val1 = node.codes_node->Accept(*this);

//             PrepState(values, state.src_type, valCount);

//             u32 val2 = node.values_node->Accept(*this);

//             RestoreState(state); // This is just a guard if i add something after left/right node search

//             return val1 + val2;
//         }
//     }

//     template <typename T>
//     inline void RleEncodeTemplated(u16 * counts, T * values, u32 & count)
//     {
//         RleEncodedRes<T> result{
//             .values = values,
//             .counts = counts,
//             .count = 0};
//         auto castSrc = reinterpret_cast<const T *>(src);
//         RleEncoder::Encode(&result, castSrc, nullmap, nitems);
//         count = result.count;
//     }

//     u32 Visit(RleNode & node) override
//     {
//         // std::cout << "rle visited" << std::endl;

//         void *counts, *values;
//         u32 count;
//         DispatchType(src_type, [&]<typename T>() { //
//             counts = arena->Alloc<u16>(nitems);
//             values = arena->Alloc<T>(nitems);
//             RleEncodeTemplated((u16 *)counts, (T *)values, count);
//         });

//         PushOffset(count);

//         // Collect stats again

//         // Compare w estimated stats

//         auto state = SaveState();

//         PrepState(values, state.src_type, count);

//         u32 val1 = node.values_node->Accept(*this);

//         PrepState(counts, SrcType::U16, count);

//         u32 val2 = node.lens_node->Accept(*this);

//         RestoreState(state); // This is just a guard if i add something after left/right node search

//         return val1 + val2;
//     }

//     template <typename T>
//     inline void BitpackEncodeTemplated(T * out, T * in, u32 nitems, u32 & size, u32 & usedBits)
//     {
//         static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");
//         static_assert(!std::is_same_v<T, u8>, "Bitpacking not supported for u8");

//         T max = 0;
//         for (u32 i = 0; i < nitems; i++)
//             max |= in[i];

//         usedBits = CountBitsUsed(max);

//         T *newOut = BitPackCombinedEncoder<T>::Encode(out, in, nitems, usedBits);

//         size = (newOut - out) * sizeof(T);
//     }

//     u32 Visit(BitpackNode &) override
//     {
//         // std::cout << "bitpack visited" << std::endl;

//         u32 size = 0;
//         u32 usedBits = 0;
//         DispatchType(src_type, [&]<typename T>()
//                      { if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T,u8>){
//                     data += GetAlignment<T>(data);
//                     BitpackEncodeTemplated((T *)data, (T *)src, nitems, size, usedBits);
//                  } else
//                     throw std::runtime_error("bitpack floating point err"); });

//         data += size;
//         PushOffset(data - init_data);
//         PushOffset(usedBits);

//         return size;
//     }

//     template <typename T>
//     inline void FastPForEncodeTemplated(u32 * out, T * in, u32 nitems, u32 & size)
//     {
//         static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");
//         static_assert(!std::is_same_v<T, u8>, "Bitpacking not supported for u8");

//         FastPForEncoder encoder; // TODO make this stateless (static encode and decode)
//         size = encoder.Encode<T>(out, in, nitems) * sizeof(u32);
//     }

//     u32 Visit(FastPForNode &) override
//     {
//         // std::cout << "bitpack visited" << std::endl;

//         u32 size = 0;

//         DispatchType(src_type, [&]<typename T>()
//                      { if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T,u8>){
//                     data += GetAlignment<T>(data);
//                     FastPForEncodeTemplated((u32 *)data, (T *)src, nitems, size);
//                  } else
//                     throw std::runtime_error("bitpack floating point err"); });

//         data += size;
//         PushOffset(data - init_data);

//         return size;
//     }

//     u32 Visit(ForNode & node) override
//     {
//         // std::cout << "for visited" << std::endl;

//         void *values;
//         DispatchType(src_type, [&]<typename T>() { //
//             values = arena->Alloc<T>(nitems);

//             T *in = (T *)src;

//             T min = in[0];
//             for (u32 i = 1; i < nitems; i++)
//                 min = std::min(min, in[i]);

//             ForEncoder::Encode((T *)values, (__m256i *)src, nitems, min);

//             PushOffset(min);
//         });

//         // Collect stats again

//         // Compare w estimated stats

//         auto state = SaveState();

//         PrepState(values, state.src_type, state.nitems);

//         u32 val = node.values_node->Accept(*this);

//         RestoreState(state); // This is just a guard if i add something after left/right node search

//         return val;
//     }

//     inline void TransformStrings(DictionaryStringEncodedRes result, u8 * *strPtrs, u32 * lens)
//     {
//         u8 *cur = result.stringBuf;
//         for (u32 i = 0; i < result.strCount - 1; i++)
//         {
//             lens[i] = result.indexes[i + 1] - result.indexes[i];
//             strPtrs[i] = cur;
//             cur += lens[i];
//         }
//     }

//     u32 EstimateStringDictionary(StringData data, SlabArena * arena, FixedDeque<SchemeAlgorithm> * queue)
//     {
//         auto result = DictionaryStringEncodedRes{
//             .codes = arena->Alloc<u32>(data.nitems),
//             .indexes = arena->Alloc<u32>(data.nitems + 1),
//             .stringBuf = arena->Alloc<u8>(data.totalLen),
//             .totalStrLen = 0,
//             .strCount = 0};
//         DictionaryStringEncoder::Encode(&result, data.src, data.lenSrc, data.nullmap, data.nitems);

//         auto codes = NumberData(result.codes, data.nitems, data.nullmap, data.depth);

//         u32 val1 = EstimateNext(codes);

//         u8 **strPtrs = arena->Alloc<u8 *>(result.strCount); // TODO test one block w single len
//         u32 *lens = arena->Alloc<u32>(result.strCount);
//         TransformStrings(result, strPtrs, lens);
//         auto strings = StringData(strPtrs, lens, result.totalStrLen, result.strCount, data.nullmap, data.depth);

//         u32 val2 = EstimateString(strings);

//         return val1 + val2;
//     }

//     u32 EstimateFsst(StringData data, SlabArena * arena)
//     {
//         size_t *lens64 = arena->Alloc<size_t>(data.nitems); // TODO this is a hack
//         for (u32 i = 0; i < data.nitems; i++)
//             lens64[i] = data.lenSrc[i];

//         auto encoder = fsst_create(data.nitems, lens64, (const u8 **)data.src, 0);

//         u32 outSize = 7 + 4 * data.totalLen;
//         auto strBuffer = arena->Alloc<u8>(outSize);
//         auto strLens = arena->Alloc<size_t>(outSize);
//         auto strings = arena->Alloc<u8 *>(outSize);
//         auto src = (const u8 **)data.src;

//         u32 nstrings = fsst_compress(encoder, data.nitems, lens64, src, outSize, strBuffer, strLens, strings);
//         if (nstrings != data.nitems)
//         {
//             throw std::runtime_error("fsst failed");
//         }

//         u32 totalSize = 0;
//         for (u32 i = 0; i < nstrings; i++)
//             totalSize += strLens[i];

//         return totalSize;
//     }

//     u32 EstimateString(StringData data, SlabArena * arena, FixedDeque<SchemeAlgorithm> * queue)
//     {
//         if (data.depth > MAX_COMPRESSION_DEPTH)
//         {
//             return data.totalLen;
//         }

//         data.depth++;

//         u32 fsst = EstimateFsst(data, arena);
//         u32 dict = EstimateStringDictionary(data);
//         u32 raw = data.totalLen;

//         u32 best = std::min({fsst, dict, raw * UNCOMPRESSED_FAVOR / 100});

//         if (best == fsst)
//             queue->Push(SchemeAlgorithm::Fsst);
//         else if (best == dict)
//             queue->Push(SchemeAlgorithm::Dictionary);
//         else
//             queue->Push(SchemeAlgorithm::Uncompressed);

//         return best;
//     }
// };
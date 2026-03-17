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

template <typename T>
struct EstimateData
{
    T *src;
    u32 nitems;
    ValidityMask *nullmap;
    u8 depth;

    EstimateData(T *src, u32 nitems, ValidityMask *nullmap, u8 depth = 0) : src(src), nitems(nitems), nullmap(nullmap), depth(depth) {}
};

template <typename T>
inline u32 EstimateNext(EstimateData<T> data, SlabArena *arena)
{
    if constexpr (std::is_floating_point_v<T>)
        return EstimateDouble(data, arena);
    else if constexpr (std::is_integral_v<T>)
        return EstimateInteger(data, arena);
    else if constexpr (std::is_same_v<T, std::string>) // TODO change this is not string
        return EstimateString(data, arena);
    else
        throw std::runtime_error("type doesnt exist");
}

template <typename T>
u32 EstimateInteger(EstimateData<T> data, SlabArena *arena)
{
    if (data.depth > MAX_COMPRESSION_DEPTH)
    {
        return data.nitems * sizeof(T);
    }

    data.depth++;

    return std::min({EstimateDictionary(data, arena),
                     EstimateRle(data, arena),
                     EstimateBp(data)});
}

template <typename T>
u32 EstimateDictionary(EstimateData<T> data, SlabArena *arena)
{
    DictionaryValueEncodedRes<T> result{
        .codes = arena->Alloc<u32>(data.nitems),
        .values = arena->Alloc<T>(data.nitems),
        .valCount = 0};
    DictionaryValueEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

    auto codes = EstimateData(result.codes, data.nitems, data.nullmap, data.depth);

    u32 val1 = EstimateNext(codes, arena);

    auto values = EstimateData(result.values, result.valCount, data.nullmap, data.depth);

    u32 val2 = EstimateNext(values, arena);

    return val1 + val2;
}

template <typename T>
u32 EstimateRle(EstimateData<T> data, SlabArena *arena)
{
    RleEncodedRes<T> result{
        .values = arena->Alloc<T>(data.nitems),
        .counts = arena->Alloc<u16>(data.nitems),
        .count = 0};
    RleEncoder::Encode(&result, data.src, data.nullmap, data.nitems);

    auto values = EstimateData(result.values, result.count, data.nullmap, data.depth);

    u32 val1 = EstimateNext(values, arena);

    auto counts = EstimateData(result.counts, result.count, data.nullmap, data.depth);

    u32 val2 = EstimateNext(counts, arena);

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
u32 EstimateDouble(EstimateData<T> data, SlabArena *arena)
{
    if (data.depth > MAX_COMPRESSION_DEPTH)
    {
        return data.nitems * sizeof(T);
    }

    data.depth++;

    return std::min({EstimateDictionary(data, arena),
                     EstimateRle(data, arena),
                     EstimateBp(data),
                     EstimateFrequency(data, arena)});
}

template <typename T>
u32 EstimateFrequency(EstimateData<T> data, SlabArena *arena)
{
    if constexpr (std::is_floating_point_v<T>) // TODO hack should work on all types
    {
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

        // FreqEncodedRes{
        //     .bitmap =
        // }

        // auto exceptions = arena->Alloc<T>(data.nitems);
        // FreqEncoder::Encode(exceptions, data.src, data.nullmap, data.nitems, topVal)
    }

    return UINT32_MAX;
}

struct EstimateSamplingVisitorState
{
    const void *src;
    u32 nitems;
    SrcType src_type;
};

struct EstimateSamplingVisitor : IVisitor
{
    using T = u32;

    const void *src;
    SrcType src_type;
    u32 nitems;
    SlabArena *arena;
    ValidityMask *nullmap;

    EstimateSamplingVisitor(SrcType src_type, void *src, u32 nitems, SlabArena *arena, ValidityMask *nullmap)
        : src(src),
          src_type(src_type),
          nitems(nitems),
          arena(arena),
          nullmap(nullmap)
    {
    }

    inline EstimateSamplingVisitorState SaveState()
    {
        return {src, nitems, src_type};
    }

    inline void RestoreState(const EstimateSamplingVisitorState &state)
    {
        src = state.src;
        src_type = state.src_type;
        nitems = state.nitems;
    }

    inline void PrepState(const void *new_src, SrcType new_type, u32 new_nitems)
    {
        src = new_src;
        src_type = new_type;
        nitems = new_nitems;
    }

    inline u32 VisitCompressionNodes(std::span<INode *> children, u8 &best_node_idx)
    {
        u32 local_best = UINT32_MAX;
        u32 idx = 0;

        for (auto *child : children)
        {
            if (!child)
            {
                idx++;
                continue;
            }

            u32 cost = child->Accept(*this);
            if (cost < local_best)
            {
                local_best = cost;
                best_node_idx = idx;
            }
            idx++;
        }
        // // std::cout << local_best << std::endl;

        return local_best;
    }

    u32 Visit(IntegerNode &node) override
    {
        // std::cout << "int visited" << std::endl;

        return VisitCompressionNodes(node.children, node.best_node_idx);
    }

    u32 Visit(DoubleNode &node) override
    {
        // std::cout << "dbl visited" << std::endl;

        return VisitCompressionNodes(node.children, node.best_node_idx);
    }

    u32 Visit(StringNode &node) override
    {
        // std::cout << "dbl visited" << std::endl;

        return VisitCompressionNodes(node.children, node.best_node_idx);
    }

    u32 Visit(UncompressedNode &) override
    {
        // std::cout << "uncom visited" << std::endl;

        return SizeOfBuffer(src_type, nitems);
    }

    template <typename T>
    void DictEncodeTemplated(u32 *codes, T *values, u32 &valCount)
    {
        DictionaryValueEncodedRes<T> result{
            .codes = codes,
            .values = values,
            .valCount = 0};
        auto castSrc = reinterpret_cast<const T *>(src);
        DictionaryValueEncoder::Encode(&result, castSrc, nullmap, nitems);
        valCount = result.valCount;
    }

    u32 Visit(DictionaryNode &node) override
    {
        // std::cout << "dict visited" << std::endl;

        if (src_type == SrcType::STR)
        {
            // TODO add str compress
            return 0;
        }
        else
        {
            u32 valCount;
            void *codes, *values;
            DispatchType(src_type, [&]<typename T>() { //
                codes = arena->Alloc<u32>(nitems);
                values = arena->Alloc<T>(nitems);
                DictEncodeTemplated((u32 *)codes, (T *)values, valCount);
            });

            auto state = SaveState();

            PrepState(codes, SrcType::U32, state.nitems);

            u32 val1 = node.codes_node->Accept(*this);

            PrepState(values, state.src_type, valCount);

            u32 val2 = node.values_node->Accept(*this);

            RestoreState(state); // This is just a guard if i add something after left/right node search

            return val1 + val2;
        }
    }

    template <typename T>
    inline void RleEncodeTemplated(u16 *counts, T *values, u32 &count)
    {
        RleEncodedRes<T> result{
            .values = values,
            .counts = counts,
            .count = 0};
        auto castSrc = reinterpret_cast<const T *>(src);
        RleEncoder::Encode(&result, castSrc, nullmap, nitems);
        count = result.count;
    }

    u32 Visit(RleNode &node) override
    {
        // std::cout << "rle visited" << std::endl;

        void *counts, *values;
        u32 count;
        DispatchType(src_type, [&]<typename T>() { //
            counts = arena->Alloc<u16>(nitems);
            values = arena->Alloc<T>(nitems);
            RleEncodeTemplated((u16 *)counts, (T *)values, count);
        });

        auto state = SaveState();

        PrepState(values, state.src_type, count);

        u32 val1 = node.values_node->Accept(*this);

        PrepState(counts, SrcType::U16, count);

        u32 val2 = node.lens_node->Accept(*this);

        RestoreState(state); // This is just a guard if i add something after left/right node search

        return val1 + val2;
    }

    template <typename T>
    inline void BitpackEncodeTemplated(T *out, T *in, u32 nitems, u32 &size, u32 &usedBits)
    {
        static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");
        static_assert(!std::is_same_v<T, u8>, "Bitpacking not supported for u8");

        T max = 0;
        for (u32 i = 0; i < nitems; i++)
            max |= in[i];

        usedBits = CountBitsUsed(max);

        T *newOut = BitPackCombinedEncoder<T>::Encode(out, in, nitems, usedBits);

        size = (newOut - out) * sizeof(T);
    }

    u32 Visit(BitpackNode &node) override
    {
        // std::cout << "bitpack visited" << std::endl;

        u32 size = 0;
        u32 usedBits = 0;
        DispatchType(src_type, [&]<typename T>()
                     { if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T,u8>){
                        
                        node.buf = arena->Alloc<T>(nitems); // TODO calculate size

                        BitpackEncodeTemplated((T *)node.buf, (T *)src, nitems, size, usedBits);
                     } else 
                        throw std::runtime_error("bitpack floating point err"); });

        return size;
    }

    template <typename T>
    inline void FastPForEncodeTemplated(u32 *out, T *in, u32 nitems, u32 &size)
    {
        static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");
        static_assert(!std::is_same_v<T, u8>, "Bitpacking not supported for u8");

        FastPForEncoder encoder; // TODO make this stateless (static encode and decode)
        size = encoder.Encode<T>(out, in, nitems) * sizeof(u32);
    }

    u32 Visit(FastPForNode &) override
    {
        return UINT32_MAX;
    }

    u32 Visit(ForNode &node) override
    {
        // std::cout << "for visited" << std::endl;

        void *values;
        DispatchType(src_type, [&]<typename T>() { //
            values = arena->Alloc<T>(nitems);

            T *in = (T *)src;

            T min = in[0];
            for (u32 i = 1; i < nitems; i++)
                min = std::min(min, in[i]);

            ForEncoder::Encode((T *)values, (__m256i *)src, nitems, min);
        });

        auto state = SaveState();

        PrepState(values, state.src_type, state.nitems);

        u32 val = node.values_node->Accept(*this);

        RestoreState(state); // This is just a guard if i add something after left/right node search

        return val;
    }
};
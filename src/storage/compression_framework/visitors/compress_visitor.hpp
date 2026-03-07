#pragma once

#include "common.hpp"
#include "../nodes/types.hpp"
#include "../nodes/nodes.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include "nullbitmap.hpp"

#include <cstring>

struct CompressVisitorState
{
    const void *src;
    u32 nitems;
    SrcType src_type;
};

struct CompressVisitor : IVisitor
{
    const IStats *stats;
    const void *src;
    SrcType src_type;
    ValidityMask *nullmap;
    u32 nitems;

    u8 *init_data;
    u8 *data;

    u8 *init_header;
    u8 *header;

    u32 *init_offsets;
    u32 *offsets;

    CompressVisitor(IStats *stats, SrcType src_type, void *src, ValidityMask *nullmap, u32 nitems, u8 *out)
        : stats(stats), src(src), src_type(src_type), nullmap(nullmap), nitems(nitems), init_data(out), data(out)
    { // TODO pool it and make values scale based on depth
        init_header = (u8 *)malloc(64);
        header = init_header;
        init_offsets = (u32 *)malloc(32 * sizeof(u32));
        offsets = init_offsets;
    }

    inline void Write(const void *buf, u32 size)
    { // TODO align data
        memcpy(data, buf, size);
        data += size;
        PushOffset(data - init_data);
    }

    inline void PushOffset(u32 off)
    {
        *(offsets++) = off;
    }

    inline void PushHeader(u8 val)
    {
        *(header++) = val;
    }

    inline CompressVisitorState SaveState()
    {
        return {src, nitems, src_type};
    }

    inline void RestoreState(const CompressVisitorState &state)
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

    u32 Visit(NumberNode &node) override
    {
        std::cout << "num visited" << std::endl;

        INode *cur = node.children[node.best_node_idx];

        PushHeader(node.best_node_idx);

        return cur->Accept(*this);
    }

    u32 Visit(UncompressedNode &) override
    {
        std::cout << "uncom visited" << std::endl;

        u32 size = SizeOfBuffer(src_type, nitems);

        Write(src, size);

        return size;
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
        std::cout << "dict visited" << std::endl;

        // TODO should take from pool

        u32 valCount;
        void *codes, *values;

        DispatchType(src_type, [&]<typename T>() { // TODO pool
            codes = new u32[nitems];
            values = new T[nitems];
            DictEncodeTemplated((u32 *)codes, (T *)values, valCount);
        });

        // SwitchDictTypes(codes, values, valCount);

        // Collect stats again

        // Compare w estimated stats

        auto state = SaveState();

        PrepState(codes, SrcType::U32, state.nitems);

        u32 val1 = node.codes_node->Accept(*this);

        PrepState(values, state.src_type, valCount);

        u32 val2 = node.values_node->Accept(*this);

        RestoreState(state); // This is just a guard if i add something after left/right node search

        return val1 + val2;
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
        std::cout << "rle visited" << std::endl;

        // TODO should take from pool

        void *counts, *values;
        u32 count;
        DispatchType(src_type, [&]<typename T>()
                     { 
            counts = new u16[nitems];
            values = new T[nitems];
            RleEncodeTemplated((u16 *)counts, (T *)values, count); });

        // Collect stats again

        // Compare w estimated stats

        auto state = SaveState();

        PrepState(values, state.src_type, state.nitems);

        u32 val1 = node.values_node->Accept(*this);

        PrepState(counts, SrcType::U16, count);

        u32 val2 = node.lens_node->Accept(*this);

        RestoreState(state); // This is just a guard if i add something after left/right node search

        return val1 + val2;
    }

    template <typename T>
    inline void BitpackEncodeTemplated(T *out, T *in, u32 nitems, u32 &size)
    {
        static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");

        T max = 0;
        for (u32 i = 0; i < nitems; i++)
            max |= in[i];

        u32 usedBits = CountBitsUsed(max);

        T *newOut = BitPackEncoder<T>::Encode(out, in, nitems, usedBits);
        size = (newOut - out) * sizeof(T);

        // TODO
        data = reinterpret_cast<u8 *>(newOut);
    }

    u32 Visit(BitpackNode &) override
    {
        std::cout << "bitpack visited" << std::endl;

        u32 size = 0;
        DispatchType(src_type, [&]<typename T>()
                     { if constexpr (!std::is_floating_point_v<T>)
                        BitpackEncodeTemplated((T *)data, (T *)src, nitems, size); });

        PushOffset(size);

        return size;
    }
};
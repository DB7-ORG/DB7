#pragma once

#include "../nodes/types.hpp"
#include "common.hpp"
#include "nullbitmap.hpp"
#include "../nodes/nodes.hpp"
#include "slab_arena.hpp"
#include "../../compressions/compression.hpp"

struct DecompressVisitorState
{
    u32 nitems;
    SrcType src_type;
};

struct DecompressVisitor : IVisitor
{
    SrcType src_type;
    ValidityMask *nullmap;
    u32 nitems;
    u8 *data;
    u8 *header;
    u32 *offsets;
    u32 last_off;

    SlabArena *arena;

    DecompressVisitor(
        SrcType src_type,
        ValidityMask *nullmap,
        u32 nitems,
        u8 *data,
        u8 *header,
        u32 *offsets,
        SlabArena *arena)
        : src_type(src_type),
          nullmap(nullmap),
          nitems(nitems),
          data(data),
          header(header),
          offsets(offsets),
          last_off(0),
          arena(arena)
    {
    }

    inline u8 PopHeader()
    {
        return *(header++);
    }

    inline u32 PopOffset()
    {
        return *(offsets++);
    }

    inline DecompressVisitorState SaveState()
    {
        return {nitems, src_type};
    }

    inline void RestoreState(const DecompressVisitorState &state)
    {
        src_type = state.src_type;
        nitems = state.nitems;
    }

    inline void PrepState(SrcType new_type, u32 new_nitems)
    {
        src_type = new_type;
        nitems = new_nitems;
    }

    u32 Visit(IntegerNode &node) override
    {
        // std::cout << "int visited" << std::endl;

        u8 alg = PopHeader();

        INode *cur = node.children[alg];

        cur->Accept(*this);

        node.buf = cur->buf;

        return 0;
    }

    u32 Visit(DoubleNode &node) override
    {
        // std::cout << "dbl visited" << std::endl;

        u8 alg = PopHeader();

        INode *cur = node.children[alg];

        cur->Accept(*this);

        node.buf = cur->buf;

        return 0;
    }

    u32 Visit(StringNode &node) override
    {
        // std::cout << "dbl visited" << std::endl;

        u8 alg = PopHeader();

        INode *cur = node.children[alg];

        cur->Accept(*this);

        node.buf = cur->buf;

        return 0;
    }

    u32 Visit(UncompressedNode &node) override
    {
        // std::cout << "uncom visited" << std::endl;

        u32 offset = PopOffset();

        DispatchType(src_type, [&]<typename T>() { //
            last_off += GetAlignment<T>(last_off);
        });

        node.buf = &data[last_off];

        last_off = offset;

        return 0;
    }

    template <typename T>
    void DictDecodeTemplated(u32 *codes, T *values, u32 &valCount, T *out)
    {
        DictionaryValueEncodedRes<T> result{
            .codes = codes,
            .values = values,
            .valCount = nitems};
        DictionaryValueEncoder::Decode(out, &result, nitems);
        valCount = result.valCount;
    }

    u32 Visit(DictionaryNode &node) override
    {
        // std::cout << "dict visited" << std::endl;

        if (src_type == SrcType::STR)
        {
            // TODO add dict decode
        }
        else
        {
            u32 valCount = PopOffset();

            auto state = SaveState();

            PrepState(SrcType::U32, state.nitems);

            node.codes_node->Accept(*this);

            PrepState(state.src_type, valCount);

            node.values_node->Accept(*this);

            RestoreState(state);

            auto codes = node.codes_node->buf;
            auto values = node.values_node->buf;
            DispatchType(src_type, [&]<typename T>() { //
                node.buf = arena->Alloc<T>(nitems);
                DictDecodeTemplated((u32 *)codes, (T *)values, nitems, (T *)node.buf);
            });
        }

        return 0;
    }

    template <typename T>
    void RleDecodeTemplated(u16 *counts, T *values, u32 &count, T *out)
    {
        RleEncodedRes<T> result{
            .values = values,
            .counts = counts,
            .count = count};
        RleEncoder::Decode(out, &result);
        count = result.count;
    }

    u32 Visit(RleNode &node) override
    {
        // std::cout << "rle visited" << std::endl;

        u32 count = PopOffset();

        auto state = SaveState();

        PrepState(state.src_type, count);

        node.values_node->Accept(*this);

        PrepState(SrcType::U16, count);

        node.lens_node->Accept(*this);

        RestoreState(state);

        void *values = node.values_node->buf;
        void *lens = node.lens_node->buf;
        DispatchType(src_type, [&]<typename T>() { //
            constexpr u32 SIMD_PAD = 32;
            node.buf = arena->Alloc<u8>(nitems * sizeof(T) + SIMD_PAD);

            RleDecodeTemplated((u16 *)lens, (T *)values, count, (T *)node.buf);
        });

        return 0;
    }

    template <typename T>
    inline void BitpackDecodeTemplated(T *out, T *in, u32 nitems, u32 usedBits)
    {
        static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");
        static_assert(!std::is_same_v<T, u8>, "Bitpacking not supported for u8");

        BitPackCombinedEncoder<T>::Decode(out, in, nitems, usedBits);
    }

    u32 Visit(BitpackNode &node) override
    {
        u32 offset = PopOffset();
        u32 usedBits = PopOffset();

        DispatchType(src_type, [&]<typename T>()
                     { if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T,u8>){
                        
                        last_off += GetAlignment<T>(last_off);
                        auto tmp = &data[last_off];

                        node.buf = arena->Alloc<T>(nitems); 
                        BitpackDecodeTemplated((T *)node.buf, (T *)tmp, nitems, usedBits);
                    } else 
                        throw std::runtime_error("bitpacking floating point unsupported"); });

        last_off = offset;
        return 0;
    }

    template <typename T>
    inline void FastPForDecodeTemplated(T *out, u32 *in, u32 nitems)
    {
        static_assert(!std::is_floating_point_v<T>, "Bitpacking not supported for floating point types");
        static_assert(!std::is_same_v<T, u8>, "Bitpacking not supported for u8");

        FastPForEncoder encoder; // TODO make this stateless (static encode and decode)
        encoder.Decode(out, in, nitems);
    }

    u32 Visit(FastPForNode &node) override
    {
        u32 offset = PopOffset();

        DispatchType(src_type, [&]<typename T>()
                     { if constexpr (!std::is_floating_point_v<T> && !std::is_same_v<T,u8>){
                        
                        last_off += GetAlignment<T>(last_off);
                        auto tmp = &data[last_off];

                        node.buf = arena->Alloc<T>(nitems); 
                        FastPForDecodeTemplated((T *)node.buf, (u32 *)tmp, nitems);
                    } else 
                        throw std::runtime_error("bitpacking floating point unsupported"); });

        last_off = offset;
        return 0;
    }

    u32 Visit(ForNode &node) override
    {
        // std::cout << "rle visited" << std::endl;

        u32 delta = PopOffset();

        auto state = SaveState();

        PrepState(state.src_type, state.nitems);

        node.values_node->Accept(*this);

        RestoreState(state);

        void *values = node.values_node->buf;
        DispatchType(src_type, [&]<typename T>() { //
            node.buf = arena->Alloc<T>(nitems);

            ForEncoder::Decode((T *)node.buf, (__m256i *)values, nitems, (T)delta);
        });

        return 0;
    }
};
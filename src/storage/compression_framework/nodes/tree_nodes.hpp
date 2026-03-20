#pragma once

#include "common.hpp"
#include "../visitors/models.hpp"
#include "align_utils.hpp"
#include "types.hpp"
#include "slab_arena.hpp"
#include <cstring>

//
// FORWARD
//
template <typename T>
struct UncompressedNode;
template <typename T>
struct BitpackNode;
template <typename T>
struct DictionaryNode;
template <typename T>
struct RleNode;
template <typename T>
struct FsstNode;
template <typename T>
struct FrequencyNode;

template <typename T>
struct DoubleNode;
template <typename T>
struct IntegerNode;

template <typename T>
struct IntegerNode
{
    SlabArena *arena;

    UncompressedNode<T> *unc;
    DictionaryNode<T> *dict;
    RleNode<T> *rle;
    BitpackNode<T> *bp;

    u8 depth;
    SchemeAlgorithm best_alg;

    IntegerNode(SlabArena *arena, u8 depth = 0) : arena(arena), depth(depth), best_alg(Uncompressed)
    {
        this->depth = depth;

        unc = arena->New<UncompressedNode<T>>();

        if (depth >= MAX_COMPRESSION_DEPTH)
        {
            dict = nullptr;
            rle = nullptr;
            bp = nullptr;
        }
        else
        {
            dict = arena->New<DictionaryNode<T>>(arena, depth);
            rle = arena->New<RleNode<T>>(arena, depth);
            bp = arena->New<BitpackNode<T>>();
        }
    }

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct DoubleNode
{
    SlabArena *arena;

    UncompressedNode<T> *unc;
    DictionaryNode<T> *dict;
    RleNode<T> *rle;
    FrequencyNode<T> *freq;

    u8 depth;
    SchemeAlgorithm best_alg;

    DoubleNode(SlabArena *arena, u8 depth = 0) : arena(arena), depth(depth), best_alg(Uncompressed)
    {
        this->depth = depth;

        unc = arena->New<UncompressedNode<T>>();

        if (depth >= MAX_COMPRESSION_DEPTH)
        {
            dict = nullptr;
            rle = nullptr;
            freq = nullptr;
        }
        else
        {
            dict = arena->New<DictionaryNode<T>>(arena, depth);
            rle = arena->New<RleNode<T>>(arena, depth);
            freq = arena->New<FrequencyNode<T>>();
        }
    }

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct StringNode
{
    SlabArena *arena;

    UncompressedNode<T> *unc;
    DictionaryNode<T> *dict;
    FsstNode<T> *fsst;

    u8 depth;
    SchemeAlgorithm best_alg;

    StringNode(SlabArena *arena, u8 depth = 0) : arena(arena), depth(depth), best_alg(Uncompressed)
    {
        this->depth = depth;

        unc = arena->New<UncompressedNode<T>>();

        if (depth >= MAX_COMPRESSION_DEPTH)
        {
            dict = nullptr;
            fsst = nullptr;
        }
        else
        {
            dict = arena->New<DictionaryNode<T>>(arena, depth);
            fsst = arena->New<FsstNode<T>>();
        }
    }

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct UncompressedNode
{
    UncompressedNode() {};

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct DictionaryNode
{
    std::conditional_t<std::is_floating_point_v<T>, DoubleNode<T>, IntegerNode<T>> *values_node;
    IntegerNode<T> *codes_node;
    u8 depth;

    DictionaryNode(SlabArena *arena, u8 depth)
    {
        this->depth = depth;
        u8 newDepth = depth + 1;

        if constexpr (std::is_floating_point_v<T>)
        {
            values_node = arena->New<DoubleNode<T>>(arena, newDepth);
        }
        else
        {
            values_node = arena->New<IntegerNode<T>>(arena, newDepth);
        }

        codes_node = arena->New<IntegerNode<T>>(arena, newDepth);
    }

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct RleNode
{
    std::conditional_t<std::is_floating_point_v<T>, DoubleNode<T>, IntegerNode<T>> *values_node;
    IntegerNode<T> *lens_node;
    u8 depth;

    RleNode(SlabArena *arena, u8 depth)
    {
        this->depth = depth;
        u8 newDepth = depth + 1;

        if constexpr (std::is_floating_point_v<T>)
        {
            values_node = arena->New<DoubleNode<T>>(arena, newDepth);
        }
        else
        {
            values_node = arena->New<IntegerNode<T>>(arena, newDepth);
        }

        lens_node = arena->New<IntegerNode<T>>(arena, newDepth);
    }

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct BitpackNode
{
    BitpackNode() {};

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct FsstNode
{
    FsstNode() {};

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};

template <typename T>
struct FrequencyNode
{
    FrequencyNode() {};

    template <typename Visitor>
    u32 Accept(Visitor &visitor) { return visitor.Visit(*this); }
};
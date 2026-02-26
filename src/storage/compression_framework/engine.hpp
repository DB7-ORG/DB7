#pragma once

#include "common.hpp"

#include "nodes/types.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include "../compressions/compression.hpp"

#include <vector>

using T = u32;

// static inline constexpr T EstimateUncompressed(const u32 nitems)
// {
//     return nitems * sizeof(T);
// }

// static u32 EstimateCompression(SchemaType type, NumberStats<T> &stats)
// {
//     switch (type)
//     {
//     case Uncompressed:
//         return EstimateUncompressed(stats.nitems);
//     case Bitpacking:
//         return BitPackEncoder<T>::EstimateCompression(stats.max, stats.nitems);
//     case Dictionary:
//         return DictionaryValueEncoder<T>::EstimateCompression(stats.distinct_values.Size(), stats.nitems);
//     case FastPFor:
//         return FastPForEncoder::EstimateCompression<T>(stats.bitFreq, stats.total_size);
//     case Oneval:
//         return OneValEncoder<T>::EstimateCompression(stats.distinct_values.Size());
//     case Rle:
//         return RleEncoder<T>::EstimateCompression(stats.count_run_len);
//     default:
//         throw std::runtime_error("Unsupported type in CompressSample");
//     }
// }

struct DictionaryNode;
struct RleNode;
struct NumberNode;

struct IVisitor
{
    virtual u32 Visit(DictionaryNode &node) = 0;
    virtual u32 Visit(RleNode &node) = 0;
    virtual u32 Visit(NumberNode &node) = 0;
};

struct INode
{
    u8 depth;
    virtual u32 Accept(IVisitor &visitor) = 0;
};

struct NumberNode : INode
{
    INode *children[2];
    NumberNode(u8 depth = 0);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct DictionaryNode : INode
{
    NumberNode *values_node;
    NumberNode *codes_node;
    DictionaryNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct RleNode : INode
{
    NumberNode *values_node;
    NumberNode *codes_node;
    RleNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

inline NumberNode::NumberNode(u8 depth)
{
    this->depth = depth;

    if (depth == MAX_DEPTH)
        return;

    u8 newDepth = depth + 1;
    children[0] = new DictionaryNode(newDepth);
    children[1] = new RleNode(newDepth);
}

inline DictionaryNode::DictionaryNode(u8 depth)
{
    this->depth = depth;

    if (depth == MAX_DEPTH)
        return;

    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    codes_node = new NumberNode(newDepth);
}

inline RleNode::RleNode(u8 depth)
{
    this->depth = depth;

    if (depth == MAX_DEPTH)
        return;

    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    codes_node = new NumberNode(newDepth);
}

struct EstimateCostVisitor : IVisitor
{
    u32 Visit(NumberNode &node) override
    {
        std::cout << "num visited" << std::endl;

        if (node.depth == MAX_DEPTH)
            return 0;

        for (auto *child : node.children)
        {
            child->Accept(*this);
        }

        return 0;
    }

    u32 Visit(DictionaryNode &node) override
    {
        std::cout << "dict visited" << std::endl;

        if (node.depth == MAX_DEPTH)
            return 0;

        node.codes_node->Accept(*this);
        node.values_node->Accept(*this);

        return 0;
    }

    u32 Visit(RleNode &node) override
    {
        std::cout << "rle visited" << std::endl;

        if (node.depth == MAX_DEPTH)
            return 0;

        node.codes_node->Accept(*this);
        node.values_node->Accept(*this);

        return 0;
    }
};
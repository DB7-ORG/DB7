#pragma once

#include "common.hpp"

#include "nodes/types.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include "../compressions/compression.hpp"

#include <vector>
#include <cstring>

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

    DictionaryNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct RleNode : INode
{
    NumberStats stats;
    NumberNode *lens_node;
    NumberNode *values_node;
    RleNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

inline NumberNode::NumberNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_DEPTH)
        return;

    children[0] = new DictionaryNode(depth);
    children[1] = new RleNode(depth);
}

inline DictionaryNode::DictionaryNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_DEPTH)
        return;

    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
}

inline RleNode::RleNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_DEPTH)
        return;

    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    lens_node = new NumberNode(newDepth);
}

struct StatsTransformer
{
    static NumberStats DictionaryTransform(NumberStats *stats)
    {
        return NumberStats(
            stats->uniqueBitFreq,                        // bitFreq
            stats->uniqueBitFreq,                        // uniqueBitFreq
            stats->count_distinct,                       // num_items
            stats->count_distinct * stats->size_of_type, // total_size
            std::min(1u, stats->null_count),             // null_count
            stats->count_distinct,                       // count_run_len
            1,                                           // average_run_len
            stats->count_distinct,                       // count_distinct
            stats->is_sorted_asc,                        // is_sorted_asc
            stats->is_sorted_desc,                       // is_sorted_desc
            stats->size_of_type);                        // size_of_type
    }

    static StringStats *DictionaryTransform(StringStats *stats)
    {
        return stats;
    }

    static NumberStats RleLensTransform(NumberStats *stats)
    {
        u32 typeSize = sizeof(u16);
        return NumberStats(
            stats->uniqueBitFreq,            // bitFreq        ?
            stats->uniqueBitFreq,            // uniqueBitFreq  ?
            stats->count_run_len,            // num_items
            stats->count_run_len * typeSize, // total_size
            0,                               // null_count
            UINT32_MAX,                      // count_run_len  ?
            1,                               // average_run_len
            11111111,                        // TODO // count_distinct ?
            false,                           // is_sorted_asc
            false,                           // is_sorted_desc
            typeSize);                       // size_of_type
    }

    static NumberStats RleValsTransform(NumberStats *stats)
    {
        return NumberStats(
            stats->uniqueBitFreq,                        // bitFreq
            stats->uniqueBitFreq,                        // uniqueBitFreq
            stats->count_distinct,                       // num_items
            stats->count_distinct * stats->size_of_type, // total_size
            1,                                           // null_count
            stats->count_distinct,                       // count_run_len
            1,                                           // average_run_len
            stats->count_distinct,                       // count_distinct
            stats->is_sorted_asc,                        // is_sorted_asc
            stats->is_sorted_desc,                       // is_sorted_desc
            stats->size_of_type);                        // size_of_type
    }
};

struct EstimateCostVisitor : IVisitor
{
    IStats *current_stats;

    EstimateCostVisitor(IStats *stats) : current_stats(stats) {}

    u32 Visit(NumberNode &node) override
    {
        if (node.depth >= MAX_DEPTH)
            return 0;

        std::cout << "num visited" << std::endl;

        u32 local_best = UINT32_MAX;
        for (auto *child : node.children)
        {
            u32 cost = child->Accept(*this);
            local_best = std::min(local_best, cost); // TODO save best seq also
        }
        std::cout << local_best << std::endl;
        return local_best;
    }

    u32 Visit(DictionaryNode &node) override
    {
        if (node.depth >= MAX_DEPTH)
            return 0;

        std::cout << "dict visited" << std::endl;

        if (current_stats->type == StatsType::Number)
        {
            NumberStats *stats = static_cast<NumberStats *>(current_stats);
            u32 packed_codes_size = DictionaryValueEncoder::EstimateCompression(stats->count_distinct, stats->num_items, stats->size_of_type);
            NumberStats values_stats = StatsTransformer::DictionaryTransform(stats);
            current_stats = &values_stats;
            return packed_codes_size + node.values_node->Accept(*this);
        }
        else if (current_stats->type == StatsType::String)
        {
            // TODO
            node.values_node->Accept(*this);
        }
        else
        {
            throw std::runtime_error("Unhandled StatsType");
        }

        return UINT32_MAX;
    }

    u32 Visit(RleNode &node) override
    {
        if (node.depth >= MAX_DEPTH)
            return 0;

        std::cout << "rle visited" << std::endl;

        NumberStats *stats = static_cast<NumberStats *>(current_stats);
        u32 len_size, val_size;
        RleEncoder::EstimateCompression(stats->count_run_len, stats->size_of_type, len_size, val_size);
        NumberStats len_stats = StatsTransformer::RleLensTransform(stats);
        NumberStats val_stats = StatsTransformer::RleValsTransform(stats);

        current_stats = &len_stats;
        u32 len_cost = node.lens_node->Accept(*this);

        current_stats = &val_stats;
        u32 val_cost = node.values_node->Accept(*this);

        return len_cost + val_cost;
    }
};
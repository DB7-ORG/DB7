#pragma once

#include "common.hpp"

#include "nodes/types.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include "../compressions/compression.hpp"

#include <vector>
#include <cstring>
#include <unordered_set>
#include <array>

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
    u32 best_node_idx;
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
    NumberStats stats;
    NumberNode *lens_node;
    NumberNode *values_node;
    RleNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

inline NumberNode::NumberNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
        return;

    best_node_idx = 0;
    children[0] = new DictionaryNode(depth);
    children[1] = new RleNode(depth);
}

inline DictionaryNode::DictionaryNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
        return;

    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    codes_node = new NumberNode(newDepth);
}

inline RleNode::RleNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
        return;

    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    lens_node = new NumberNode(newDepth);
}

struct StatsAproxTransformer
{
private:
    static void ScaleBitFreq(const u32 bitFreq[MAX_HIST_SIZE], const u32 nitems, const u32 ndistinct, u32 outFreq[MAX_HIST_SIZE])
    {
        u32 distinctPerBucket[MAX_HIST_SIZE] = {};
        u32 total = 0;
        for (u32 i = 0; i < MAX_HIST_SIZE; i++)
        {
            u64 capacity = (i == 0) ? 1 : (1ULL << i); // 2^i possible values
            u32 capped = (u32)std::min((u64)bitFreq[i], capacity);
            distinctPerBucket[i] = capped;
            total += capped;
        }

        float ratio = (float)ndistinct / std::max(total, 1u);
        for (u32 i = 0; i < MAX_HIST_SIZE; i++)
        {
            outFreq[i] = (u32)(distinctPerBucket[i] * ratio);
        }
    }

public:
    static NumberStats DictionaryValuesTransform(NumberStats *stats)
    {
        u32 newBitFreq[MAX_HIST_SIZE] = {};
        ScaleBitFreq(stats->bitFreq, stats->num_items, stats->count_distinct, newBitFreq);
        return NumberStats(
            newBitFreq,                                  // bitFreq
            stats->count_distinct,                       // num_items
            stats->count_distinct * stats->size_of_type, // total_size
            1,                                           // count_run_len
            stats->count_distinct,                       // count_distinct
            stats->min,                                  // min
            stats->max,                                  // max
            stats->size_of_type);                        // size_of_type
    }

    static NumberStats DictionaryCodesTransform(NumberStats *stats)
    {
        return NumberStats(
            nullptr,                                // bitFreq //TODO (should be INF)
            stats->num_items,                       // num_items
            stats->num_items * stats->size_of_type, // total_size
            stats->count_run_len,                   // count_run_len
            stats->count_distinct,                  // count_distinct
            0,                                      // min
            stats->count_distinct - 1,              // max
            stats->size_of_type);                   // size_of_type
    }

    static StringStats *DictionaryTransform(StringStats *stats)
    {
        return stats;
    }

    static NumberStats RleLensTransform(NumberStats *stats)
    {
        u32 typeSize = sizeof(u16);
        u32 nitems = stats->count_run_len;

        float avgRunLen = (float)stats->num_items / (float)stats->count_run_len;
        u64 min = 1;
        u64 max = (u64)(2 * avgRunLen);

        // count_distinct: run lengths are values in [1, 2*avgRunLen]
        // in average case, distinct lengths ~ sqrt(n) or min(n, 2*avgRunLen)
        u32 distinct = (u32)std::min((float)nitems, 2 * avgRunLen);

        u32 newBitFreq[MAX_HIST_SIZE] = {};
        ScaleBitFreq(stats->bitFreq, stats->num_items, nitems, newBitFreq);

        return NumberStats(
            newBitFreq,        // bitFreq
            nitems,            // num_items
            nitems * typeSize, // total_size
            1,                 // count_run_len
            distinct,          // count_distinct
            min,               // min
            max,               // max
            typeSize);         // size_of_type
    }

    static NumberStats RleValsTransform(NumberStats *stats)
    {
        u32 newBitFreq[MAX_HIST_SIZE] = {};
        ScaleBitFreq(stats->bitFreq, stats->num_items, stats->count_run_len, newBitFreq);
        return NumberStats(
            newBitFreq,                                 // bitFreq
            stats->count_run_len,                       // num_items
            stats->count_run_len * stats->size_of_type, // total_size
            1,                                          // count_run_len
            stats->count_distinct,                      // count_distinct
            stats->min,                                 // min
            stats->max,                                 // max
            stats->size_of_type);
    }
};

enum struct UnknownStats
{
    NumItems,
    BitFreq,
    CountRunLen,
    CountDistinct,
    Min,
    Max
};

struct EstimateCostVisitor : IVisitor
{
    IStats *current_stats;

    EstimateCostVisitor(IStats *stats) : current_stats(stats) {}

    u32 Visit(NumberNode &node) override
    {
        if (node.depth >= MAX_COMPRESSION_DEPTH)
            return 0;

        std::cout << "num visited" << std::endl;

        u32 local_best = UINT32_MAX;
        u32 idx = 0;
        for (auto *child : node.children)
        {
            u32 cost = child->Accept(*this);
            if (cost < local_best)
            {
                local_best = cost;
                node.best_node_idx = idx;
            }
            idx++;
        }
        std::cout << local_best << std::endl;
        return local_best;
    }

    u32 Visit(DictionaryNode &node) override
    {
        if (node.depth >= MAX_COMPRESSION_DEPTH)
            return 0;

        std::cout << "dict visited" << std::endl;

        if (current_stats->type == StatsType::Number)
        {
            NumberStats *stats = static_cast<NumberStats *>(current_stats);

            u32 codeSize, valueSize;
            DictionaryValueEncoder::EstimateCompression(stats->count_distinct, stats->num_items, stats->size_of_type, codeSize, valueSize);

            NumberStats values_stats = StatsAproxTransformer::DictionaryValuesTransform(stats);
            NumberStats codes_stats = StatsAproxTransformer::DictionaryCodesTransform(stats);

            current_stats = &values_stats;
            u32 val_cost = node.values_node->Accept(*this);

            current_stats = &codes_stats;
            u32 code_cost = node.codes_node->Accept(*this);

            return val_cost + code_cost;
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
        if (node.depth >= MAX_COMPRESSION_DEPTH)
            return 0;

        std::cout << "rle visited" << std::endl;

        NumberStats *stats = static_cast<NumberStats *>(current_stats);

        u32 len_size, val_size;
        RleEncoder::EstimateCompression(stats->count_run_len, stats->size_of_type, len_size, val_size);

        NumberStats len_stats = StatsAproxTransformer::RleLensTransform(stats);
        NumberStats val_stats = StatsAproxTransformer::RleValsTransform(stats);

        current_stats = &len_stats;
        u32 len_cost = node.lens_node->Accept(*this);

        current_stats = &val_stats;
        u32 val_cost = node.values_node->Accept(*this);

        return len_cost + val_cost;
    }
};

template <typename ValueType>
struct CompressVisitor : IVisitor
{

    const ValueType *src;
    const ValidityMask *nullmap;
    const u32 count;

    CompressVisitor(const ValueType *in, const ValidityMask *nullmap, const u32 count)
    {
    }

    u32 Visit(NumberNode &node) override
    {
        auto node = node.children[node.best_node_idx];
        node.Accept(*this);
    }

    u32 Visit(DictionaryNode &node) override{
        // compress
        DictionaryValueEncoder::Encode()

        // drop to next node
    }

    u32 Visit(RleNode &node) override
    {
        // compress

        // drop to next node
    }
};
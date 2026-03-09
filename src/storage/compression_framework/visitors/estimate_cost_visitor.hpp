#pragma once

#include "common.hpp"
#include "../nodes/types.hpp"
#include "../nodes/nodes.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"

struct StatsAproxTransformer
{
private:
    static void ScaleBitFreq(const u32 bitFreq[MAX_HIST_SIZE], const u32 nitems, const u32 ndistinct, u32 outFreq[MAX_HIST_SIZE])
    {
        (void)nitems; // TODO unused

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
    static NumberStats DictionaryValuesTransform(const NumberStats *stats)
    {
        u32 newBitFreq[MAX_HIST_SIZE] = {};
        ScaleBitFreq(stats->bitFreq, stats->num_items, stats->count_distinct, newBitFreq);
        return NumberStats(
            newBitFreq,                                  // bitFreq
            stats->count_distinct,                       // num_items
            stats->count_distinct * stats->size_of_type, // total_size
            stats->count_distinct,                       // count_run_len
            stats->count_distinct,                       // count_distinct
            stats->min,                                  // min
            stats->max,                                  // max
            stats->size_of_type);                        // size_of_type
    }

    static NumberStats DictionaryCodesTransform(const NumberStats *stats)
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

    static const StringStats *DictionaryTransform(const StringStats *stats)
    {
        return stats;
    }

    static NumberStats RleLensTransform(const NumberStats *stats)
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

        u32 lensRunLen = (u32)std::max(1.0f, (float)nitems / avgRunLen);

        return NumberStats(
            newBitFreq,        // bitFreq
            nitems,            // num_items
            nitems * typeSize, // total_size
            lensRunLen,        // count_run_len
            distinct,          // count_distinct
            min,               // min
            max,               // max
            typeSize);         // size_of_type
    }

    static NumberStats RleValsTransform(const NumberStats *stats)
    {
        u32 newBitFreq[MAX_HIST_SIZE] = {};
        ScaleBitFreq(stats->bitFreq, stats->num_items, stats->count_run_len, newBitFreq);
        return NumberStats(
            newBitFreq,                                 // bitFreq
            stats->count_run_len,                       // num_items
            stats->count_run_len * stats->size_of_type, // total_size
            stats->count_run_len,                       // count_run_len
            stats->count_distinct,                      // count_distinct
            stats->min,                                 // min
            stats->max,                                 // max
            stats->size_of_type);
    }
};

struct EstimateCostVisitor : IVisitor
{
    IStats *current_stats;

    EstimateCostVisitor(IStats *stats) : current_stats(stats) {}

    u32 Visit(NumberNode &node) override
    {
        std::cout << "num visited" << std::endl;

        u32 local_best = UINT32_MAX;
        u32 idx = 0;
        IStats *stats = current_stats;
        for (auto *child : node.children)
        {
            if (!child)
            {
                idx++;
                continue;
            }
            current_stats = stats;
            u32 cost = child->Accept(*this);
            if (cost < local_best)
            {
                local_best = cost;
                node.best_node_idx = idx;
            }
            idx++;
        }
        // std::cout << local_best << std::endl;
        return local_best;
    }

    u32 Visit(UncompressedNode &) override
    {
        std::cout << "uncompressed visited" << std::endl;

        return current_stats->num_items * current_stats->size_of_type;
    }

    u32 Visit(DictionaryNode &node) override
    {
        std::cout << "dict visited" << std::endl;

        if (current_stats->type == StatsType::Number)
        {
            NumberStats *stats = static_cast<NumberStats *>(current_stats);

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
        std::cout << "rle visited" << std::endl;

        const NumberStats *stats = static_cast<NumberStats *>(current_stats);

        NumberStats len_stats = StatsAproxTransformer::RleLensTransform(stats);
        NumberStats val_stats = StatsAproxTransformer::RleValsTransform(stats);

        current_stats = &len_stats;
        u32 len_cost = node.lens_node->Accept(*this);

        current_stats = &val_stats;
        u32 val_cost = node.values_node->Accept(*this);

        return len_cost + val_cost;
    }

    u32 Visit(BitpackNode &) override
    { // TODO it can bitpack double types
        std::cout << "bp visited" << std::endl;
        auto stats = reinterpret_cast<NumberStats *>(current_stats);
        return BitPackEncoder<u8>::EstimateCompression(stats->max, stats->num_items);
    }
};
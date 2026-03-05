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

// using T = u32;

static inline constexpr u32 EstimateUncompressed(const u32 nitems, const u32 sizeof_type)
{
    return nitems * sizeof_type;
}

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
struct UncompressedNode;

struct IVisitor
{
    virtual u32 Visit(NumberNode &node) = 0;
    virtual u32 Visit(UncompressedNode &node) = 0;
    virtual u32 Visit(DictionaryNode &node) = 0;
    virtual u32 Visit(RleNode &node) = 0;
};

struct INode
{
    u8 depth;
    virtual u32 Accept(IVisitor &visitor) = 0;
};

struct NumberNode : INode
{
    u32 best_node_idx;
    INode *children[3];

    NumberNode(u8 depth = 0);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct UncompressedNode : INode
{
    UncompressedNode(u8 depth);
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
    NumberNode *lens_node;

    RleNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

inline NumberNode::NumberNode(u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
    {
        children[0] = new UncompressedNode(depth);
        children[1] = nullptr;
        children[2] = nullptr;
        return;
    }

    best_node_idx = 0;
    children[0] = new UncompressedNode(depth);
    children[1] = new DictionaryNode(depth);
    children[2] = new RleNode(depth);
}

inline UncompressedNode::UncompressedNode(u8 depth)
{
    this->depth = depth;
    return;
}

inline DictionaryNode::DictionaryNode(u8 depth)
{
    this->depth = depth;
    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    codes_node = new NumberNode(newDepth);
}

inline RleNode::RleNode(u8 depth)
{
    this->depth = depth;
    u8 newDepth = depth + 1;
    values_node = new NumberNode(newDepth);
    lens_node = new NumberNode(newDepth);
}

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

        // TODODODODODODODODODO 120000;
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
        std::cout << local_best << std::endl;
        return local_best;
    }

    u32 Visit(UncompressedNode &) override
    {
        std::cout << "uncompressed visited" << std::endl;

        return EstimateUncompressed(current_stats->num_items, current_stats->size_of_type);
    }

    u32 Visit(DictionaryNode &node) override
    {
        std::cout << "dict visited" << std::endl;

        if (current_stats->type == StatsType::Number)
        {
            NumberStats *stats = static_cast<NumberStats *>(current_stats);

            // u32 codeSize, valueSize;
            // DictionaryValueEncoder::EstimateCompression(stats->count_distinct, stats->num_items, stats->size_of_type, codeSize, valueSize);

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

        // u32 len_size, val_size;
        // RleEncoder::EstimateCompression(stats->count_run_len, stats->size_of_type, len_size, val_size);

        NumberStats len_stats = StatsAproxTransformer::RleLensTransform(stats);
        NumberStats val_stats = StatsAproxTransformer::RleValsTransform(stats);

        current_stats = &len_stats;
        u32 len_cost = node.lens_node->Accept(*this);

        current_stats = &val_stats;
        u32 val_cost = node.values_node->Accept(*this);

        return len_cost + val_cost;
    }
};

enum struct SrcType
{
    U8,
    U16,
    U32,
    U64,
    DBL
};

template <typename Func>
static void DispatchType(SrcType type, Func &&f)
{
    switch (type)
    {
    case SrcType::U8:
        f.template operator()<u8>();
        break;
    case SrcType::U16:
        f.template operator()<u16>();
        break;
    case SrcType::U32:
        f.template operator()<u32>();
        break;
    case SrcType::U64:
        f.template operator()<u64>();
        break;
    case SrcType::DBL:
        f.template operator()<double>();
        break;
    default:
        throw std::runtime_error("unsupported type");
    }
}

static u32 TypeSize(SrcType t)
{
    switch (t)
    {
    case SrcType::U8:
        return sizeof(u8);
    case SrcType::U16:
        return sizeof(u16);
    case SrcType::U32:
        return sizeof(u32);
    case SrcType::U64:
        return sizeof(u64);
    case SrcType::DBL:
        return sizeof(double);
    }
    std::terminate();
}

// template <typename ValueType>
struct CompressVisitor : IVisitor
{
    const IStats *stats;
    const void *src;
    SrcType src_type;
    ValidityMask *nullmap;
    u32 nitems;

    u8 *init_data;
    u8 *data;
    u8 *header;
    u32 *offsets;
    u8 hcount;
    u8 offcount;

    CompressVisitor(IStats *stats, SrcType src_type, void *src, ValidityMask *nullmap, u32 nitems, u8 *out)
        : stats(stats), src(src), src_type(src_type), nullmap(nullmap), nitems(nitems), init_data(out), data(out)
    { // TODO pool it and make values scale based on depth
        hcount = 0;
        offcount = 0;
        header = (u8 *)malloc(64);
        offsets = (u32 *)malloc(32 * sizeof(u32));
    }

    inline void Write(const void *buf, u32 size)
    { // TODO align data
        memcpy(data, buf, size);
        *(offsets++) = data - init_data;
        offcount++;
        data += size;
    }

    inline void WriteHeader(u8 val)
    {
        *(header++) = val;
        hcount++;
    }

    inline u32 SizeOfType()
    {
        return TypeSize(src_type) * nitems;
    }

    u32 Visit(NumberNode &node) override
    {
        std::cout << "num visited" << std::endl;
        INode *cur = node.children[node.best_node_idx];
        return cur->Accept(*this);
    }

    u32 Visit(UncompressedNode &) override
    {
        std::cout << "uncom visited" << std::endl;
        WriteHeader(SchemeAlgorythm::Uncompressed);
        u32 size = SizeOfType();
        Write(src, size);
        return size;
    }

    void SwitchDictTypes(void *&codes, void *&values, u32 &valCount)
    {
        DispatchType(src_type, [&]<typename T>() { // TODO pool
            codes = new T[nitems];
            values = new T[nitems];
            DictEncodeTemplated((T *)codes, (T *)values, valCount);
        });
    }

    template <typename T>
    void DictEncodeTemplated(T *codes, T *values, u32 &valCount)
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
        WriteHeader(SchemeAlgorythm::Dictionary);

        // TODO should take from pool
        void *codes;
        void *values;
        u32 valCount;
        SwitchDictTypes(codes, values, valCount);

        // Collect stats again

        // Compare w estimated stats

        src = codes;
        u32 val1 = node.codes_node->Accept(*this);

        src = values;
        nitems = valCount;
        u32 val2 = node.values_node->Accept(*this);

        return val1 + val2;
    }

    void SwitchRleTypes(void *&counts, void *&values, u32 &count)
    {
        DispatchType(src_type, [&]<typename T>() { // TODO pool
            counts = new u16[nitems];
            values = new T[nitems];
            RleEncodeTemplated((u16 *)counts, (T *)values, count);
        });
    }

    template <typename T>
    void RleEncodeTemplated(u16 *counts, T *values, u32 &count)
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
        WriteHeader(SchemeAlgorythm::Rle);

        // TODO should take from pool
        void *counts;
        void *values;
        u32 count;
        SwitchRleTypes(counts, values, count);

        // Collect stats again

        // Compare w estimated stats

        src = values;
        u32 val1 = node.values_node->Accept(*this);

        src = counts;
        nitems = count;
        src_type = SrcType::U16;
        u32 val2 = node.lens_node->Accept(*this);

        return val1 + val2;
    }
};
#include "engine.hpp"
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include <vector>
#include "../compressions/bitpacking.hpp"
#include "../compressions/dictionary.hpp"
#include "../compressions/compression.hpp"
#include "nodes/types.hpp"
#include "nodes/dictionary_value_node.hpp"

using T = u32;

struct UncompressedNode : AlgNode
{
    SchemeAlgorythm alg = Uncompressed;

    void Next()
    {
    }
};

struct BitpackingNode : AlgNode
{
    SchemeAlgorythm alg = Bitpacking;

    void Next()
    {
        // TODO
    }
};

struct FastPForNode : AlgNode
{
    SchemeAlgorythm alg = FastPFor;

    void Next()
    {
        // TODO
    }
};

struct OnevalNode : AlgNode
{
    SchemeAlgorythm alg = Oneval;

    void Next()
    {
        // TODO
    }
};

struct RleNode : AlgNode
{
    SchemeAlgorythm alg = Rle;
    INode *values_node;
    INode *counts_node;

    RleNode(u8 depth);

    void Next()
    {
        // TODO
    }
};

struct FsstNode : AlgNode
{
    SchemeAlgorythm alg = Fsst;

    void Next()
    {
        // TODO
    }
};

struct FrequencyNode : AlgNode
{
    SchemeAlgorythm alg = Frequency;
    INode *exceptions_node;

    FrequencyNode(u8 depth);

    void Next()
    {
        // TODO
    }
};

// template <typename T>
// u32 CalcScore(SchemeAlgorythm alg, const NumberStats<T> &stats)
// {
//     switch (alg)
//     {
//     case Uncompressed:
//         return stats.total_size;
//     case Bitpacking:
//         return stats.total_size * 1;
//     case Rle:
//         return stats.average_run_len * 1;
//     case Oneval:
//         return stats.distinct_values.Size() == 1 ? 0 : UINT32_MAX;
//     case FastPFor:
//         return 0;
//     case Dictionary:
//         return 0;
//     case Frequency:
//         return 0;
//     default:
//         throw std::runtime_error("Unsupported type in CalcScore number type");
//     }
// }

u32 CalcScore(SchemeAlgorythm alg, const StringStats &stats)
{
    switch (alg)
    {
    case Uncompressed:
        return stats.total_size;
    case Fsst:
        return stats.total_size * 1;
    case Oneval:
        return 0;
    case Dictionary:
        return 0;
    default:
        throw std::runtime_error("Unsupported type in CalcScore string type");
    }
}

static inline constexpr T EstimateUncompressed(const u32 nitems)
{
    return nitems * sizeof(T);
}

u32 EstimateCompression(SchemaType type, NumberStats<T> &stats)
{
    switch (type)
    {
    case Uncompressed:
        return EstimateUncompressed(stats.nitems);
    case Bitpacking:
        return BitPackEncoder<T>::EstimateCompression(stats.max, stats.nitems);
    case Dictionary:
        return DictionaryValueEncoder<T>::EstimateCompression(stats.distinct_values.Size(), stats.nitems);
    case FastPFor:
        return FastPForEncoder::EstimateCompression<T>(stats.bitFreq, stats.total_size);
    case Oneval:
        return OneValEncoder<T>::EstimateCompression(stats.distinct_values.Size());
    case Rle:
        return RleEncoder<T>::EstimateCompression(stats.count_run_len);
    default:
        throw std::runtime_error("Unsupported type in CompressSample");
    }
}

u32 EstimateCompression(std::vector<u32> samples, StringStats &stats)
{
}

struct NumberNode : INode
{
    using T = u32;

    SchemaType type = Number;
    AlgNode *nodes[6];
    NumberStats<T> stats; // TODO

    NumberNode(const T *src, const ValidityMask *nullmap, const u32 nitems, u8 depth = 0) : stats(src, nullmap, nitems)
    {
        nodes[0] = new UncompressedNode();
        nodes[1] = new BitpackingNode();
        nodes[2] = new DictionaryNode(depth, type);
        nodes[3] = new FastPForNode();
        nodes[4] = new OnevalNode();
        nodes[5] = new RleNode(depth);
    }

    void Next()
    {
    }
};

struct StringNode : INode
{
    SchemaType type = String;
    AlgNode *nodes[4];

    StringNode(u8 depth = 0)
    {
        nodes[0] = new UncompressedNode();
        nodes[1] = new OnevalNode();
        nodes[2] = new DictionaryNode(depth, type);
        nodes[3] = new FsstNode();
    }
};

struct DoubleNode : INode
{
    SchemaType type = Double;
    AlgNode *nodes[5];

    DoubleNode(u8 depth = 0)
    {
        nodes[0] = new UncompressedNode();
        nodes[1] = new OnevalNode();
        nodes[2] = new DictionaryNode(depth, type);
        nodes[3] = new RleNode(depth);
        nodes[4] = new FrequencyNode(depth);
    };
};

static INode *switchType(SchemaType type, u32 depth)
{
    switch (type)
    {
    case Number:
        return new NumberNode(nullptr, nullptr, 0, depth);
    case Double:
        return new DoubleNode(depth);
    case String:
        return new StringNode(depth);
    default:
        throw std::runtime_error("Unsupported type in AlgNode");
    }
}

inline RleNode::RleNode(u8 depth)
{
    if (depth >= MAX_DEPTH)
    {
        values_node = nullptr;
        counts_node = nullptr;
        return;
    }
    values_node = new NumberNode(nullptr, nullptr, 0, depth + 1);
    counts_node = new NumberNode(nullptr, nullptr, 0, depth + 1);
}

inline FrequencyNode::FrequencyNode(u8 depth)
{
    if (depth >= MAX_DEPTH)
    {
        exceptions_node = nullptr;
        return;
    }

    exceptions_node = new DoubleNode();
}
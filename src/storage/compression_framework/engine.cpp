#include "engine.hpp"
#include <stdexcept>
#include "../stats/number_stats.hpp"
#include "../stats/string_stats.hpp"
#include <vector>
#include "../compressions/bitpacking.hpp"
#include "../compressions/dictionary.hpp"
#include "../compressions/compression.hpp"

using T = u32;
constexpr u32 MAX_DEPTH = 3;

enum SchemaType
{
    Number,
    Double,
    String
};

enum SchemeAlgorythm
{
    Uncompressed,
    Bitpacking,
    Dictionary,
    FastPFor,
    Frequency,
    Fsst,
    Oneval,
    Rle
};

struct INode
{
};

struct AlgNode
{
    SchemeAlgorythm alg;

    virtual void Next() = 0;
};

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

struct DictionaryNode : AlgNode
{
    SchemeAlgorythm alg = Dictionary;
    INode *values_node;
    INode *codes_node;

    DictionaryNode(u8 depth, SchemaType type);

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

template <typename T>
u32 CalcScore(SchemeAlgorythm alg, const NumberStats<T> &stats)
{
    switch (alg)
    {
    case Uncompressed:
        return stats.total_size;
    case Bitpacking:
        return stats.total_size * 1;
    case Rle:
        return stats.average_run_len * 1;
    case Oneval:
        return stats.distinct_values.Size() == 1 ? 0 : UINT32_MAX;
    case FastPFor:
        return 0;
    case Dictionary:
        return 0;
    case Frequency:
        return 0;
    default:
        throw std::runtime_error("Unsupported type in CalcScore number type");
    }
}

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

// AlgNode *PickBestNode(AlgNode **nodes, u32 size, NumberStats<u32> &stats, std::vector<u32> &samples)
// {
//     AlgNode *best = nodes[0];
//     assert(best->alg == Uncompressed);
//     u32 uncompressed_score = CalcScore(best->alg, stats);
//     u32 best_score = uncompressed_score;

//     for (u32 i = 1; i < size; i++)
//     {
//         AlgNode *node = nodes[i];

//         u32 score = CalcScore(node->alg, stats);
//         if (score < best_score)
//         {
//             u32 newSize = CompressSamples(type, stats);
//             if (newSize < best_score)
//             {
//                 best = node;
//                 best_score = score;
//             }
//         }
//     }

//     return best;
// }

u32 CompressSamples(SchemaType type, NumberStats<T> &stats)
{
    switch (type)
    {
    case Uncompressed:
        return stats.nitems * sizeof(u32);
    case Bitpacking:
    {
        u32 usedBits = CountBitsUsed(stats.max);
        return (stats.nitems * usedBits + sizeof(u8) - 1) / sizeof(u8);
    }
    case Dictionary:
    {
        // if (stats.distinct_values.Size() >= 0.8 * stats.nitems)
        // {
        //     return UINT32_MAX;
        // }
        // DictionaryValueEncodedRes<T> out = {
        //     .codes = nullptr,
        //     .values = nullptr, // TODO
        //     .valCount = 0};
        // DictionaryValueEncoder<T>::Encode(&out, sampleData.samples.data(), sampleData.bitmap, sampleData.size());
        // return out.valCount * (sizeof(T) + sizeof(T));

        u32 uniqNum = stats.distinct_values.Size();
        u32 usedBits = CountBitsUsed(uniqNum + 1);
        u32 codeSize = (stats.nitems * usedBits + sizeof(u8) - 1) / sizeof(u8); // dict should bitpack codes
        u32 valueSize = uniqNum * sizeof(T);
        return codeSize + valueSize;
    }
    case FastPFor:
    {
        // u32 *data = nullptr;
        // FastPForEncoder encoder;
        // u32 size = encoder.Encode(data, sampleData.samples.data(), sampleData.size());
        // return size * sizeof(u32);

        // TODO do something like in here
        // void GetBestB(const u32 *in, u8 &bestb, u8 &bestcexcept, u8 &maxb)
        return 2;
    }
    case Oneval:
        return stats.distinct_values.Size() == 1 ? sizeof(T) : UINT32_MAX;
    case Rle:
        return stats.count_run_len * (sizeof(T) + sizeof(u16));
    default:
        throw std::runtime_error("Unsupported type in CompressSample");
    }
}

u32 CompressSamples(std::vector<u32> samples, StringStats &stats)
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
        stats.GenerateStats();
        SampleStats<T> samples = stats.GenerateSamples();
        // AlgNode *best = PickBestNode(nodes, std::size(nodes), stats, samples);

        // MAIN LOGIC for compressing data
        AlgNode *best = nodes[0];
        assert(best->alg == Uncompressed);
        u32 uncompressed_score = CalcScore(best->alg, stats);
        u32 best_score = uncompressed_score;

        for (u32 i = 1; i < std::size(nodes); i++)
        {
            AlgNode *node = nodes[i];

            u32 score = CalcScore(node->alg, stats);
            if (score < best_score)
            {
                u32 newSize = CompressSamples(type, stats);
                if (newSize < best_score)
                {
                    best = node;
                    best_score = score;
                }
            }
        }

        best->Next();
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

inline DictionaryNode::DictionaryNode(u8 depth, SchemaType type)
{
    if (depth >= MAX_DEPTH)
    {
        values_node = nullptr;
        codes_node = nullptr;
        return;
    }
    codes_node = new NumberNode(nullptr, nullptr, 0, depth + 1);
    values_node = switchType(type, depth + 1);
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
#include "engine.hpp"
#include <stdexcept>
#include "../stats/number_stats.hpp"

constexpr u32 MAX_DEPTH = 3;

INode *switchType(SchemaType type, u32 depth)
{
    switch (type)
    {
    case Number:
        return new NumberNode(depth);
    case Double:
        return new DoubleNode(depth);
    case String:
        return new StringNode(depth);
    default:
        throw std::runtime_error("Unsupported type in AlgNode");
    }
}

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
};

struct UncompressedNode : AlgNode
{
    SchemeAlgorythm alg = Uncompressed;

    bool Compress()
    {
        return false;
    }
};

struct BitpackingNode : AlgNode
{
    SchemeAlgorythm alg = Bitpacking;

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct DictionaryNode : AlgNode
{
    SchemeAlgorythm alg = Dictionary;
    INode *values_node;
    INode *codes_node;

    DictionaryNode(u8 depth, SchemaType type)
    {
        if (depth >= MAX_DEPTH)
        {
            values_node = nullptr;
            codes_node = nullptr;
            return;
        }

        codes_node = new NumberNode(depth + 1);
        values_node = switchType(type, depth + 1);
    }

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct FastPForNode : AlgNode
{
    SchemeAlgorythm alg = FastPFor;

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct OnevalNode : AlgNode
{
    SchemeAlgorythm alg = Oneval;

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct RleNode : AlgNode
{
    SchemeAlgorythm alg = Rle;
    INode *values_node;
    INode *counts_node;

    RleNode(u8 depth)
    {
        if (depth >= MAX_DEPTH)
        {
            values_node = nullptr;
            counts_node = nullptr;
            return;
        }
        values_node = new NumberNode(depth + 1);
        counts_node = new NumberNode(depth + 1);
    }

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct FsstNode : AlgNode
{
    SchemeAlgorythm alg = Fsst;

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct FrequencyNode : AlgNode
{
    SchemeAlgorythm alg = Frequency;
    INode *exceptions_node;

    FrequencyNode(u8 depth)
    {
        if (depth >= MAX_DEPTH)
        {
            exceptions_node = nullptr;
            return;
        }

        exceptions_node = new DoubleNode();
    }

    bool Compress()
    {
        // TODO
        return false;
    }
};

struct NumberNode : INode
{
    SchemaType type = Number;
    AlgNode *nodes[6];
    NumberStats<u32> stats; // TODO

    NumberNode(const u32 *src, const ValidityMask *nullmap, const u32 nitems, u8 depth = 0) : stats(src, nullmap, nitems)
    {
        nodes[0] = new UncompressedNode();
        nodes[1] = new BitpackingNode();
        nodes[2] = new DictionaryNode(depth + 1, type);
        nodes[3] = new FastPForNode();
        nodes[4] = new OnevalNode();
        nodes[5] = new RleNode(depth + 1);
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
        nodes[2] = new DictionaryNode(depth + 1, type);
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
        nodes[2] = new DictionaryNode(depth + 1, type);
        nodes[3] = new RleNode(depth + 1);
        nodes[4] = new FrequencyNode(depth + 1);
    };
};
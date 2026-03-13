#pragma once

#include "common.hpp"
#include "src_type.hpp"

// Forward declare all node types so IVisitor can reference them
struct IntegerNode;
struct DoubleNode;
struct StringNode;
struct UncompressedNode;
struct DictionaryNode;
struct RleNode;
struct BitpackNode;
struct FastPForNode;
struct ForNode;
struct FrequencyNode;

struct IVisitor
{
    virtual u32 Visit(IntegerNode &node) = 0;
    virtual u32 Visit(DoubleNode &node) = 0;
    virtual u32 Visit(StringNode &node) = 0;
    virtual u32 Visit(UncompressedNode &node) = 0;
    virtual u32 Visit(DictionaryNode &node) = 0;
    virtual u32 Visit(RleNode &node) = 0;
    virtual u32 Visit(BitpackNode &node) = 0;
    virtual u32 Visit(FastPForNode &node) = 0;
    virtual u32 Visit(ForNode &node) = 0;
    // virtual u32 Visit(FrequencyNode &node) = 0;
};

struct INode
{
    void *buf;
    u8 depth;

    virtual u32 Accept(IVisitor &visitor) = 0;
};

enum SchemaType
{
    Number,
    Double,
    String
};

enum SchemeAlgorythm
{
    Uncompressed,
    Dictionary,
    Rle,
    Bitpacking,
    FastPFor,
    Frequency,
    Fsst,
    Oneval
};

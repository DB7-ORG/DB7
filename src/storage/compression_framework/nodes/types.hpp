#pragma once

#include "common.hpp"
#include "src_type.hpp"

// Forward declare all node types so IVisitor can reference them
struct NumberNode;
struct UncompressedNode;
struct DictionaryNode;
struct RleNode;
struct Bitpacknode;

struct IVisitor
{
    virtual u32 Visit(NumberNode &node) = 0;
    virtual u32 Visit(UncompressedNode &node) = 0;
    virtual u32 Visit(DictionaryNode &node) = 0;
    virtual u32 Visit(RleNode &node) = 0;
    virtual u32 Visit(BitpackNode &node) = 0;
};

struct INode
{
    u8 depth;
    void *buf;
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

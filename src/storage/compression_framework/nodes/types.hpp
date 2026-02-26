#pragma once

#include "common.hpp"

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

struct IVisitor
{
    virtual u32 Visit(DictionaryNode2 &node) = 0;
    virtual u32 Visit(RleNode2 &node) = 0;
    virtual u32 Visit(NumberNode2 &node) = 0;
};

struct INode
{
    virtual u32 Accept(IVisitor &visitor) = 0;
};

struct NumberNode2 : INode
{
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct DictionaryNode2 : INode
{
    SchemeAlgorythm alg = Dictionary;
    INode *values_node;
    INode *codes_node;

    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct RleNode2 : INode
{
    SchemeAlgorythm alg = Rle;
    INode *values_node;
    INode *codes_node;

    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct EstimateCostVisitor : IVisitor
{
    u32 Visit(DictionaryNode2 &node) override { /* use node.stats */ }
    u32 Visit(RleNode2 &node) override { /* use node.stats */ }
    u32 Visit(NumberNode2 &node) override { /* use node.stats */ }
};
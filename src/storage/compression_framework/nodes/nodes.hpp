#pragma once

#include "common.hpp"
#include "types.hpp"

struct NumberNode : INode
{
    u8 best_node_idx;
    INode *children[4];

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

struct BitpackNode : INode
{
    BitpackNode(u8 depth);
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
        children[3] = nullptr;
        return;
    }

    best_node_idx = 0;
    children[0] = new UncompressedNode(depth);
    children[1] = new DictionaryNode(depth);
    children[2] = new RleNode(depth);
    children[3] = new BitpackNode(depth);
}

inline UncompressedNode::UncompressedNode(u8 depth)
{
    this->depth = depth;
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

inline BitpackNode::BitpackNode(u8 depth)
{
    this->depth = depth;
}
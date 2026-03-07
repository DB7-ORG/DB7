#pragma once

#include "common.hpp"
#include "types.hpp"
#include "slab_arena.hpp"

struct NumberNode : INode
{
    u8 best_node_idx;
    INode *children[4];

    NumberNode(SlabArena &arena, u8 depth = 0);
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

    DictionaryNode(SlabArena &arena, u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct RleNode : INode
{
    NumberNode *values_node;
    NumberNode *lens_node;

    RleNode(SlabArena &arena, u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct BitpackNode : INode
{
    BitpackNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

inline NumberNode::NumberNode(SlabArena &arena, u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
    {
        children[0] = arena.New<UncompressedNode>(depth);
        children[1] = nullptr;
        children[2] = nullptr;
        children[3] = nullptr;
        return;
    }

    best_node_idx = 0;
    children[0] = arena.New<UncompressedNode>(depth);
    children[1] = arena.New<DictionaryNode>(arena, depth);
    children[2] = arena.New<RleNode>(arena, depth);
    children[3] = arena.New<BitpackNode>(depth);
}

inline UncompressedNode::UncompressedNode(u8 depth)
{
    this->depth = depth;
}

inline DictionaryNode::DictionaryNode(SlabArena &arena, u8 depth)
{
    this->depth = depth;
    u8 newDepth = depth + 1;
    values_node = arena.New<NumberNode>(arena, newDepth);
    codes_node = arena.New<NumberNode>(arena, newDepth);
}

inline RleNode::RleNode(SlabArena &arena, u8 depth)
{
    this->depth = depth;
    u8 newDepth = depth + 1;
    values_node = arena.New<NumberNode>(arena, newDepth);
    lens_node = arena.New<NumberNode>(arena, newDepth);
}

inline BitpackNode::BitpackNode(u8 depth)
{
    this->depth = depth;
}
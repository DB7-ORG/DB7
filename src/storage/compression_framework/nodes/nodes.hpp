#pragma once

#include "common.hpp"
#include "types.hpp"
#include "slab_arena.hpp"

struct IntegerNode : INode
{
    u8 best_node_idx;
    INode *children[6];

    IntegerNode(SlabArena &arena, u8 depth = 0);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct DoubleNode : INode
{
    u8 best_node_idx;
    INode *children[3];

    DoubleNode(SlabArena &arena, u8 depth = 0);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct UncompressedNode : INode
{
    UncompressedNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct DictionaryNode : INode
{
    INode *values_node;
    IntegerNode *codes_node;

    DictionaryNode(SlabArena &arena, u8 depth, SrcType type = SrcType::U32);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct RleNode : INode
{
    INode *values_node;
    IntegerNode *lens_node;

    RleNode(SlabArena &arena, u8 depth, SrcType type = SrcType::U32);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct BitpackNode : INode
{
    BitpackNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct ForNode : INode
{
    IntegerNode *values_node;

    ForNode(SlabArena &arena, u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

struct FastPForNode : INode
{
    FastPForNode(u8 depth);
    u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
};

// struct FrequencyNode : INode
// {
//     DoubleNode *values_node;

//     FrequencyNode(SlabArena &arena, u8 depth);
//     u32 Accept(IVisitor &visitor) override { return visitor.Visit(*this); }
// };

inline IntegerNode::IntegerNode(SlabArena &arena, u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
    {
        children[0] = arena.New<UncompressedNode>(depth);
        children[1] = nullptr;
        children[2] = nullptr;
        children[3] = nullptr;
        children[4] = nullptr;
        children[5] = nullptr;
        return;
    }

    best_node_idx = 0;
    children[0] = arena.New<UncompressedNode>(depth);
    children[1] = arena.New<DictionaryNode>(arena, depth);
    children[2] = arena.New<RleNode>(arena, depth);
    children[3] = arena.New<BitpackNode>(depth);
    children[4] = arena.New<FastPForNode>(depth);
    children[5] = arena.New<ForNode>(arena, depth);
}

inline DoubleNode::DoubleNode(SlabArena &arena, u8 depth)
{
    this->depth = depth;

    if (depth >= MAX_COMPRESSION_DEPTH)
    {
        children[0] = arena.New<UncompressedNode>(depth);
        children[1] = nullptr;
        children[2] = nullptr;
        // children[3] = nullptr;
        //  children[4] = nullptr;
        //  children[5] = nullptr;
        return;
    }

    best_node_idx = 0;
    children[0] = arena.New<UncompressedNode>(depth);
    children[1] = arena.New<DictionaryNode>(arena, depth, SrcType::DBL);
    children[2] = arena.New<RleNode>(arena, depth, SrcType::DBL);
    // children[3] = arena.New<FrequencyNode>(arena, depth);
    //  children[4] = arena.New<FastPForNode>(depth);
    //  children[5] = arena.New<ForNode>(arena, depth);
}

inline UncompressedNode::UncompressedNode(u8 depth)
{
    this->depth = depth;
}

inline DictionaryNode::DictionaryNode(SlabArena &arena, u8 depth, SrcType type)
{
    this->depth = depth;
    u8 newDepth = depth + 1;

    if (type == SrcType::DBL)
    {
        values_node = arena.New<DoubleNode>(arena, newDepth);
    }
    else
    {
        values_node = arena.New<IntegerNode>(arena, newDepth);
    }

    codes_node = arena.New<IntegerNode>(arena, newDepth);
}

inline RleNode::RleNode(SlabArena &arena, u8 depth, SrcType type)
{
    this->depth = depth;
    u8 newDepth = depth + 1;

    if (type == SrcType::DBL)
    {
        values_node = arena.New<DoubleNode>(arena, newDepth);
    }
    else
    {
        values_node = arena.New<IntegerNode>(arena, newDepth);
    }

    lens_node = arena.New<IntegerNode>(arena, newDepth);
}

inline BitpackNode::BitpackNode(u8 depth)
{
    this->depth = depth;
}

inline FastPForNode::FastPForNode(u8 depth)
{
    this->depth = depth;
}

inline ForNode::ForNode(SlabArena &arena, u8 depth)
{
    this->depth = depth;
    u8 newDepth = depth + 1;
    values_node = arena.New<IntegerNode>(arena, newDepth);
}

// inline FrequencyNode::FrequencyNode(SlabArena &arena, u8 depth)
// {
//     this->depth = depth;
//     u8 newDepth = depth + 1;
//     values_node = arena.New<DoubleNode>(arena, newDepth);
// }
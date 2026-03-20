#pragma once

#include "common.hpp"
#include "src_type.hpp"

// Forward declare all node types so IVisitor can reference them
// struct IntegerNode;
// struct DoubleNode;
// struct StringNode;
// struct UncompressedNode;
// struct DictionaryNode;
// struct RleNode;
// struct BitpackNode;
// struct FastPForNode;
// struct ForNode;
// struct FrequencyNode;

// struct IVisitor
// {
//     virtual u32 Visit(IntegerNode &node) = 0;
//     virtual u32 Visit(DoubleNode &node) = 0;
//     virtual u32 Visit(StringNode &node) = 0;
//     virtual u32 Visit(UncompressedNode &node) = 0;
//     virtual u32 Visit(DictionaryNode &node) = 0;
//     virtual u32 Visit(RleNode &node) = 0;
//     virtual u32 Visit(BitpackNode &node) = 0;
//     virtual u32 Visit(FastPForNode &node) = 0;
//     virtual u32 Visit(ForNode &node) = 0;
//     // virtual u32 Visit(FrequencyNode &node) = 0;
// };

// struct INode
// {
//     void *buf;
//     u8 depth;

//     virtual u32 Accept(IVisitor &visitor) = 0;
// };

enum SchemaType
{
    Number,
    Double,
    String
};

enum SchemeAlgorithm : u8
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

inline void PrintScheme(SchemeAlgorithm *applied, u32 size)
{
    for (u32 i = 0; i < size; i++)
    {
        switch (applied[i])
        {
        case SchemeAlgorithm::Uncompressed:
            printf("[%u] Uncompressed\n", i);
            break;
        case SchemeAlgorithm::Dictionary:
            printf("[%u] Dictionary\n", i);
            break;
        case SchemeAlgorithm::Rle:
            printf("[%u] Rle\n", i);
            break;
        case SchemeAlgorithm::Bitpacking:
            printf("[%u] Bitpacking\n", i);
            break;
        case SchemeAlgorithm::FastPFor:
            printf("[%u] FastPFor\n", i);
            break;
        case SchemeAlgorithm::Frequency:
            printf("[%u] Frequency\n", i);
            break;
        case SchemeAlgorithm::Fsst:
            printf("[%u] Fsst\n", i);
            break;
        case SchemeAlgorithm::Oneval:
            printf("[%u] Oneval\n", i);
            break;
        }
    }
}

#pragma once

#include "types.hpp"

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
        codes_node = new NumberNode(nullptr, nullptr, 0, depth + 1);
        values_node = switchType(type, depth + 1);
    }

    void Next()
    {
        // TODO
    }
};
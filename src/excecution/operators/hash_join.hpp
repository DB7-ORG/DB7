#pragma once

#include "common.hpp"

#include "inode.hpp"

#include <vector>

constexpr std::string IS_BUILD_SIDE = "@IsBuildSide";

class HashJoin : INode
{
    INode *left;
    INode *right;
    std::string leftKey;
    std::string rightKey;
    bool inMem = true;

    void initMem(CodeGen &codegen, Context &context) const
    {
        return;
    }

    void produceLeft(CodeGen &codegen, Context &context) const
    {
        context.setState(IS_BUILD_SIDE, true);
        context.add(leftKey, nullptr);
        left->produce(codegen, context);
    }

    void produceRight(CodeGen &codegen, Context &context) const
    {
        context.setState(IS_BUILD_SIDE, false);
        context.add(rightKey, nullptr);
        right->produce(codegen, context);
    }

    void insertToHashMap(CodeGen &codegen, Context &context) const
    {
    }

public:
    void produce(CodeGen &codegen, Context &context) const
    {
        initMem(codegen, context); // create a htable

        produceLeft(codegen, context);

        produceRight(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        if (context.getState(IS_BUILD_SIDE))
        {
            // generate array

            // store all values from context.map

            // calc size
        }
    }
};
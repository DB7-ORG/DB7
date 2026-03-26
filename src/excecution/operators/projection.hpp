#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Projection : INode
{
    INode *input;
    std::unordered_set<std::string> attributes;

    Projection(INode *input, std::unordered_set<std::string> attributes)
        : input(input), attributes(std::move(attributes))
    {
        input->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        AddRequired required(context, attributes);
        input->produce(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        parent->consume(codegen, context);
    }
};

#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Projection : INode
{
    INode *next;
    std::unordered_set<std::string> attributes;

    Projection(INode *next, std::unordered_set<std::string> attributes)
        : next(next), attributes(std::move(attributes))
    {
        next->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        AddRequired required(context, attributes);
        next->produce(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        parent->consume(codegen, context);
    }
};

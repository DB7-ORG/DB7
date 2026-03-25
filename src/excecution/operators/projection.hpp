#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Projection : INode
{
    INode *next;
    std::vector<std::string> attributes;

    Projection(INode *next, std::vector<std::string> attributes)
        : next(next), attributes(std::move(attributes))
    {
        next->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        for (auto &attr : attributes)
            context.add(attr, nullptr);
        next->produce(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        parent->consume(codegen, context);
    }
};

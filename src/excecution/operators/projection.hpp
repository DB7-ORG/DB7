#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Projection : INode
{
    INode *next;
    void *attributes;

    Projection(INode *next, void *attributes)
        : next(next), attributes(attributes)
    {
        next->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        // TODO add which tuples scan needs
        next->produce(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        parent->consume(codegen, context);
    }
};

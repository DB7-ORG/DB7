#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Materialize : INode
{
    INode *next;

    Materialize(INode *next)
        : next(next)
    {
        next->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        next->produce(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        llvm::Value *val = context.get("tid");
        codegen.callPrintf(val);
    }
};

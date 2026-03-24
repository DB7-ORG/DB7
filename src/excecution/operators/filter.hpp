#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Filter : INode
{
    INode *next;
    void *expression;
    void *attributes;

    Filter(INode *next, void *expression, void *attributes)
        : next(next), expression(expression), attributes(attributes)
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
        // auto filterCondition = codegen.deriveExpression(expression, context);
        // TODO this should codegen a real expression
        // TODO figure out naming also
        llvm::Value *filter = codegen->CreateICmpULT(context.get("tid"), codegen.const64(5));

        {
            If check(codegen, filter);

            parent->consume(codegen, context);
        }
    }
};

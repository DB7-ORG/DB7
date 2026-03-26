#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Filter : INode
{
    INode *next;
    void *expression;
    std::unordered_set<std::string> attributes;

    Filter(INode *next, void *expression, std::unordered_set<std::string> attributes)
        : next(next), expression(expression), attributes(std::move(attributes))
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

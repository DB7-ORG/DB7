#pragma once

#include "common.hpp"
#include "../llvm.hpp"
#include "inode.hpp"

struct Filter : INode
{
    INode *input;
    void *expression;
    std::unordered_set<std::string> attributes;

    Filter(INode *input, void *expression, std::unordered_set<std::string> attributes)
        : input(input), expression(expression), attributes(std::move(attributes))
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

#pragma once

#include "../llvm.hpp"
#include "inode.hpp"

struct Mapping
{
    std::string resultName;
    void *expression;
};

struct Map : INode
{
    INode *input;
    std::unordered_set<std::string> attributes;
    std::vector<Mapping> mappings;

    Map(INode *input, std::vector<Mapping> mappings)
        : input(input), mappings(std::move(mappings))
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
#pragma once

#include "common.hpp"

#include "inode.hpp"
#include "../helpers/hahs_join_proxy.hpp"
#include "materialize_helper.hpp"

#include <vector>

struct HashJoin : INode
{
    INode *left;
    INode *right;
    std::string leftKey;
    std::string rightKey;
    MatHelper helper;

    HashJoin(
        INode *left,
        INode *right,
        std::string leftKey,
        std::string rightKey)
        : left(left), right(right), leftKey(leftKey), rightKey(rightKey), helper()
    {
        left->parent = this;
        // right->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        JoinState state;
        state.isBuild = true;
        state.inMem = true;
        context.setJoinState(this, state);

        left->produce(codegen, context);
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        JoinState *state = context.getJoinState(this);

        auto *proxy = new HashJoinProxy();
        llvm::Value *proxyPtr = codegen.ptrConst(proxy);
        context.test = proxy->ptr; // TODO test

        if (state->isBuild)
        {
            state->isBuild = false;

            auto values = helper.collectValues(codegen, context);

            llvm::Value *joinKey = context.get(leftKey);
            llvm::Value *hash = helper.calcHash(codegen, joinKey);

            llvm::Value *size = helper.calcSize(codegen, values);

            llvm::Value *ptr = codegen.callBase(HashJoinProxy::allocTupleJIT, {proxyPtr, size});
            helper.materialize(codegen, values, hash, ptr);
        }
        else
        {
        }

        parent->consume(codegen, context);
    }
};
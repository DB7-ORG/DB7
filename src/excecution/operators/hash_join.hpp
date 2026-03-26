#pragma once

#include "common.hpp"

#include "inode.hpp"
#include "../helpers/hahs_join_proxy.hpp"
#include "materialize_helper.hpp"

struct HashJoin : INode
{
    INode *left;
    INode *right;
    std::unordered_set<std::string> leftKeys;
    std::unordered_set<std::string> rightKeys;
    MatHelper helper;

    HashJoin(
        INode *left,
        INode *right,
        std::unordered_set<std::string> leftKeys,
        std::unordered_set<std::string> rightKeys)
        : left(left), right(right), leftKeys(std::move(leftKeys)), rightKeys(std::move(rightKeys)), helper()
    {
        left->parent = this;
        // right->parent = this;
    }

    void produce(CodeGen &codegen, Context &context) const
    {
        {
            AddRequired required(context, leftKeys);

            JoinState state;
            state.isBuild = true;
            state.inMem = true;
            context.setJoinState(this, state);

            left->produce(codegen, context);
        }
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

            auto values = helper.sortValues(codegen, context);

            std::vector<llvm::Value *> leftVals = helper.collectValues(context, leftKeys);

            llvm::Value *hash = helper.calcHash(codegen, leftVals);

            llvm::Value *size = helper.calcSize(codegen, values);

            llvm::Value *ptr = codegen.callBase(HashJoinProxy::allocTupleJIT, {proxyPtr, size});
            helper.materialize(codegen, values, hash, ptr);
        }
        else
        {
            std::vector<llvm::Value *> rightVals = helper.collectValues(context, rightKeys);

            llvm::Value *hash = helper.calcHash(codegen, rightVals);
            (void)hash;

            // TODO probe the hash table for matches
        }
        parent->consume(codegen, context);
    }
};
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
        auto *proxy = new HashJoinProxy(context.jit);
        JoinState state;
        state.isBuild = true;
        state.inMem = true;
        // state.keep = helper.copyRequiredAttributes(context);
        state.proxyPtr = codegen.ptrConst(proxy);

        context.test = proxy->ptr; // TODO test

        context.setJoinState(this, state);
        {
            AddRequired required(context, leftKeys);
            llvm::Function *buildFn = codegen.createFunction("build");
            left->produce(codegen, context);
            codegen->CreateRetVoid();
            proxy->produceLeftName = buildFn->getName().str();
        }
        auto st = context.getJoinState(this);
        st->isBuild = false;
        {
            AddRequired required(context, rightKeys);
            llvm::Function *buildFn = codegen.createFunction("probe");
            right->produce(codegen, context);
            codegen->CreateRetVoid();
            proxy->produceLeftName = buildFn->getName().str();
        }

        codegen.callBase(HashJoinProxy::gather, {state.proxyPtr});
    }

    void consume(CodeGen &codegen, Context &context) const
    {
        JoinState *state = context.getJoinState(this);
        auto proxyPtr = state->proxyPtr;

        if (state->isBuild)
        {
            state->isBuild = false;

            std::vector<llvm::Value *> leftVals = helper.collectValues(context, leftKeys);

            llvm::Value *hash = helper.calcHash(codegen, leftVals);

            // helper.filterExtraAttributes(context, state->keep);

            auto values = helper.sortValues(codegen, context);
            state->valuesLeft = &values;

            llvm::Value *size = helper.calcSize(codegen, values);

            llvm::Value *ptr = codegen.callBase(HashJoinProxy::getLeftSlotToInsert, {proxyPtr, size});
            helper.materialize(codegen, values, hash, ptr);
        }
        else
        {
            std::vector<llvm::Value *> rightVals = helper.collectValues(context, rightKeys);

            llvm::Value *hash = helper.calcHash(codegen, rightVals);

            // helper.filterExtraAttributes(context, state->keep);

            auto values = helper.sortValues(codegen, context);
            state->valuesRight = &values;

            llvm::Value *size = helper.calcSize(codegen, values);

            llvm::Value *ptr = codegen.callBase(HashJoinProxy::getRightSlotToInsert, {proxyPtr, size});
            helper.materialize(codegen, values, hash, ptr);

            codegen.callBase(HashJoinProxy::probe, {proxyPtr});
        }
        // parent->consume(codegen, context);
    }

    void join(CodeGen &codegen, Context &context) const
    {
        // nested loop join for now
    }
};
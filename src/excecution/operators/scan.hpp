#pragma once

#include "common.hpp"

#include "inode.hpp"

#include <vector>

struct Scan : INode
{
    void produce(CodeGen &codegen, Context &context) const
    {
        llvm::Value *strPtr = codegen->CreateGlobalStringPtr("hello world");
        llvm::Value *strLen = codegen.const32(11);

        // Pack into {ptr, len} struct
        llvm::Type *strType = llvm::StructType::get(codegen.getContext(), {llvm::PointerType::getUnqual(codegen.getContext()),
                                                                           llvm::Type::getInt32Ty(codegen.getContext())});

        llvm::Value *strVal = llvm::UndefValue::get(strType);
        strVal = codegen->CreateInsertValue(strVal, strPtr, {0});
        strVal = codegen->CreateInsertValue(strVal, strLen, {1});

        llvm::Value *tid = codegen.const64(0);
        llvm::Value *limit = codegen.const64(10);

        auto condition = [&](auto &phis)
        {
            return codegen->CreateICmpULT(phis[0], limit);
        };

        {
            Loop loop(codegen, condition, {{tid, "tid"}});

            tid = loop.getLoopVar(0);
            context.add("tid", tid);
            context.add("name", strVal);

            parent->consume(codegen, context);

            tid = codegen->CreateAdd(tid, codegen.const64(1));
            loop.endLoop({tid});
        }
    }

    void consume(CodeGen &, Context &) const
    {
        __builtin_unreachable();
    }
};
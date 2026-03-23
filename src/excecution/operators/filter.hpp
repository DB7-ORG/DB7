#pragma once;

#include "../llvm.hpp"

#include <iostream>
#include <memory>

using namespace llvm;

std::unique_ptr<Module> buildScan(LLVMContext &ctx)
{
    auto mod = std::make_unique<Module>("phase1_module", ctx);
    // u32 *output;
    // for (u32 i = 0; i < num; i++)
    // {
    //     if (input[i] > 20)
    //     {
    //         *(output++) = input[i];
    //     }
    // }

    Type *i32 = Type::getInt32Ty(ctx);
    Type *ptr = PointerType::getUnqual(ctx);

    FunctionType *funcTy = FunctionType::get(
        Type::getVoidTy(ctx),
        {
            ptr,
            ptr,
            i32,
        },
        false);

    Function *func = Function::Create(
        funcTy,
        Function::ExternalLinkage,
        "filter",
        mod.get());

    llvm::Function::arg_iterator args = func->arg_begin();
    llvm::Value *output = args++;
    output->setName("output");
    llvm::Value *input = args++;
    input->setName("input");
    llvm::Value *num = args++;
    input->setName("num");

    BasicBlock *entry = BasicBlock::Create(ctx, "entry", func);
    llvm::BasicBlock *loopHeader = llvm::BasicBlock::Create(ctx, "loop_header", func);
    llvm::BasicBlock *loopBody = llvm::BasicBlock::Create(ctx, "loop_body", func);
    llvm::BasicBlock *filterPass = llvm::BasicBlock::Create(ctx, "filter_pass", func);
    llvm::BasicBlock *loopInc = llvm::BasicBlock::Create(ctx, "loop_inc", func);
    llvm::BasicBlock *loopExit = llvm::BasicBlock::Create(ctx, "loop_exit", func);

    llvm::IRBuilder<> B(ctx);

    B.SetInsertPoint(entry);
    B.CreateBr(loopHeader);

    B.SetInsertPoint(loopHeader);
    llvm::PHINode *iPhi = B.CreatePHI(i32, 2, "i");
    iPhi->addIncoming(llvm::ConstantInt::get(i32, 0), entry);
    llvm::PHINode *outIdxPhi = B.CreatePHI(i32, 2, "out_idx");
    outIdxPhi->addIncoming(llvm::ConstantInt::get(i32, 0), entry);
    llvm::Value *cond = B.CreateICmpULT(iPhi, num, "cond");
    B.CreateCondBr(cond, loopBody, loopExit);

    B.SetInsertPoint(loopBody);
    llvm::Value *inPtr = B.CreateGEP(i32, input, iPhi, "in_ptr");
    llvm::Value *val = B.CreateLoad(i32, inPtr, "val");
    llvm::Value *filterCond = B.CreateICmpUGT(val, llvm::ConstantInt::get(i32, 20), "filter");
    B.CreateCondBr(filterCond, filterPass, loopInc);

    B.SetInsertPoint(filterPass);
    llvm::Value *outPtr = B.CreateGEP(i32, output, outIdxPhi, "out_ptr");
    B.CreateStore(val, outPtr);
    llvm::Value *outIdxNext = B.CreateAdd(outIdxPhi, llvm::ConstantInt::get(i32, 1), "out_idx_next");
    B.CreateBr(loopInc);

    B.SetInsertPoint(loopInc);
    llvm::PHINode *outIdxMerge = B.CreatePHI(i32, 2, "out_idx_merge");
    outIdxMerge->addIncoming(outIdxPhi, loopBody);
    outIdxMerge->addIncoming(outIdxNext, filterPass);
    llvm::Value *iNext = B.CreateAdd(iPhi, llvm::ConstantInt::get(i32, 1), "i_next");
    B.CreateBr(loopHeader);
    iPhi->addIncoming(iNext, loopInc);
    outIdxPhi->addIncoming(outIdxMerge, loopInc);

    B.SetInsertPoint(loopExit);
    B.CreateRet(outIdxPhi);

    return mod;
}
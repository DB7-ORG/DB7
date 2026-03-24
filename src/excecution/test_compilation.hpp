#pragma once

#include "common.hpp"

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <memory>

u32 add(u32 a, u32 b);

template <typename T>
T exitOnError(llvm::Expected<T> val, const char *msg)
{
    if (!val)
    {
        llvm::errs() << msg << ": " << toString(val.takeError()) << "\n";
        std::exit(1);
    }
    return std::move(*val);
}

inline std::unique_ptr<llvm::Module> buildModule(llvm::LLVMContext &ctx)
{
    auto mod = std::make_unique<llvm::Module>("phase1_module", ctx);

    // Create the function signature: (i64, i64) -> i64
    llvm::FunctionType *funcTy = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(ctx),                                // return type
        {llvm::Type::getInt64Ty(ctx), llvm::Type::getInt64Ty(ctx)}, // param types
        false                                                       // not variadic
    );

    // Create the function in the module
    llvm::Function *addFunc = llvm::Function::Create(
        funcTy,
        llvm::Function::ExternalLinkage, // visible to JIT lookup
        "add",                           // symbol name
        mod.get());

    // Name the parameters (optional but great for reading IR dumps)
    llvm::Function::arg_iterator args = addFunc->arg_begin();
    llvm::Value *a = args++;
    a->setName("a");
    llvm::Value *b = args++;
    b->setName("b");

    // Create the entry basic block and an IRBuilder positioned in it
    llvm::BasicBlock *entry = llvm::BasicBlock::Create(ctx, "entry", addFunc);
    llvm::IRBuilder<> builder(entry);

    // Emit: %result = add i64 %a, %b
    llvm::Value *result = builder.CreateAdd(a, b, "result");

    // Emit: ret i64 %result
    builder.CreateRet(result);

    return mod;
}
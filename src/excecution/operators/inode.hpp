#pragma once

#include "common.hpp"
#include "../llvm.hpp"

#include <map>

class CodeGen
{
    std::unique_ptr<llvm::LLVMContext> llvm_context;
    std::unique_ptr<llvm::Module> module;
    std::unique_ptr<llvm::IRBuilder<>> builder;

public:
    CodeGen()
        : llvm_context(std::make_unique<llvm::LLVMContext>()),
          module(std::make_unique<llvm::Module>("module", *llvm_context)),
          builder(std::make_unique<llvm::IRBuilder<>>(*llvm_context))
    {
    }

    llvm::Function *createFunction(const std::string &name)
    {
        llvm::FunctionType *fnType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*llvm_context), false);

        llvm::Function *fn = llvm::Function::Create(
            fnType, llvm::Function::ExternalLinkage, name, module.get());

        llvm::BasicBlock *entry = llvm::BasicBlock::Create(
            *llvm_context, "entry", fn);

        builder->SetInsertPoint(entry);

        return fn;
    }

    llvm::IRBuilder<> *operator->() { return builder.get(); }
    llvm::LLVMContext &getContext() { return *llvm_context; }
    llvm::Module &getModule() { return *module; }
    std::unique_ptr<llvm::LLVMContext> takeContext() { return std::move(llvm_context); }
    std::unique_ptr<llvm::Module> takeModule() { return std::move(module); }

    llvm::Value *const64(u64 val)
    {
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(getContext()), val);
    }

    // TODO test
    void callPrintf(llvm::Value *val)
    {
        auto *module = builder->GetInsertBlock()->getModule();

        // Declare printf if not already declared
        auto *printfFunc = module->getFunction("printf");
        if (!printfFunc)
        {
            auto *printfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(getContext()),
                {llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(getContext()))},
                true // variadic
            );
            printfFunc = llvm::Function::Create(
                printfType, llvm::Function::ExternalLinkage, "printf", module);
        }

        // Create format string
        auto *formatStr = builder->CreateGlobalStringPtr("%ld\n");

        // Call printf
        builder->CreateCall(printfFunc, {formatStr, val});
    }
};

class Context
{
    std::unordered_map<std::string, llvm::Value *> attributes;

public:
    Context() = default;
    void add(const std::string &name, llvm::Value *val) { attributes[name] = val; }
    llvm::Value *get(const std::string &name) { return attributes.at(name); }
};

struct INode
{
    INode *parent = nullptr;
    virtual void produce(CodeGen &codegen, Context &context) const = 0;
    virtual void consume(CodeGen &codegen, Context &context) const = 0;
};

class Loop
{
    CodeGen &codegen;
    llvm::BasicBlock *header;
    llvm::BasicBlock *body;
    llvm::BasicBlock *exit;
    std::vector<llvm::PHINode *> phis;

public:
    Loop(CodeGen &cg,
         std::function<llvm::Value *(std::vector<llvm::PHINode *> &)> make_condition,
         std::vector<std::pair<llvm::Value *, std::string>> initial_values)
        : codegen(cg)
    {
        auto *func = codegen->GetInsertBlock()->getParent();
        header = llvm::BasicBlock::Create(codegen.getContext(), "loop_header", func);
        body = llvm::BasicBlock::Create(codegen.getContext(), "loop_body", func);
        exit = llvm::BasicBlock::Create(codegen.getContext(), "loop_exit", func);

        auto *entry_block = codegen->GetInsertBlock();

        codegen->CreateBr(header);

        codegen->SetInsertPoint(header);

        for (auto &[init_val, name] : initial_values)
        {
            auto *phi = codegen->CreatePHI(init_val->getType(), 2, name);
            phi->addIncoming(init_val, entry_block);
            phis.push_back(phi);
        }

        auto *cond = make_condition(phis);
        codegen->CreateCondBr(cond, body, exit);

        codegen->SetInsertPoint(body);
    }

    llvm::PHINode *getLoopVar(unsigned idx) { return phis[idx]; }

    void endLoop(std::vector<llvm::Value *> updated_values)
    {
        for (unsigned i = 0; i < phis.size(); i++)
        {
            phis[i]->addIncoming(updated_values[i], codegen->GetInsertBlock());
        }
    }

    ~Loop()
    {
        codegen->CreateBr(header);
        codegen->SetInsertPoint(exit);
    }
};

class If
{
    CodeGen &codegen;
    llvm::BasicBlock *then_bb;
    llvm::BasicBlock *cont_bb;

public:
    If(CodeGen &cg, llvm::Value *cond)
        : codegen(cg)
    {
        auto *func = codegen->GetInsertBlock()->getParent();
        then_bb = llvm::BasicBlock::Create(codegen.getContext(), "if_then", func);
        cont_bb = llvm::BasicBlock::Create(codegen.getContext(), "if_cont", func);

        codegen->CreateCondBr(cond, then_bb, cont_bb);
        codegen->SetInsertPoint(then_bb);
    }

    ~If()
    {
        codegen->CreateBr(cont_bb);
        codegen->SetInsertPoint(cont_bb);
    }
};
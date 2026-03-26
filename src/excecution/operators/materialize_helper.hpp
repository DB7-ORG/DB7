#pragma once

#include "../llvm.hpp"
#include "inode.hpp"
#include "../helpers/var_type.hpp"

#include <nmmintrin.h>

struct MatHelper
{

    std::vector<llvm::Value *> collectValues(Context &context, const std::unordered_set<std::string> &values) const
    {
        std::vector<llvm::Value *> collected;
        for (auto &name : values)
        {
            auto val = context.get(name);
            collected.push_back(val);
        }
        return collected;
    }

    std::vector<llvm::Value *> sortValues(CodeGen &codegen, Context &context) const
    {
        std::vector<llvm::Value *> fixed;
        std::vector<llvm::Value *> strings;

        for (auto &[key, val] : context.attributes)
        {
            if (val->getType()->isStructTy())
                strings.push_back(val);
            else
                fixed.push_back(val);
        }

        std::sort(fixed.begin(), fixed.end(),
                  [&](auto &a, auto &b)
                  {
                      auto sizeA = codegen.getModule().getDataLayout().getTypeAllocSize(a->getType());
                      auto sizeB = codegen.getModule().getDataLayout().getTypeAllocSize(b->getType());
                      return sizeA > sizeB;
                  });

        fixed.insert(fixed.end(), strings.begin(), strings.end());
        return fixed;
    }

    llvm::Value *calcSize(CodeGen &codegen, std::vector<llvm::Value *> &values) const
    {
        llvm::Value *size = codegen.const32(8);

        for (auto val : values)
        {
            if (val->getType()->isStructTy()) // string {ptr, len}
            {
                llvm::Value *strLen = codegen->CreateExtractValue(val, {1});
                u32 lenSize = codegen.getModule().getDataLayout().getTypeAllocSize(
                    strLen->getType());

                // len field + string bytes
                size = codegen->CreateAdd(size, codegen.const32(lenSize));
                size = codegen->CreateAdd(size, strLen);
            }
            else
            {
                u32 typeSize = codegen.getModule().getDataLayout().getTypeAllocSize(val->getType());
                size = codegen->CreateAdd(size, codegen.const32(typeSize));
            }
        }

        return size;
    }

    llvm::Value *calcHash(CodeGen &codegen, std::vector<llvm::Value *> &joinKeys) const
    {
        llvm::Function *crc = llvm::Intrinsic::getDeclaration(
            &codegen.getModule(),
            llvm::Intrinsic::x86_sse42_crc32_64_64);

        llvm::Value *hash = codegen.const64(0);
        for (auto joinKey : joinKeys)
        {
            if (joinKey->getType()->isStructTy()) // hash first 8 bytes of a string (treat as i64)
            {
                llvm::Value *strPtr = codegen->CreateExtractValue(joinKey, {0});

                llvm::Value *typedPtr = codegen->CreateBitCast(
                    strPtr, llvm::PointerType::getUnqual(llvm::Type::getInt64Ty(codegen.getContext())));
                llvm::Value *first8 = codegen->CreateLoad(
                    llvm::Type::getInt64Ty(codegen.getContext()), typedPtr);

                hash = codegen->CreateCall(crc, {hash, first8});
            }
            else
            {
                llvm::Value *asInt;
                if (joinKey->getType()->isIntegerTy(64))
                    asInt = joinKey;
                else if (joinKey->getType()->isIntegerTy())
                    asInt = codegen->CreateZExt(joinKey, llvm::Type::getInt64Ty(codegen.getContext()));
                else
                    asInt = codegen->CreateBitCast(joinKey, llvm::Type::getInt64Ty(codegen.getContext()));

                hash = codegen->CreateCall(crc, {hash, asInt});
            }
        }

        return hash;
    }

    void materialize(CodeGen &codegen, std::vector<llvm::Value *> &values, llvm::Value *hash, llvm::Value *ptr) const
    {
        llvm::Value *hashDest = codegen->CreateBitCast(
            ptr, llvm::PointerType::getUnqual(llvm::Type::getInt64Ty(codegen.getContext())));
        codegen->CreateStore(hash, hashDest);

        llvm::Value *offset = codegen.const32(8);

        for (auto val : values)
        {
            llvm::Value *dest = codegen->CreateGEP(
                llvm::Type::getInt8Ty(codegen.getContext()), ptr, offset);

            if (val->getType()->isStructTy()) // string {ptr, len}
            {
                llvm::Value *strPtr = codegen->CreateExtractValue(val, {0});
                llvm::Value *strLen = codegen->CreateExtractValue(val, {1});

                llvm::Value *lenDest = codegen->CreateBitCast(
                    dest, llvm::PointerType::getUnqual(strLen->getType()));
                codegen->CreateStore(strLen, lenDest);

                u32 lenSize = codegen.getModule().getDataLayout().getTypeAllocSize(strLen->getType());

                // Copy string bytes after len
                llvm::Value *bytesDest = codegen->CreateGEP(
                    llvm::Type::getInt8Ty(codegen.getContext()), dest, codegen.const32(lenSize));
                codegen->CreateMemCpy(bytesDest, llvm::MaybeAlign(1), strPtr, llvm::MaybeAlign(1), strLen);

                offset = codegen->CreateAdd(offset, codegen.const32(lenSize));
                offset = codegen->CreateAdd(offset, strLen);
            }
            else
            {
                llvm::Value *typedPtr = codegen->CreateBitCast(
                    dest, llvm::PointerType::getUnqual(val->getType()));
                codegen->CreateStore(val, typedPtr);
                u32 typeSize = codegen.getModule().getDataLayout().getTypeAllocSize(val->getType());
                offset = codegen->CreateAdd(offset, codegen.const32(typeSize));
            }
        }
    }
};
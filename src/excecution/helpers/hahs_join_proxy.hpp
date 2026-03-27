#pragma once

#include "../llvm.hpp"
#include "slab_arena.hpp"
#include "../hashtable.hpp"

using PipelineFn = void (*)(HashJoinProxy *);

struct HashJoinProxy
{
    SlabArena arena;
    llvm::orc::LLJIT *jit;
    u64 *ptr;
    u64 *rightPtr;
    std::string produceLeftName;
    std::string produceRightName;
    db7::HashTable<u32, u8 *> *table; // should just store pointers

    HashJoinProxy(llvm::orc::LLJIT *jit)
        : arena(1'000'000), jit(jit)
    {
        ptr = arena.Alloc<u64>(0);
    }

    static void printTuples(u8 *base)
    {
        u32 count = 5;
        u32 tupleSize = 8 + 8 + 4 + 11; // hash(u64) + tid(u64) + strlen(u32) + bytes(11)
        for (u32 i = 0; i < count; i++)
        {
            u8 *ptr = base + i * tupleSize;

            u64 hash = *(uint64_t *)(ptr);
            uint64_t tid = *(uint64_t *)(ptr + 8);
            uint32_t len = *(uint32_t *)(ptr + 16);
            char *str = (char *)(ptr + 20);

            printf("hash: %lu, tid: %lu, str: %.*s\n", hash, tid, len, str);
        }
        std::cout << std::endl;
    }

    void *allocTuple(u32 size)
    {
        return arena.Alloc<u8>(size);
    }

    template <typename T>
    static T exitOnError(llvm::Expected<T> val, const char *msg)
    {
        if (!val)
        {
            llvm::errs() << msg << ": " << toString(val.takeError()) << "\n";
            std::exit(1);
        }
        return std::move(*val);
    }

    static void *getLeftSlotToInsert(HashJoinProxy *self, u32 size)
    {
        return self->allocTuple(size);
    }

    static void *getRightSlotToInsert(HashJoinProxy *self, u32 size)
    {
        return self->allocTuple(size);
    }

    static void *gather(HashJoinProxy *self)
    {
        auto lsym = exitOnError(self->jit->lookup(self->produceLeftName), "left not found");
        using FunctionType = void (*)();
        auto leftFn = lsym.toPtr<FunctionType>();
        leftFn();

        self->rightPtr = (u64 *)self->allocTuple(0);
        // build hash table

        auto rsym = exitOnError(self->jit->lookup(self->produceRightName), "right not found");
        using FunctionType = void (*)();
        auto rightFn = rsym.toPtr<FunctionType>();
        rightFn();
    }

    static void *probe(HashJoinProxy *self)
    {
        }
};
#pragma once

#include "../llvm.hpp"
#include "slab_arena.hpp"

struct HashJoinProxy
{
    SlabArena arena;
    u64 *ptr;

    HashJoinProxy() : arena(100'000)
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

    static void *allocTupleJIT(HashJoinProxy *self, u32 size)
    {
        return self->allocTuple(size);
    }
};
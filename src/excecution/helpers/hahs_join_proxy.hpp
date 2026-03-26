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

    static void printTuples(u64 *ptr)
    {
        u32 size = 5 * 2;
        for (u32 i = 0; i < size; i++)
        {
            std::cout << ptr[i] << " ";
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
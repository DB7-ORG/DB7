#pragma once

#include "common.hpp"
#include "align_utils.hpp"

struct ArenaBlock
{
    u8 *base;
    u8 *ptr;
    u32 size;
    ArenaBlock *next;

    static ArenaBlock *AllocBlock(u32 size)
    {
        u8 *mem = (u8 *)malloc(sizeof(ArenaBlock) + size);
        ArenaBlock *block = (ArenaBlock *)mem;
        block->base = mem + sizeof(ArenaBlock);
        block->ptr = block->base;
        block->size = size;
        block->next = nullptr;
        return block;
    }
};

struct SlabArena
{
private:
    ArenaBlock *current;

public:
    SlabArena(u32 size)
    {
        current = ArenaBlock::AllocBlock(size);
    }

    ~SlabArena()
    {
        Reset();
        free(current);
    }

    template <typename T>
    T *Alloc(u32 count)
    {
        u32 needed = sizeof(T) * count;
        u8 *aligned = reinterpret_cast<u8 *>(AlignUp<T>(current->ptr));

        if (aligned + needed > current->base + current->size)
        {
            u32 newSize = std::max(current->size, (u32)(needed + alignof(T)));
            std::cout << "new block " << newSize << std::endl;
            ArenaBlock *block = ArenaBlock::AllocBlock(newSize);
            block->next = current;
            current = block;

            aligned = reinterpret_cast<u8 *>(AlignUp<T>(current->ptr));
        }

        T *result = reinterpret_cast<T *>(aligned);
        current->ptr = aligned + needed;
        return result;
    }

    void Reset()
    {
        // Free all blocks except first
        while (current->next)
        {
            ArenaBlock *next = current->next;
            free(current);
            current = next;
        }
        current->ptr = current->base;
    }

    template <typename T, typename... Args>
    T *New(Args &&...args)
    {
        T *mem = Alloc<T>(1);
        return new (mem) T(std::forward<Args>(args)...);
    }
};

static constexpr u32 ArenaSize(u32 nitems)
{
    constexpr u32 MAX_ELEMENT_SIZE = 8;  // sizeof(f64/i64)
    constexpr u32 MAX_ALIGN_PADDING = 8; // alignof(f64) - 1, rounded up
    return nitems * (MAX_ELEMENT_SIZE * MAX_COMPRESSION_DEPTH + MAX_ALIGN_PADDING);
}

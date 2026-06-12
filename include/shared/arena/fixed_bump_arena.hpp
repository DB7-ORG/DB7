#pragma once

#include "common.hpp"
#include "shared/macro_helper.hpp"
#include "shared/align_util.hpp"

namespace db7::shared
{

    class FixedBumpArena
    {
    private:
        static constexpr u32 ALLOCATOR_BLOCK_SIZE = 4096;
        byte data_[ALLOCATOR_BLOCK_SIZE];
        u32 size_;

    public:
        FixedBumpArena() : size_(0) {}

        void Reset() { size_ = 0; }

        byte *Allocate(u32 size)
        {
            DB7_ASSERT(HasAvailableSpace(size), "No space available");
            byte *res = data_ + size_;
            size_ += AlignUp(size, u32(8));
            return res;
        }

        bool HasAvailableSpace(u32 size)
        {
            return size_ + AlignUp(size, u32(8)) <= ALLOCATOR_BLOCK_SIZE;
        }
    };
}
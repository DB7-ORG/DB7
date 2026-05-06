#pragma once

#include "common.hpp"

#include <algorithm>
#include <span>

namespace db7::storage
{
    struct VarlenRef
    {
        page_id pid;
        u32 offset;
    };

    constexpr size_t INLINE_SIZE_CAP = sizeof(VarlenRef) + sizeof(u32);

    struct VarlenEntry
    {
    private:
        u32 size_;
        u32 prefix_;
        union
        {
            VarlenRef ref_content_;
            char inline_content_[sizeof(VarlenRef)];
        };

    public:
        VarlenEntry() {}

        u32 GetSize() { return size_; }
        u32 GetPrefix() { return prefix_; }
        VarlenRef GetRef() { return ref_content_; }
        const char *GetInline() { return reinterpret_cast<const char *>(&prefix_); }
        bool IsInline() const { return size_ <= INLINE_SIZE_CAP; }

        void Set(const std::span<byte> data)
        {
            size_ = data.size();
            std::memcpy(&prefix_, data.data(), std::min(data.size(), INLINE_SIZE_CAP));
        }

        void Set(const std::span<byte> data, page_id pid, u32 offset)
        {
            size_ = data.size();
            std::memcpy(&prefix_, data.data(), data.size() < sizeof(prefix_) ? data.size() : sizeof(prefix_));
            ref_content_ = {pid, offset};
        }
    };

    static_assert(sizeof(VarlenEntry) == 16);
    static_assert(INLINE_SIZE_CAP == 12);
}
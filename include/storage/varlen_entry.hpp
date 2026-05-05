#pragma once

#include "common.hpp"

namespace db7::storage
{
    struct VarlenData
    {
        page_id pid;
        u32 offset;
    };

    struct VarlenEntry
    {
    private:
        u32 size_;
        u32 prefix_;
        union
        {
            VarlenData content_;
            char inline_content_[sizeof(VarlenData)];
        };

    public:
        VarlenEntry(u32 size, u32 prefix, VarlenData data)
            : size_(size), prefix_(prefix), content_(data) {}

        VarlenEntry(u32 size, u32 prefix, char *data)
            : size_(size), prefix_(prefix)
        {
            std::memcpy(inline_content_, data, sizeof(inline_content_));
        }

        u32 GetSize() { return size_; }
        u32 GetPrefix() { return prefix_; }
        VarlenData GetRef() { return content_; }
        char *GetInline() { return inline_content_; }
    };

    static_assert(sizeof(VarlenEntry) == 16);
}
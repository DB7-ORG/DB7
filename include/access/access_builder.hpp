#pragma once

#include "storage/varlen_entry.hpp"
#include "access/table.hpp"

#include <span>

namespace db7::access
{
    // class AccessBuilder
    // {
    // public:
    //     static storage::VarlenEntry CreateVarlenEntry(const std::span<byte> data, Table *table)
    //     {
    //         storage::VarlenEntry entry;
    //         if (data.size() > storage::INLINE_SIZE_CAP)
    //         {
    //             auto [offset, page_id] = table->Insert(data);
    //             entry.Set(data, page_id, offset);
    //         }
    //         else
    //         {
    //             entry.Set(data);
    //         }
    //         return entry;
    //     }
    // };
}
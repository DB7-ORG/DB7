#pragma once

#include "common.hpp"

namespace db7
{
    struct TupleId
    {
        u64 value;

        constexpr TupleId() = default;
        constexpr TupleId(u64 val) : value(val) {}
        constexpr TupleId(u32 idx, u32 pagid)
            : value((static_cast<u64>(pagid) << 32) | idx) {}

        constexpr u32 GetIndex() const { return static_cast<u32>(value); }
        constexpr u32 GetPageId() const { return static_cast<u32>(value >> 32); }
        constexpr u64 GetValue() const { return value; }

        constexpr operator u64() const { return value; }

        friend constexpr bool operator==(TupleId, TupleId) = default;
        friend constexpr auto operator<=>(TupleId, TupleId) = default;
    };

    constexpr TupleId INVALID_TID = TupleId(0);
}
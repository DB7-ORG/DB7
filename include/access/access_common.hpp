#pragma once

#include "common.hpp"
#include "shared/macro_helper.hpp"

namespace db7::access
{
    enum class type_id : u8
    {
        // Boolean
        BOOLEAN,

        // Integers
        TINYINT,  // int8
        SMALLINT, // int16
        INTEGER,  // int32
        BIGINT,   // int64

        // Unsigned integers
        UTINYINT,  // uint8
        USMALLINT, // uint16
        UINTEGER,  // uint32
        UBIGINT,   // uint64

        DOUBLE,

        VARCHAR,
        VARBINARY,
    };

    constexpr u8 SizeOf(type_id t)
    {
        switch (t)
        {
        case type_id::BOOLEAN:
        case type_id::TINYINT:
        case type_id::UTINYINT:
            return 1;
        case type_id::SMALLINT:
        case type_id::USMALLINT:
            return 2;
        case type_id::INTEGER:
        case type_id::UINTEGER:
            return 4;
        case type_id::BIGINT:
        case type_id::UBIGINT:
        case type_id::DOUBLE:
            return 8;
        case type_id::VARCHAR:
        case type_id::VARBINARY:
            return 16; // VarlenEntry size
        default:
            DB7_UNREACHABLE();
        }
    }
}
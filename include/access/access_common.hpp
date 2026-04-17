#pragma once

#include "common.hpp"

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

        VARCHAR,
    };

    constexpr u8 SizeOf(type_id t)
    {
        switch (t)
        {
        case type_id::BOOLEAN:
            return 1;
        case type_id::TINYINT:
            return 1;
        case type_id::SMALLINT:
            return 2;
        case type_id::INTEGER:
            return 4;
        case type_id::BIGINT:
            return 8;
        case type_id::VARCHAR:
            return 16;
        default:
            return 0; // TODO exception
        }
    }

}
#pragma once

#include "common.hpp"

namespace noisepage::execution::sql
{
    enum class SqlTypeId : i8
    {
        Invalid = -1,
        Boolean,
        TinyInt,   // 1-byte integer
        SmallInt,  // 2-byte integer
        Integer,   // 4-byte integer
        BigInt,    // 8-byte integer
        Real,      // 4-byte float //TODO(Matt): front-end doesn't support this, just changes REAL to DOUBLE
        Double,    // 8-byte float
        Decimal,   // Arbitrary-precision numeric //TODO(Matt): back-end doesn't support this. See #1434
        Date,      // Dates
        Timestamp, // Timestamps
        Char,      // Fixed-length string //TODO(Matt): front-end doesn't support this
        Varchar,   // Variable-length string
        Varbinary  // TODO(Matt): front-end doesn't support this. See #788
    };
}
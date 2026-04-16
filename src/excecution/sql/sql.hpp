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

    uint16_t GetSqlTypeIdSize(SqlTypeId type)
    {
        switch (type)
        {
        case SqlTypeId::Boolean:
            return sizeof(bool);
        case SqlTypeId::TinyInt:
            return sizeof(int8_t);
        case SqlTypeId::SmallInt:
            return sizeof(int16_t);
        case SqlTypeId::Integer:
            return sizeof(int32_t);
        case SqlTypeId::BigInt:
            return sizeof(int64_t);
            //    case SqlTypeId::Float:    // TODO(Matt): not supported on front-end
            //      return sizeof(float);
        case SqlTypeId::Double:
            return sizeof(double);
        case SqlTypeId::Date:
            return sizeof(Date);
        case SqlTypeId::Timestamp:
            return sizeof(Timestamp);
        case SqlTypeId::Varchar:
        case SqlTypeId::Varbinary:
            return storage::VARLEN_COLUMN;
        case SqlTypeId::Decimal:
            return 16; // TODO(Matt): double-check when fixed point decimal support merges
        default:
            // All cases handled
            UNREACHABLE("Impossible type");
        }
    }
}
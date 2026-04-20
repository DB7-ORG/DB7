#pragma once

#include "access/schema_column.hpp"
#include "shared/align_util.hpp"

#include <vector>

namespace db7::access
{
    struct ProjectedRows
    {
    public:
        /**
         * @warning Should match schema when isnserting to catalog for performance
         */
        // std::vector<catalog::col_oid_t> col_oid_ids;
        byte *data;
        u32 total_size;
        u32 row_count;
    };
}
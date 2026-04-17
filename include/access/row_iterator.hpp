#pragma once

#include "catalog/catalog_common.hpp"
#include "access/projected_row.hpp"
#include "shared/macro_helper.hpp"

#include <vector>

namespace db7::access
{
    class RowIterator
    {

    private:
        std::vector<catalog::col_oid_t> col_ids_;
        std::vector<u32> offsets_;
        std::vector<u32> row_size_;

    public:
        DB7_DISALLOW_COPY(RowIterator);

        ProjectedRow Get(u32 idx, byte *page_ptr)
        {
            for (u32 i = 0; i < col_ids_.size(); i++)
            {
                u32 off = offsets_[i];
                u32 size = row_size_[i];
                auto start_col = page_ptr + off;
                auto item_ptr = start_col + idx * size;
            }
        }
    };
}
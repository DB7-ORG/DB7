#pragma once

#include "access/schema_column.hpp"
#include "catalog/catalog_common.hpp"
#include "shared/align_util.hpp"
#include "shared/macro_helper.hpp"

#include <vector>
#include <unordered_map>
#include <memory>

namespace db7::access
{
    class Schema
    {
    private:
        std::vector<SchemaColumn> columns_;

    public:
        DB7_DISALLOW_COPY(Schema);

        Schema(std::vector<SchemaColumn> &&columns)
            : columns_(std::move(columns)) {}

        const std::vector<SchemaColumn> &GetColumns() const
        {
            return columns_;
        }

        u32 CalculateMaxSize(u32 row_count)
        {
            u32 max_size = 0;
            for (const auto &col : columns_)
            {
                max_size = shared::AlignUp(max_size, col.GetTypeSize());
                max_size += col.GetTypeSize() * row_count;
            }
            return max_size;
        }
    };
}
#pragma once

#include "access/schema_column.hpp"
#include "catalog/catalog_common.hpp"
#include "shared/align_util.hpp"
#include "shared/macro_helper.hpp"

#include <vector>
#include <unordered_map>
#include <memory>
#include <span>

namespace db7::access
{
    class Schema
    {
    private:
        std::vector<SchemaColumn> columns_;
        std::unordered_map<catalog::col_oid_t, u32> oid_to_index_;

    public:
        DB7_DISALLOW_COPY(Schema);

        Schema(std::vector<SchemaColumn> &&columns)
            : columns_(std::move(columns))
        {

            u32 i = 0;
            for (auto &col : columns_)
            {
                col.SetPosition(i);
                oid_to_index_[col.GetOid()] = i++;
            }
        }

        const std::vector<SchemaColumn> &GetColumns() const { return columns_; }

        u32 GetColumnIndex(catalog::col_oid_t column_id) { return oid_to_index_[column_id]; }

        SchemaColumn &GetColumn(u32 idx) { return columns_[idx]; }
    };
}
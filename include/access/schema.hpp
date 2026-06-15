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
        std::unordered_map<catalog::col_oid_t, u32> oid_to_index_;

    public:
        DB7_DISALLOW_COPY(Schema);

        Schema(std::vector<SchemaColumn> &&columns)
            : columns_(std::move(columns))
        {

            u32 i = 0;
            for (auto &col : columns_)
            {
                oid_to_index_[col.GetOid()] = i++;
            }
        }

        const std::vector<SchemaColumn> &GetColumns() const
        {
            return columns_;
        }

        std::vector<u32> GetColumnIndexes(std::span<catalog::col_oid_t> column_ids)
        {
            std::vector<u32> result;
            result.reserve(column_ids.size());

            for (auto col_id : column_ids)
            {
                result.emplace_back(oid_to_index_[col_id]);
            }

            return result;
        }

        SchemaColumn &GetColumn(u32 idx)
        {
            return columns_[idx];
        }
    };
}
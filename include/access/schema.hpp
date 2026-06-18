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
    class ProjectedSchemaInfo
    {
    private:
        u32 position_;
        u32 attr_size_;

    public:
        ProjectedSchemaInfo() = default;

        ProjectedSchemaInfo(u32 position, u32 attr_size)
            : position_(position), attr_size_(attr_size) {}

        u32 GetPosition() { return position_; }

        u32 GetAttrSize() { return attr_size_; }
    };

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

        std::vector<ProjectedSchemaInfo> // TODO i hate this
        GetProjectedSchemaInfo(std::span<catalog::col_oid_t> column_ids)
        {
            std::vector<ProjectedSchemaInfo> result;
            result.reserve(column_ids.size());

            for (auto col_id : column_ids)
            {
                auto item = columns_[oid_to_index_[col_id]];
                result.push_back({item.GetPosiiton(), item.GetTypeSize()});
            }

            return result; // NRVO/move
        }
    };
}
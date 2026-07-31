#pragma once

#include "access/schema_column.hpp"
#include "catalog/catalog_common.hpp"
#include "shared/align_util.hpp"
#include "shared/macro_helper.hpp"

#include <vector>
#include <unordered_map>
#include <memory>
#include <span>
#include <ranges>

namespace db7::access
{
    class Schema
    {
    private:
        std::unordered_map<catalog::col_oid_t, SchemaColumn> mapping_;

    public:
        DB7_DISALLOW_COPY(Schema);

        Schema(std::vector<SchemaColumn> &&columns)
        {
            u32 i = 0;
            for (auto &col : columns)
            {
                col.SetPosition(i++);
                mapping_[col.GetOid()] = col;
            }
        }

        const SchemaColumn &GetColumn(catalog::col_oid_t id) const { return mapping_.at(id); }

        u32 GetCount() const { return mapping_.size(); }

        auto begin()
        {
            return std::views::values(mapping_).begin();
        }

        auto end()
        {
            return std::views::values(mapping_).end();
        }

        auto begin() const
        {
            return std::views::values(mapping_).begin();
        }

        auto end() const
        {
            return std::views::values(mapping_).end();
        };
    };
}
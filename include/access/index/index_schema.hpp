#pragma once

#include "access/schema_column.hpp"
#include "catalog/catalog_common.hpp"
#include "shared/align_util.hpp"
#include "shared/macro_helper.hpp"
#include "storage/storage_common.hpp"
#include "storage/page_header.hpp"

#include <vector>
#include <unordered_map>
#include <memory>

namespace db7::access
{
    class IndexSchema
    {
    private:
        std::vector<SchemaColumn> columns_;

    public:
        DB7_DISALLOW_COPY(IndexSchema);

        IndexSchema() = default;

        IndexSchema(std::vector<SchemaColumn> columns)
            : columns_(std::move(columns))
        {
        }

        const std::vector<SchemaColumn> &GetColumns() const
        {
            return columns_;
        }
    };
}
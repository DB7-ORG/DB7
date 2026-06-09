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
    };
}
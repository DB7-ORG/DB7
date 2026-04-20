#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"

#include <vector>

namespace db7::access
{
    class SchemaColumn
    {
    private:
        catalog::col_oid_t oid_;
        type_id col_type_;
        std::string col_name_;

    public:
        SchemaColumn() {}

        SchemaColumn(catalog::col_oid_t oid, type_id col_type, std::string col_name)
            : oid_(oid), col_type_(col_type), col_name_(std::move(col_name)) {}

        catalog::col_oid_t GetOid() const
        {
            return oid_;
        }

        u32 GetTypeSize() const
        {
            return SizeOf(col_type_);
        }
    };
}
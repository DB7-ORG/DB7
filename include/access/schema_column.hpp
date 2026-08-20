#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"

#include <span>

namespace db7::access
{
    class SchemaColumn
    {
    private:
        catalog::col_oid_t oid_;
        type_id col_type_;
        u32 type_size_;
        std::string col_name_;
        u32 position_;
        bool is_nullable_ = false;

    public:
        SchemaColumn() {}

        SchemaColumn(catalog::col_oid_t oid, type_id col_type, std::string col_name)
            : oid_(oid), col_type_(col_type), type_size_(SizeOf(col_type)), col_name_(std::move(col_name)) {}

        catalog::col_oid_t GetOid() const { return oid_; }

        type_id GetType() const { return col_type_; }

        u32 GetTypeSize() const { return type_size_; }

        const std::string &GetName() const { return col_name_; }

        std::span<char> GetNameSpan() { return {col_name_.data(), col_name_.size()}; }

        void SetPosition(u32 position) { position_ = position; }

        u32 GetPosiiton() const { return position_; }

        bool IsNullable() const { return is_nullable_; }
    };
}
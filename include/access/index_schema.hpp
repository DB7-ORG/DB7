#pragma once

#include "access/schema.hpp"

namespace db7::access
{
    class IndexSchema : public Schema
    {
    private:
        bool is_unique_;
        bool is_primary_;
        bool is_exclusion_;
        bool is_immediate_;

    public:
        IndexSchema(
            std::vector<SchemaColumn> &&columns,
            bool is_unique,
            bool is_primary,
            bool is_exclusion,
            bool is_immediate)
            : Schema(std::move(columns)),
              is_unique_(is_unique),
              is_primary_(is_primary),
              is_exclusion_(is_exclusion),
              is_immediate_(is_immediate) {}

        bool IsUnique() const { return is_unique_; }
        bool IsPrimary() const { return is_primary_; }
        bool IsExclusion() const { return is_exclusion_; }
        bool IsImmediate() const { return is_immediate_; }
    };
}
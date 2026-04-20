#pragma once

#include "access/schema.hpp"
#include "access/projected_rows.hpp"

#include <vector>
#include <cstring>

namespace db7::access
{
    class ProjectedRowsBuilder
    {
        const Schema *schema_;
        byte *data_;
        byte *curr_;
        u32 row_count_;
        // std::vector<catalog::col_oid_t> col_oids_;

    public:
        ProjectedRowsBuilder(Schema *schema)
            : schema_(schema)
        {
        }

        void PrepareBuilder(u32 row_count)
        {
            u32 max_size = 0;
            for (const auto &col : schema_->GetColumns())
            {
                max_size = shared::AlignUp(max_size, col.GetTypeSize());
                max_size += col.GetTypeSize() * row_count;
            }
            data_ = new byte[max_size]();
            curr_ = data_;
        }

        template <typename T>
        void Set(catalog::col_oid_t oid, const T &value)
        {
            (void)oid;

            curr_ = shared::AlignUp(curr_, alignof(T));
            *(reinterpret_cast<T *>(curr_)) = value;
            curr_ += sizeof(T) * row_count_;
            // col_oids_.push_back(oid);
        }

        // For variable-length / string columns
        void SetBytes(catalog::col_oid_t oid, const byte *src, u32 len, u32 align = 16)
        {
            (void)oid;

            curr_ = shared::AlignUp(curr_, align);
            memcpy(curr_, src, len);
            curr_ += len * row_count_; // or fixed field width
            // col_oids_.push_back(oid);
        }

        ProjectedRows Build()
        {
            return ProjectedRows{
                //.col_oid_ids = col_oids_,
                .data = data_,
                .total_size = static_cast<u32>(curr_ - data_),
                .row_count = row_count_};
        }
    };
}
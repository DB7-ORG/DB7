#pragma once

#include "access/schema.hpp"
#include "access/projected_rows.hpp"

#include <initializer_list>
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
        ProjectedRowsBuilder(Schema *schema, byte *data, u32 row_count)
            : schema_(schema), data_(data), curr_(data), row_count_(row_count) {}

        template <typename T>
        void Push(std::initializer_list<T> values)
        {
            DB7_ASSERT(values.size() == row_count_, "Invalid values dont match schema");

            curr_ = shared::AlignUp(curr_, alignof(T));
            T *dst = reinterpret_cast<T *>(curr_);
            for (u32 i = 0; i < row_count_; i++)
                dst[i] = *(values.begin() + i);
            curr_ += sizeof(T) * row_count_;
        }

        ProjectedRows Build()
        {
            return ProjectedRows{
                .data = data_,
                .total_size = static_cast<u32>(curr_ - data_),
                .row_count = row_count_};
        }
    };
}
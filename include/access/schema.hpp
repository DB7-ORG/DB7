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
    class Schema
    {
    private:
        std::vector<SchemaColumn> columns_;
        std::unordered_map<catalog::col_oid_t, u32> col_to_offset_map_;

        void CalculateOffsets(u32 header_size)
        {
            u32 row_size = 0;
            u32 worst_case_pad = 0;
            for (auto &col : columns_)
            {
                u32 item_size = col.GetTypeSize();
                row_size += item_size;
                worst_case_pad += item_size - 1;
            }

            const u32 row_count = (PAGE_SIZE - header_size - worst_case_pad) * 8 / (1 + 8 * row_size);

            u32 curr_offset = header_size + (row_count + 7) / 8;
            for (auto &col : columns_)
            {
                // pad to type
                auto oid = col.GetOid();
                u32 size = col.GetTypeSize();
                curr_offset = shared::AlignUp(curr_offset, size);

                // write to map
                col_to_offset_map_[oid] = curr_offset;
                curr_offset += row_count * size;
            }
        }

    public:
        DB7_DISALLOW_COPY(Schema);

        Schema(std::vector<SchemaColumn> columns)
            : columns_(std::move(columns))
        {
            CalculateOffsets(storage::HEADER_SIZE);
        }

        const std::vector<SchemaColumn> &GetColumns() const
        {
            return columns_;
        }

        u32 GetOffset(catalog::col_oid_t oid) const
        {
            auto it = col_to_offset_map_.find(oid);
            if (it == col_to_offset_map_.end())
            {
                throw std::runtime_error("column oid not found");
                // or DB7_UNREACHABLE() if caller guarantees validity
            }
            return it->second;
        }

        const std::unordered_map<catalog::col_oid_t, u32> &GetOffsetMap()
        {
            return col_to_offset_map_;
        }

        u32 CalculateMaxSize(u32 row_count)
        {
            u32 max_size = 0;
            for (const auto &col : columns_)
            {
                max_size = shared::AlignUp(max_size, col.GetTypeSize());
                max_size += col.GetTypeSize() * row_count;
            }
            return max_size;
        }
    };
}
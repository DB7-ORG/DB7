#pragma once

#include "access/schema_column.hpp"
#include "catalog/catalog_common.hpp"
#include "shared/align_util.hpp"
#include "shared/macro_helper.hpp"
#include "storage/storage_common.hpp"

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

        void CalculateOffsets()
        {
            u32 row_size = 0;
            u32 worst_case_pad = 0;
            for (auto &col : columns_)
            {
                u32 item_size = col.GetTypeSize();
                row_size += item_size;
                worst_case_pad += item_size - 1;
            }

            const u32 row_count = (storage::PAYLOAD_SIZE - worst_case_pad) / row_size;

            u32 curr_offset = storage::HEADER_SIZE;
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
            CalculateOffsets();
        }

        const std::vector<SchemaColumn> &GetColumns() const
        {
            return columns_;
        }

        const std::unordered_map<catalog::col_oid_t, u32> &GetOffsetMap()
        {
            return col_to_offset_map_;
        }
    };
}
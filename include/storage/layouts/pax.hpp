#pragma once

#include "storage/storage_common.hpp"
#include "shared/align_util.hpp"
#include "storage/page_header.hpp"
#include "shared/macro_helper.hpp"

#include <vector>
#include <span>

namespace db7::storage
{
    class PaxLayout
    {
    private:
        std::vector<u32> offsets_;
        std::vector<u16> sizes_;
        u32 max_row_count_;

        void CalculateOffsets();

        // TODO not used
        u32 CalculateMaxSize(u32 row_count)
        {
            u32 max_size = 0;
            for (const auto size : sizes_)
            {
                max_size = shared::AlignUp(max_size, (u32)size);
                max_size += size * row_count;
            }
            return max_size;
        }

    public:
        PaxLayout() = default;

        PaxLayout(std::vector<u16> &&sizes);

        u32 IncrementHeaderCount(byte *data, u32 count);

        u32 CalcOffset(u32 row_idx, u32 column_idx);

        void Update(byte *dest, std::span<byte> update_data);

        void Update(byte *page_data, std::span<byte> update_data, u16 column_idx, u32 row_idx);

        void Insert(byte *page_data, std::span<byte> insert_data, u16 column_idx, u32 row_idx);

        void Delete(byte *data, u32 row_idx);

        byte *Get(byte *page_data, u16 column_idx, u32 row_idx);

        u32 GetMaxRowCount() { return max_row_count_; }
    };
}
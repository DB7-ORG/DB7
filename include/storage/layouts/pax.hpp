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

        void CalculateOffsets();

    public:
        PaxLayout() = default;

        PaxLayout(std::vector<u16> &&sizes);

        u32 IncrementHeaderCount(byte *data, u32 count);

        void Insert(byte *page_data, std::span<byte> insert_data, u16 column_idx, u32 row_idx);

        void Delete(byte *data, u32 row_idx);

        byte *Get(byte *page_data, u16 column_idx, u32 row_idx);
    };
}
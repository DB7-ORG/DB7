#include "storage/layouts/pax.hpp"

namespace db7::storage
{
    PaxLayout::PaxLayout(std::vector<u16> &&sizes) : sizes_(std::move(sizes))
    {
        offsets_.reserve(sizes.size());
        CalculateOffsets();
    }

    void PaxLayout::CalculateOffsets()
    {
        u32 header_size = HEADER_SIZE;
        u32 row_size = 0;
        u32 worst_case_pad = 0;
        for (auto &size : sizes_)
        {
            row_size += size;
            worst_case_pad += size - 1;
        }

        max_row_count_ = (DB7_PAGE_SIZE - header_size - worst_case_pad) * 8 / (1 + 8 * row_size);

        u32 curr_offset = header_size + (max_row_count_ + 7) / 8;
        for (auto &size : sizes_)
        {
            // pad to type
            curr_offset = shared::AlignUp(curr_offset, (u32)size);
            offsets_.emplace_back(curr_offset);
            curr_offset += max_row_count_ * size;
        }
    }

    void InternalWrite(byte *data_, std::span<byte> payload)
    {
        DB7_ASSERT(data_ != nullptr, "Page data in null");
        memcpy(data_, payload.data(), payload.size());
    }

    u32 PaxLayout::IncrementHeaderCount(byte *data, u32 count) const
    {
        auto *header = storage::PageHeader::CastHeader(data);
        u32 old = header->count;
        header->count += count;
        DB7_ASSERT(max_row_count_ > header->count, "Tried to insert more than available in page");
        return old;
    }

    u32 PaxLayout::CalcOffset(u32 row_idx, u32 column_idx) const { return row_idx * sizes_[column_idx] + offsets_[column_idx]; }

    void PaxLayout::Update(byte *dest, std::span<byte> update_data) const
    {
        std::memcpy(dest, update_data.data(), update_data.size());
    }

    void PaxLayout::Update(byte *page_data, std::span<byte> update_data, u16 column_idx, u32 row_idx) const
    {
        u32 off = CalcOffset(row_idx, column_idx);
        std::memcpy(page_data + off, update_data.data(), update_data.size());
    }

    void PaxLayout::Insert(byte *page_data, std::span<byte> insert_data, u16 column_idx, u32 row_idx) const
    {
        u32 off = CalcOffset(row_idx, column_idx);
        InternalWrite(page_data + off, insert_data);
    }

    void PaxLayout::Delete(byte *data, u32 row_idx) const
    {
        u32 byte_offset = storage::HEADER_SIZE + row_idx / 8;
        byte mask = byte(1 << (row_idx % 8));
        data[byte_offset] |= mask;
    }

    byte *PaxLayout::Get(byte *page_data, u16 column_idx, u32 row_idx) const
    {
        u32 off = CalcOffset(row_idx, column_idx);
        return page_data + off;
    }

    bool PaxLayout::IsDeleted(byte *data, u32 row_idx) const
    {
        u32 byte_offset = storage::HEADER_SIZE + row_idx / 8;
        byte mask = byte(1 << (row_idx % 8));
        return (data[byte_offset] & mask) > 0;
    }
}
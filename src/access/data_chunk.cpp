#include "access/data_chunk.hpp"
#include "shared/align_util.hpp"
#include "storage/varlen_entry.hpp"

namespace db7::access
{
    DataChunkLayout::DataChunkLayout(
        std::span<const catalog::col_oid_t> col_ids,
        std::span<const u32> attr_sizes)
        : column_count_(col_ids.size())
    {
        column_ids_ = 2 * sizeof(u32);
        header_size_ = column_ids_ + sizeof(catalog::col_oid_t) * col_ids.size();
        offsets_ = shared::AlignUp(header_size_, (u32)sizeof(u32));
        header_size_ = offsets_ + sizeof(u32) * col_ids.size();

        header_underlying_ = new byte[header_size_];
        *(u32 *)header_underlying_ = col_ids.size();

        catalog::col_oid_t *column_ids_ptr = reinterpret_cast<catalog::col_oid_t *>(header_underlying_ + column_ids_);
        std::memcpy(column_ids_ptr, col_ids.data(), col_ids.size() * sizeof(catalog::col_oid_t));

        u32 *offsets_ptr = reinterpret_cast<u32 *>(header_underlying_ + offsets_);

        total_size_ = header_size_;

        u32 i = 0;
        for (auto size : attr_sizes)
        {
            total_size_ = shared::AlignUp(total_size_, std::min(size, u32(8)));

            offsets_ptr[i++] = total_size_;

            total_size_ += size;
        }

        total_size_ = shared::AlignUp(total_size_, u32(8));

        *((u32 *)header_underlying_ + 1) = total_size_;
    }

    DataChunk DataChunkLayout::CreateDataChunk()
    {
        return DataChunk(this);
    }

    DataChunk::DataChunk(DataChunkLayout *layout)
    {
        underlying_ = new byte[layout->total_size_];
        column_ids_ = reinterpret_cast<catalog::col_oid_t *>(underlying_ + layout->column_ids_);
        offsets_ = reinterpret_cast<u32 *>(underlying_ + layout->offsets_);
        data_ = underlying_ + layout->header_size_;
        total_size_ = layout->total_size_;
        column_count_ = layout->column_count_;
        std::memcpy(underlying_, layout->header_underlying_, layout->header_size_);
    }

    void DataChunk::Print(Schema *schema)
    {
        for (u32 i = 0; i < column_count_; i++)
        {
            auto col_id = column_ids_[i];

            auto idx = schema->GetColumnIndex(col_id);

            auto &col = schema->GetColumn(idx);

            std::cout << col.GetName() << " | ";

            switch (col.GetType())
            {
            case type_id::VARCHAR:
            {

                auto entry = *(storage::VarlenEntry *)Access(i);
                if (entry.IsInline())
                {
                    std::cout.write(entry.GetInline(), entry.GetSize());
                }
                else
                {
                    std::cout << "not inlined";
                }
                break;
            }
            default:
                std::cout << *reinterpret_cast<u32 *>(Access(i));
                break;
            }

            std::cout << " | ";
        }

        std::cout << "\n";
    }
}
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
        u32 column_ids_ = 2 * sizeof(u32);
        header_size_ = column_ids_ + sizeof(catalog::col_oid_t) * col_ids.size();
        u32 offsets_ = shared::AlignUp(header_size_, (u32)sizeof(u32));
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

    DataChunk *DataChunkLayout::CreateDataChunk()
    {
        byte *dest = new byte[total_size_];
        std::memcpy(dest, header_underlying_, header_size_);
        return reinterpret_cast<DataChunk *>(dest);
    }

    DataChunk *DataChunkLayout::CreateDataChunk(void *dest)
    {
        std::memcpy(dest, header_underlying_, header_size_);
        return reinterpret_cast<DataChunk *>(dest);
    }

    void DataChunk::Print(Schema *schema)
    {
        for (u32 i = 0; i < column_count_; i++)
        {
            auto col_id = GetColumnIdsPtr()[i];

            auto &col = schema->GetColumn(col_id);

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
#include "access/data_chunk.hpp"
#include "shared/align_util.hpp"
#include "storage/varlen_entry.hpp"

namespace db7::access
{
    static std::vector<catalog::col_oid_t> BuildColumnIds(const Schema &schema)
    {
        std::vector<catalog::col_oid_t> ids;
        ids.reserve(schema.GetCount());

        for (const auto &col : schema)
            ids.push_back(col.GetOid());

        return ids;
    }

    static std::vector<u16> BuildAttrSizes(const Schema &schema)
    {
        std::vector<u16> sizes;
        sizes.reserve(schema.GetCount());

        for (const auto &col : schema)
            sizes.push_back(col.GetTypeSize());

        return sizes;
    }

    DataChunkLayout::DataChunkLayout(const Schema &schema)
        : DataChunkLayout(BuildColumnIds(schema), BuildAttrSizes(schema)) {}

    DataChunkLayout::DataChunkLayout(
        std::span<const catalog::col_oid_t> col_ids,
        std::span<const u16> attr_sizes)
        : column_count_(col_ids.size())
    {
        u32 column_ids_ = 2 * sizeof(u32);
        header_size_ = column_ids_ + sizeof(catalog::col_oid_t) * col_ids.size();
        u32 offsets_ = shared::AlignUp(header_size_, (u32)sizeof(u16));
        header_size_ = offsets_ + sizeof(u16) * col_ids.size();

        header_underlying_ = new byte[header_size_];
        *(u32 *)header_underlying_ = col_ids.size();

        catalog::col_oid_t *column_ids_ptr = reinterpret_cast<catalog::col_oid_t *>(header_underlying_ + column_ids_);
        std::memcpy(column_ids_ptr, col_ids.data(), col_ids.size() * sizeof(catalog::col_oid_t));

        u16 *offsets_ptr = reinterpret_cast<u16 *>(header_underlying_ + offsets_);

        total_size_ = header_size_;

        u32 i = 0;
        for (auto size : attr_sizes)
        {
            total_size_ = shared::AlignUp(total_size_, (u32)std::min(size, u16(8)));

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
        u32 idx = 0;
        for (auto id : GetColumnIds())
        {
            auto col = schema->GetColumn(id);
            byte *ptr = Access(idx++);
            std::cout << col.GetName() << " | ";

            type_id t = col.GetType();
            if (t == type_id::VARCHAR || t == type_id::VARBINARY)
            {
                auto *e = reinterpret_cast<storage::VarlenEntry *>(ptr);
                if (e->IsInline())
                    std::cout.write(e->GetInline(), e->GetSize());
                else
                    std::cout << "vlen(pid=" << e->GetRef().pid
                              << ",off=" << e->GetRef().offset << ")";
            }
            else
            {
                switch (SizeOf(t))
                {
                case 1:
                    // print bools/tinyints as int, not char
                    std::cout << (t == type_id::TINYINT
                                      ? (i64) * (i8 *)ptr
                                      : (u64) * (u8 *)ptr);
                    break;
                case 2:
                    std::cout << (t == type_id::SMALLINT ? (i64) * (i16 *)ptr
                                                         : (u64) * (u16 *)ptr);
                    break;
                case 4:
                    std::cout << (t == type_id::INTEGER ? (i64) * (i32 *)ptr
                                                        : (u64) * (u32 *)ptr);
                    break;
                case 8:
                    if (t == type_id::DOUBLE)
                        std::cout << *(double *)ptr;
                    else if (t == type_id::BIGINT)
                        std::cout << *(i64 *)ptr;
                    else
                        std::cout << *(u64 *)ptr;
                    break;
                }
            }
            std::cout << " | ";
        }
    }
}
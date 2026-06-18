#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"
#include "access/schema.hpp"

#include <vector>
#include <span>
#include <cstring>

namespace db7::access
{
    class DataChunk;

    constexpr u32 SIZE_PART = 2 * sizeof(u32);
    constexpr u32 COLUMN_IDS_START = shared::AlignUp(SIZE_PART, u32(sizeof(catalog::col_oid_t)));

    class DataChunkLayout
    {
    private:
        u32 column_count_;
        u32 total_size_;

        u32 header_size_;
        byte *header_underlying_;

        u32 column_ids_;
        u32 offsets_;

        catalog::col_oid_t *GetColumnIdsPtr() { return reinterpret_cast<catalog::col_oid_t *>(header_underlying_ + COLUMN_IDS_START); }

    public:
        DB7_DISALLOW_COPY(DataChunkLayout);

        DataChunkLayout(
            std::span<const catalog::col_oid_t> col_ids,
            std::span<const u32> attr_sizes);

        DataChunkLayout(
            std::initializer_list<catalog::col_oid_t> col_ids,
            std::initializer_list<u32> attr_sizes)
            : DataChunkLayout(
                  std::span<const catalog::col_oid_t>(col_ids.begin(), col_ids.size()),
                  std::span<const u32>(attr_sizes.begin(), attr_sizes.size())) {}

        ~DataChunkLayout() { delete[] header_underlying_; }

        u32 GetTotalSize() { return total_size_; }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(GetColumnIdsPtr(), column_count_); };

        DataChunk *CreateDataChunk();

        DataChunk *CreateDataChunk(void *dest);
    };

    class DataChunk
    {
    private:
        u32 column_count_;
        u32 total_size_;
        u64 varlen_contents_[0];

        byte *GetUnderlyingPtr() { return reinterpret_cast<byte *>(&column_count_); }

        catalog::col_oid_t *GetColumnIdsPtr() { return reinterpret_cast<catalog::col_oid_t *>(GetUnderlyingPtr() + COLUMN_IDS_START); }

        u32 *GetOffsetsPtr()
        {
            auto aligned = shared::AlignUp(uintptr_t(GetColumnIdsPtr() + column_count_), uintptr_t(sizeof(u32)));
            return reinterpret_cast<u32 *>(aligned);
        }

        byte *GetDataPtr() { return reinterpret_cast<byte *>(GetOffsetsPtr() + column_count_); }

    public:
        DB7_DISALLOW_COPY(DataChunk);

        std::span<byte> GetUnderlying() { return std::span<byte>(GetUnderlyingPtr(), total_size_); }

        std::span<byte> GetHeaderPtr() { return std::span<byte>(GetUnderlyingPtr(), GetHeaderSize()); }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(GetColumnIdsPtr(), column_count_); }

        // std::span<ProjectedSchemaInfo> GetSchemaInfo() { return std::span<ProjectedSchemaInfo>(schema_info_, column_count_); }

        byte *Access(u32 idx) { return GetUnderlyingPtr() + GetOffsetsPtr()[idx]; }

        u32 GetSize() { return total_size_; }

        u32 GetHeaderSize() { return GetDataPtr() - GetUnderlyingPtr(); }

        u32 GetCount() { return column_count_; }

        class Iterator
        {
        private:
            u32 iterator_idx_;

            byte *iterator_data_ptr_;

            u32 *iterator_offsets_;

        public:
            Iterator(DataChunk *chunk)
            {
                iterator_idx_ = 0;
                iterator_data_ptr_ = chunk->GetUnderlyingPtr();
                iterator_offsets_ = chunk->GetOffsetsPtr();
            }

            void PushBack(std::span<byte> new_data)
            {
                auto off = iterator_offsets_[iterator_idx_++];
                std::memcpy(iterator_data_ptr_ + off, new_data.data(), new_data.size());
            }

            template <typename T>
            void PushBack(T new_data)
            {
                DB7_ASSERT(shared::IsAligned<T>(iterator_data_ptr_ + iterator_offsets_[iterator_idx_]), "unaligned write");
                *(T *)(iterator_data_ptr_ + iterator_offsets_[iterator_idx_++]) = new_data;
            }

            byte *Next()
            {
                return iterator_data_ptr_ + iterator_offsets_[iterator_idx_++];
            }
        };

        Iterator InitIterator()
        {
            return Iterator(this);
        }

        void Print(Schema *schema);
    };

}
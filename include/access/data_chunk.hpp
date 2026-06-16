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

    class DataChunkLayout
    {
    public:
        u32 column_count_;
        u32 total_size_;

        u32 header_size_;
        byte *header_underlying_;

        u32 column_ids_;
        u32 offsets_;

        DB7_DISALLOW_COPY(DataChunkLayout);

        DataChunkLayout(
            std::span<const catalog::col_oid_t> col_ids,
            std::span<const u32> attr_sizes);

        DataChunkLayout(
            std::initializer_list<catalog::col_oid_t> col_ids,
            std::initializer_list<u32> attr_sizes)
            : DataChunkLayout(
                  std::span<const catalog::col_oid_t>(col_ids.begin(), col_ids.size()),
                  std::span<const u32>(attr_sizes.begin(), attr_sizes.size()))
        {
        }

        ~DataChunkLayout()
        {
            delete[] header_underlying_;
        }

        DataChunk CreateDataChunk();
    };

    class DataChunk
    {
    private:
        /**
         * util ptr
         */
        byte *underlying_;

        catalog::col_oid_t *column_ids_;

        u32 *offsets_;

        byte *data_;

        u32 total_size_;

        u32 column_count_;

    public:
        DB7_DISALLOW_COPY(DataChunk);

        DataChunk(DataChunkLayout *layout);

        ~DataChunk() { delete[] underlying_; }

        std::span<byte> GetUnderlying() { return std::span<byte>(underlying_, total_size_); }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(column_ids_, column_count_); }

        byte *Access(u32 idx) { return underlying_ + offsets_[idx]; }

        u32 GetSize() { return total_size_; }

        u32 GetHeaderSize() { return data_ - underlying_; }

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
                iterator_data_ptr_ = chunk->underlying_;
                iterator_offsets_ = chunk->offsets_;
            }

            void PushBack(std::span<byte> new_data)
            {
                std::memcpy(iterator_data_ptr_ + iterator_offsets_[iterator_idx_++], new_data.data(), new_data.size());
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
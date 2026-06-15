#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"

#include <vector>
#include <span>

namespace db7::access
{
    class DataChunk
    {
    private:
        u32 column_count_;

        u32 size_;

        catalog::col_oid_t *column_ids_;

        u32 *offsets_;

        byte *data_;

        /**
         * util ptr
         */
        byte *underlying_;

    public:
        DataChunk(u32 column_count, u32 size)
            : column_count_(column_count), size_(size)
        {
            DB7_ASSERT(sizeof(catalog::col_oid_t) == 4, "this changed and needs to be aligned");

            byte *underlying = new byte[size];

            underlying_ = underlying;

            *(u32 *)underlying = column_count_;
            underlying += sizeof(u32);

            column_ids_ = (catalog::col_oid_t *)underlying;
            underlying += column_count * sizeof(catalog::col_oid_t);

            offsets_ = (u32 *)underlying;
            underlying += column_count * sizeof(u32);

            data_ = underlying;
        }

        void SetColumnIds(std::initializer_list<catalog::col_oid_t> column_ids)
        {
            std::memcpy(column_ids_, column_ids.begin(), column_ids.size() * sizeof(catalog::col_oid_t));
        }

        ~DataChunk() { delete[] underlying_; }

        std::span<byte> GetUnderlying() { return std::span<byte>(underlying_, size_); }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(column_ids_, column_count_); }

        byte *Access(u32 idx) { return underlying_ + offsets_[idx]; }

        u32 GetSize() { return size_; }

        void SetSize(u32 size) { size_ = size; }

        u32 GetHeaderSize() { return data_ - underlying_; }

        u32 GetCount() { return column_count_; }

        class Iterator
        {
        private:
            u32 iterator_idx_;

            byte *iterator_data_ptr_;

            DataChunk *chunk_;

        public:
            Iterator(DataChunk *chunk)
            {
                iterator_idx_ = 0;
                iterator_data_ptr_ = chunk->data_;
                chunk_ = chunk;
            }

            void PushBack(std::span<byte> new_data)
            {
                std::memcpy(iterator_data_ptr_, new_data.data(), new_data.size());
                chunk_->offsets_[iterator_idx_++] = iterator_data_ptr_ - chunk_->underlying_;
                iterator_data_ptr_ += new_data.size();
            }

            template <typename T>
            void PushBack(T new_data)
            {
                *(T *)iterator_data_ptr_ = new_data;
                chunk_->offsets_[iterator_idx_++] = iterator_data_ptr_ - chunk_->underlying_;
                iterator_data_ptr_ += sizeof(new_data);
            }

            byte *Next()
            {
                return chunk_->underlying_ + chunk_->offsets_[iterator_idx_++];
            }
        };

        Iterator InitIterator()
        {
            return Iterator(this);
        }
    };

    class DataChunkInitializer
    {
    private:
        u32 size_;
        std::vector<catalog::col_oid_t> columns_ids_;
        std::vector<u32> offsets_;

    public:
        DataChunkInitializer(std::vector<catalog::col_oid_t> columns_ids)
            : size_(0), columns_ids_(std::move(columns_ids)) {}

        DataChunk InitializeDataChunk(std::span<u32> attr_sizes)
        {
            DataChunk chunk(columns_ids_.size(), 0);

            size_ = chunk.GetHeaderSize();

            u32 i = 0;
            for (u32 size : attr_sizes)
            {
                size_ = shared::AlignUp(size_, std::min(size, u32(8)));
                offsets_[i++] = size_;
                size_ += size;
            }

            size_ = shared::AlignUp(size_, u32(8));

            chunk.SetSize(size_);

            return chunk;
        }
    };
}
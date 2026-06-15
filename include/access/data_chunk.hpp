#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"

#include <vector>
#include <span>

namespace db7::access
{
    // class Vector
    // {
    // private:
    //     type_id type_;
    //     u32 type_size_;
    //     u32 capacity_;
    //     u32 count_;
    //     byte *data_;

    // public:
    //     Vector() = default;

    //     Vector(type_id type, u32 capacity = 1024)
    //         : type_(type), type_size_(SizeOf(type)), capacity_(capacity), count_(0), data_(nullptr)
    //     {
    //         data_ = new byte[capacity_ * type_size_];
    //     }

    //     byte *GetData() const { return data_; }

    //     void SetData(byte *data) { data_ = data; }

    //     u32 GetSize() const { return type_size_; }

    //     type_id GetType() const { return type_; }

    //     void Append(byte *data, u32 count)
    //     {
    //         DB7_ASSERT((capacity_ - count_) >= count, "no space in vector");
    //         std::memcpy(data_ + count_, data, count * type_size_);
    //         count_ += count;
    //     }

    //     template <typename T>
    //     void Append(T data)
    //     {
    //         DB7_ASSERT((capacity_ - count_) >= 1, "no space in vector");
    //         *(T *)(data_ + count_) = data;
    //         count_++;
    //     }

    //     void Clear() { count_ = 0; }

    //     u32 GetCount() { return count_; }
    // };

    // class DataChunk2
    // {
    // private:
    //     std::vector<catalog::col_oid_t> column_ids_;

    //     std::vector<u32> column_indexes_;
    //     /**
    //      * number of elements in each Vector in data
    //      */
    //     u32 vec_count_;
    //     /**
    //      * sum of all vector sizes from data_
    //      */
    //     u32 total_space_;
    //     /**
    //      * @note When inserting columns should be ordered as schema.
    //      */
    //     std::vector<Vector> data_;

    // public:
    //     DataChunk(u32 col_count, u32 vec_count)
    //         : vec_count_(vec_count), total_space_(0)
    //     {
    //         data_.reserve(col_count);
    //         column_ids_.reserve(col_count);
    //         column_indexes_.reserve(col_count);
    //     }

    //     u32 GetCount() const { return vec_count_; }

    //     u32 GetColumnCount() const { return data_.size(); }

    //     u32 GetTotalSpace() { return total_space_; }

    //     std::span<byte> GetColumnsRaw() { return std::span(reinterpret_cast<byte *>(column_ids_.data()), column_ids_.size()); }

    //     u32 GetColumnSize(u32 idx) { return data_[idx].GetSize(); }

    //     type_id GetColumnType(u32 idx) { return data_[idx].GetType(); }

    //     void Move(std::vector<catalog::col_oid_t> column_ids) { column_ids_ = std::move(column_ids); }

    //     void Move(std::vector<Vector> vectors) { data_ = std::move(vectors); }

    //     void AppendColumnId(catalog::col_oid_t col_id) { column_ids_.emplace_back(col_id); }

    //     const std::vector<u32> &GetColumnIndexes() { return column_indexes_; }

    //     void AppendColumnIndex(u32 idx) { column_indexes_.emplace_back(idx); }

    //     Vector &GetVector(u32 idx) { return data_[idx]; }

    //     std::span<byte> GetVectorColumn(u32 idx) { return std::span(data_[idx].GetData(), data_[idx].GetSize()); }

    //     void Reset()
    //     {
    //         for (auto &data : data_)
    //         {
    //             data.Clear();
    //         }
    //         data_.clear();
    //         vec_count_ = 0;
    //         total_space_ = 0;
    //     }

    //     u32 GetSerializedSize()
    //     {
    //         u32 result = column_ids_.size() * sizeof(catalog::col_oid_t);
    //         for (auto &vec : data_)
    //         {
    //             result += vec.GetSize();
    //         }
    //         return result;
    //     }

    //     void SerializeIntoBuffer(byte *buffer)
    //     {
    //         auto cols = GetColumnsRaw();
    //         u32 header_size = cols.size() * sizeof(catalog::col_oid_t);
    //         std::memcpy(buffer, cols.data(), header_size);
    //         buffer += header_size;
    //         for (auto &data : data_)
    //         {
    //             header_size = data.GetSize() * data.GetCount();
    //             std::memcpy(buffer, data.GetData(), header_size);
    //             buffer += header_size;
    //         }
    //     }
    // };

    class DataChunk
    {
    private:
        byte underlying_[0];

        u32 column_count_;

        u32 size_;

        catalog::col_oid_t *column_ids_;

        u32 *offsets_;

        byte *data_;

        /**
         * Iterator should be seperate class
         */
        u32 iterator_idx_;

        byte *iterator_data_ptr_;

    public:
        DataChunk(u32 column_count, u32 tuple_count, u32 size)
            : column_count_(column_count), size_(size)
        {
            byte *underlying = new byte[size];

            *(u32 *)underlying = column_count_;
            underlying += sizeof(u32);

            column_ids_ = (catalog::col_oid_t *)underlying;
            underlying += column_count * sizeof(catalog::col_oid_t);

            offsets_ = (u32 *)underlying;
            underlying += column_count * sizeof(u32);

            data_ = underlying;

            iterator_idx_ = 0;
            iterator_data_ptr_ = underlying;
        }

        void SetColumnIds(std::initializer_list<catalog::col_oid_t> column_ids)
        {
            std::memcpy(column_ids_, column_ids.begin(), column_ids.size() * sizeof(catalog::col_oid_t));
        }

        ~DataChunk() { delete[] (&column_count_); }

        std::span<byte> GetUnderlying() { return std::span<byte>(underlying_, size_); }

        std::span<catalog::col_oid_t> GetColumnIds() { return std::span<catalog::col_oid_t>(column_ids_, column_count_); }

        byte *Access(u32 idx) { return underlying_ + offsets_[idx]; }

        u32 GetSize() { return size_; }

        u32 GetCount() { return column_count_; }

        /**
         * Iterator should be seperate class
         */
        void InitIterator()
        {
            iterator_data_ptr_ = data_;
            iterator_idx_ = 0;
        }

        void PushBack(std::span<byte> new_data)
        {
            std::memcpy(iterator_data_ptr_, new_data.data(), new_data.size());
            offsets_[iterator_idx_++] = iterator_data_ptr_ - underlying_;
            iterator_data_ptr_ += new_data.size();
        }

        template <typename T>
        void PushBack(T new_data)
        {
            *(T *)iterator_data_ptr_ = new_data;
            offsets_[iterator_idx_++] = iterator_data_ptr_ - underlying_;
            iterator_data_ptr_ += sizeof(new_data);
        }

        byte *Next()
        {
            return underlying_ + offsets_[iterator_idx_++];
        }
    };
}
#pragma once

#include "access/access_common.hpp"
#include "catalog/catalog_common.hpp"

#include <vector>
#include <span>

namespace db7::access
{
    class Vector
    {
    private:
        type_id type_;
        u32 type_size_;
        byte *data_;

    public:
        Vector() : type_(type_id::BOOLEAN), type_size_(0), data_(nullptr) {}

        Vector(type_id type, u32 size, byte *data)
            : type_(type), type_size_(size), data_(data) {}

        u32 GetSize() const { return type_size_; }
        byte *GetData() const { return data_; }
        void SetData(byte *data) { data_ = data; }
        type_id GetType() const { return type_; }
    };

    class DataChunk
    {
    private:
        std::vector<catalog::col_oid_t> column_ids_;
        /**
         * number of elements in each Vector in data
         */
        u32 vec_count_;
        /**
         * @note When inserting columns should be ordered as schema.
         */
        std::vector<Vector> data_;
        /**
         * sum of all vector sizes from data_
         */
        u32 total_space_;

    public:
        DataChunk(u32 col_count, u32 vec_count)
            : vec_count_(vec_count), total_space_(0)
        {
            data_.reserve(col_count);
            column_ids_.reserve(col_count);
        }

        u32 GetCount() const { return vec_count_; }

        u32 GetColumnCount() const { return data_.size(); }

        u32 GetTotalSpace() { return total_space_; }

        std::span<byte> GetVectorByIdx(u32 idx) { return std::span(data_[idx].GetData(), data_[idx].GetSize()); }

        Vector GetVectorByIdx2(u32 idx) { return data_[idx]; }

        std::span<byte> GetColumnsRaw() { return std::span(reinterpret_cast<byte *>(column_ids_.data()), column_ids_.size()); }

        u32 GetRowSize()
        {
            u32 result = 0;
            for (auto &vec : data_)
            {
                result += vec.GetSize();
            }
            return result;
        }

        /**
         * @note Unsafe operation make sure u know what ur doing
         *
         * This needs to be in column_ids_ order exacly
         */
        void Set(Vector vec)
        {
            data_.emplace_back(vec);
            total_space_ += vec.GetSize();
        }

        void Set(std::vector<catalog::col_oid_t> column_ids)
        {
            column_ids_ = std::move(column_ids);
        }
    };
}
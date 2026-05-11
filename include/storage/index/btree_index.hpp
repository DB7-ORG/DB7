#pragma once

#include "storage/index/index.hpp"
#include "storage/storage_common.hpp"
#include "storage/page.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"

#include <vector>

namespace db7::storage
{
    using T = u64;
    using R = u64;

    struct BtreeHeader
    {
        u64 rlink;
        u32 count;
        u8 level;
        T max_val;

        BtreeHeader(u64 rlink, u32 count, u8 level, T max_val)
            : rlink(rlink), count(count), level(level), max_val(max_val) {}
    };

    constexpr u64 MAX_COUNT = (PAGE_SIZE - sizeof(BtreeHeader)) / sizeof(T);
    constexpr u64 REF_OFFSET = MAX_COUNT * sizeof(T) + sizeof(BtreeHeader); // TODO add alignment

    class BTreeIndex : public Index
    {
    private:
        BufferPool *buffer_pool_;
        page_id root_id_{0};
        table_id tbl_id_{0}; // TODO
        u32 max_count_{MAX_COUNT};

        static constexpr u64 UNDEFINED = 0;

        Page *DropToLevel(std::vector<page_id> &state, T key, u8 drop_level = 0);
        Page *GetNode(PageIdentifier id_);
        void ReleasePage(Page *page);
        bool InsertInternal(std::vector<page_id> &state, Page *page, T key, R value);
        Page *ReserveNode(table_id id, page_id &pid);
        void Split(BtreeHeader *header, byte *data);

    public:
        BTreeIndex(BufferPool *buffer_pool)
            : buffer_pool_(buffer_pool) {}

        bool Insert(T key, R value);
        bool Delete(/* ... */) = 0;
        void ScanKey(/* ... */) = 0;
    };
}
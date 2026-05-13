#pragma once

#include "storage/index/index.hpp"
#include "storage/storage_common.hpp"
#include "storage/page.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/align_util.hpp"

#include <vector>
#include <atomic>
#include <mutex>

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

    constexpr u64 BTREE_HEADER_SIZE = sizeof(BtreeHeader);
    constexpr u64 KEY_OFFSET = shared::AlignUp(BTREE_HEADER_SIZE, (u64)sizeof(T));

    constexpr u64 PAD_KEY_REF_INTER = sizeof(page_id) - 1;
    constexpr u64 MAX_COUNT_INTER = (PAGE_SIZE - KEY_OFFSET - PAD_KEY_REF_INTER) / (sizeof(T) + sizeof(page_id));
    constexpr u64 REF_OFFSET_INTER = shared::AlignUp(KEY_OFFSET + MAX_COUNT_INTER * sizeof(T), (u64)sizeof(page_id));

    constexpr u64 PAD_KEY_REF_LEAF = sizeof(R) - 1;
    constexpr u64 MAX_COUNT_LEAF = (PAGE_SIZE - KEY_OFFSET - PAD_KEY_REF_LEAF) / (sizeof(T) + sizeof(R));
    constexpr u64 REF_OFFSET_LEAF = shared::AlignUp(KEY_OFFSET + MAX_COUNT_LEAF * sizeof(T), (u64)sizeof(R));

    enum class LockMode
    {
        None,
        Read,
        Write
    };

    class BTreeIndex : public Index
    {
    private: // TODO seperate cache lines
        std::mutex root_mtx_;
        std::atomic<page_id> root_id_;
        BufferPool *buffer_pool_;
        table_id tbl_id_;

        Page *DropToLevel(std::vector<page_id> *state, T key);
        void DropToLevel(std::vector<page_id> *state, T key, u8 drop_level);
        template <LockMode Mode>
        Page *GetNode(PageIdentifier id_);
        template <LockMode Mode>
        void ReleasePage(Page *page);
        bool InsertInternal(std::vector<page_id> *state, Page *page, T key, R value);
        bool PropagateInsert(std::vector<page_id> *state, T key, page_id value);
        Page *ReserveNode(table_id id);
        T SplitLeaf(BtreeHeader *header, byte *data, page_id &new_pid, T key, R value);
        T SplitInter(BtreeHeader *header, byte *data, page_id &new_pid, T key, R value);
        page_id GetRoot();
        void GoRight(Page *&page, BtreeHeader *&header, T key);
        void CreateNewRoot(u8 level, T key, page_id pid, page_id new_pid);
        Page *InternalGet(T key);

    public:
        static constexpr u64 UNDEFINED = 0;

        BTreeIndex(BufferPool *buffer_pool, table_id tbl_id);
        ~BTreeIndex() = default;

        bool Insert(T key, R value);
        bool Delete(/* ... */) override { return false; }
        u64 Get(u64 key);
    };
}
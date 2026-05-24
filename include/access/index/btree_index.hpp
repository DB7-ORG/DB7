#pragma once

#include "access/index/index.hpp"
#include "storage/storage_common.hpp"
#include "storage/page.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/align_util.hpp"
#include "access/index/btree_number_layout_inter.hpp"
#include "access/index/btree_number_layout_leaf.hpp"
#include "access/index/btree_header.hpp"

#include <vector>
#include <atomic>
#include <mutex>

namespace db7::access
{
    using T = u64;
    using R = u64;

    constexpr u64 MAX_OPTIMISTIC_TRIES = 1;

    enum class LockMode
    {
        None,
        Optimistic,
        Read,
        Write
    };

    class BTreeIndex : public Index
    {
    private: // TODO seperate cache lines
        std::mutex root_mtx_;
        std::atomic<page_id> root_id_;
        storage::BufferPool *buffer_pool_;
        storage::DiskManagerAsync *disk_mng_;
        table_id tbl_id_;

        using Layout = BtreeNumberLayoutIntermediate<T>;
        static constexpr T UNDEFINED = std::numeric_limits<T>::max();

        BtreeNumberLayoutIntermediate<T> layout_inter_;
        BtreeNumberLayoutLeaf<T> layout_leaf_;

        storage::Page *ReserveNode(table_id id);
        template <LockMode Mode>
        storage::Page *GetNode(storage::PageIdentifier id_);
        template <LockMode Mode>
        void ReleasePage(storage::Page *page);

        void CreateNewRoot(u8 level, T key, page_id pid, page_id new_pid);
        T SplitLeaf(byte *data, page_id &new_pid, T key, R value);
        T SplitInter(byte *data, page_id &new_pid, T key, page_id value);
        void GoRight(storage::Page *&page, BtreeHeader<T> *&header, T key);
        page_id GetRoot();

        storage::Page *DropToLevel(T key);
        void DropToLevel(T key, u8 drop_level);
        bool InsertInternal(storage::Page *page, T key, R value);
        bool PropagateInsert(T key, page_id value);
        R InternalGet(T key);

    public:
        BTreeIndex(storage::BufferPool *buffer_pool, storage::DiskManagerAsync *disk_mng, table_id tbl_id);
        ~BTreeIndex() = default;

        bool Insert(T key, R value);
        bool Delete(/* ... */) override { return false; }
        R Get(T key);
    };
}
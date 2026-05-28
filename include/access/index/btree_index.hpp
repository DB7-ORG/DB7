#pragma once

#include "access/index/index.hpp"
#include "storage/storage_common.hpp"
#include "storage/page.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/align_util.hpp"
#include "access/index/fixed_layout/btree_number_layout_inter.hpp"
#include "access/index/fixed_layout/btree_number_layout_leaf.hpp"
#include "access/index/varlen_layout/btree_varlen_layout_inter.hpp"
#include "access/index/varlen_layout/btree_varlen_layout_leaf.hpp"
#include "shared/macro_helper.hpp"
#include "debug/printer.hpp"
#include "shared/thread_local_util.hpp"
#include "access/index/base_ly_header.hpp"

#include <vector>
#include <atomic>
#include <mutex>
#include <type_traits>

namespace db7::access
{
    // using T = Key;
    using R = u64;

    template <typename T>
    class BTreeIndex : public Index
    {
    private:
        std::mutex root_mtx_;
        std::atomic<page_id> root_id_;
        storage::BufferPool *buffer_pool_;
        storage::DiskManagerAsync *disk_mng_;
        table_id tbl_id_;

        static constexpr bool IS_VARLEN = std::is_same_v<T, Key>;
        using LeafLayout = std::conditional_t<IS_VARLEN, BtreeVarlenLayoutLeaf, BtreeNumberLayoutLeaf<T>>;
        using InterLayout = std::conditional_t<IS_VARLEN, BtreeVarlenLayoutIntermediate, BtreeNumberLayoutIntermediate<T>>;

        InterLayout layout_inter_;
        LeafLayout layout_leaf_;

        storage::Page *ReserveNode(table_id id_)
        {
            return buffer_pool_->Reserve(id_);
        }

        template <shared::LockMode Mode>
        storage::Page *GetNode(storage::PageIdentifier id_)
        {
            storage::Page *page = buffer_pool_->Pin(id_);
            page->WaitIO();
            shared::Lock<Mode>(page);
            return page;
        }

        template <shared::LockMode Mode>
        void ReleasePage(storage::Page *page)
        {
            shared::Unlock<Mode>(page);
            buffer_pool_->Unpin(page);
        }

        void CreateNewRoot(u8 level, T key, page_id pid, page_id new_pid)
        {
            storage::Page *new_root_page = ReserveNode(tbl_id_);

            byte *new_root_data = new_root_page->GetData();

            layout_inter_.InitHeader(new_root_data, 1, level + 1);

            layout_inter_.CreateRoot(new_root_data, key, pid, new_pid);

            root_id_.store(new_root_page->GetPageId());

            ReleasePage<shared::LockMode::None>(new_root_page);
        }

        T SplitLeaf(byte *data, page_id &new_pid, T key, R value)
        {
            auto *right_page = ReserveNode(tbl_id_);

            new_pid = right_page->GetPageId();

            byte *right_data = right_page->GetData();

            T sentinel = layout_leaf_.Split(data, right_data, new_pid, key, value);

            ReleasePage<shared::LockMode::None>(right_page);

            return sentinel;
        }

        T SplitInter(byte *data, page_id &new_pid, T key, page_id value)
        {
            auto *right_page = ReserveNode(tbl_id_);

            new_pid = right_page->GetPageId();

            byte *right_data = right_page->GetData();

            T sentinel = layout_inter_.Split(data, right_data, new_pid, key, value);

            ReleasePage<shared::LockMode::None>(right_page);

            return sentinel;
        }

        storage::Page *GoRight(storage::Page *page, T key)
        {
            byte *data = page->GetData();
            while (layout_inter_.HasSplit(data, key))
            {
                page_id pid = layout_leaf_.GetRLink(data);
                ReleasePage<shared::LockMode::Write>(page);
                page = GetNode<shared::LockMode::Write>(storage::PageIdentifier(tbl_id_, pid));
                data = page->GetData();
            };
            return page;
        }

        page_id GetRoot()
        {
            return root_id_.load();
        }

        storage::Page *DropToLevel(T key)
        {
            page_id pid = GetRoot();
            do
            {
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

                storage::Page *page = GetNode<shared::LockMode::None>(storage::PageIdentifier(tbl_id_, pid));
                auto *data = page->GetData();
            retry:
                constexpr shared::LockMode LM = shared::LockMode::Optimistic;
                shared::Lock<LM>(page);

                if (GetLevel(data) <= 0)
                {
                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    return page;
                }
                else if (layout_inter_.HasSplit(data, key))
                {
                    page_id new_pid = layout_inter_.GetRLink(data);

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    pid = new_pid;
                }
                else
                {
                    page_id new_pid = layout_inter_.Get(data, GetCount(data), key);

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    shared::TlState::Push(pid); // TODO should probably store a pointer and keep pages pinned
                    pid = new_pid;
                }

                ReleasePage<shared::LockMode::None>(page);
            } while (true);

            DB7_UNREACHABLE();
            return nullptr;
        }

        void DropToLevel(T key, u8 drop_level)
        {
            page_id pid = GetRoot();
            do
            {
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

                storage::Page *page = GetNode<shared::LockMode::None>(storage::PageIdentifier(tbl_id_, pid));
                auto *data = page->GetData();
            retry:
                constexpr shared::LockMode LM = shared::LockMode::Optimistic;
                shared::Lock<LM>(page);

                if (GetLevel(data) <= drop_level)
                {
                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    shared::TlState::Push(pid);

                    return;
                }
                else if (layout_inter_.HasSplit(data, key))
                {
                    page_id new_pid = layout_inter_.GetRLink(data);

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    pid = new_pid;
                }
                else
                {
                    page_id new_pid = layout_inter_.Get(data, GetCount(data), key);

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    shared::TlState::Push(pid); // TODO should probably store a pointer and keep pages pinned
                    pid = new_pid;
                }

                ReleasePage<shared::LockMode::None>(page);

            } while (true);

            DB7_UNREACHABLE();
        }

        bool InsertInternal(storage::Page *page, T key, R value)
        {
            shared::Lock<shared::LockMode::Write>(page);

            page = GoRight(page, key);
            byte *data = page->GetData();
            page_id pid = page->GetPageId();

            if (layout_leaf_.HasSpace(data, key))
            {
                layout_leaf_.Insert(data, key, value);
                ReleasePage<shared::LockMode::Write>(page);
            }
            else
            {
                page_id new_pid;
                T sentinel = SplitLeaf(data, new_pid, key, value); // TODO should not be a ref but a copy
                u8 level = GetLevel(data);
                ReleasePage<shared::LockMode::Write>(page);

                if (shared::TlState::IsEmpty())
                {
                    root_mtx_.lock();
                    if (GetRoot() == pid)
                    {
                        CreateNewRoot(level, sentinel, pid, new_pid);
                        root_mtx_.unlock();
                    }
                    else
                    {
                        root_mtx_.unlock();
                        DropToLevel(sentinel, level + 1);
                        return PropagateInsert(sentinel, new_pid);
                    }
                }
                else
                {
                    return PropagateInsert(sentinel, new_pid);
                }
            }

            return true;
        }

        bool PropagateInsert(T key, page_id value)
        {
            while (!shared::TlState::IsEmpty())
            {
                page_id pid = shared::TlState::Pop();
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

                constexpr shared::LockMode LM = shared::LockMode::Write;
                auto *page = GetNode<LM>(storage::PageIdentifier(tbl_id_, pid));
                page = GoRight(page, key);
                pid = page->GetPageId();

                auto *data = page->GetData();

                if (layout_inter_.HasSpace(data, key))
                {
                    layout_inter_.Insert(data, key, value);
                    ReleasePage<LM>(page);
                    break;
                }
                else
                {
                    page_id new_pid;
                    key = SplitInter(data, new_pid, key, value);
                    value = new_pid;
                    u8 level = GetLevel(data);
                    ReleasePage<LM>(page);

                    if (shared::TlState::IsEmpty())
                    {
                        root_mtx_.lock();
                        if (GetRoot() == pid)
                        {
                            CreateNewRoot(level, key, pid, new_pid);
                            root_mtx_.unlock();
                            break;
                        }
                        else
                        {
                            root_mtx_.unlock();
                            DropToLevel(key, level + 1);
                        }
                    }
                }
            }

            return true;
        }

        // TODO this should not exist i should first drop to level then here move right or whatever
        R InternalGet(T key)
        {
            page_id pid = GetRoot();
            do
            {
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

                storage::Page *page = GetNode<shared::LockMode::None>(storage::PageIdentifier(tbl_id_, pid));
                auto *data = page->GetData();
            retry:
                constexpr shared::LockMode LM = shared::LockMode::Optimistic;
                shared::Lock<LM>(page);

                // Check sentinel value
                if (layout_inter_.HasSplit(data, key))
                {
                    page_id new_pid = layout_leaf_.GetRLink(data); // for now this is leaf layout but this method should be changed to use drop to level

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    pid = new_pid;
                }
                else if (GetLevel(data) <= 0)
                {
                    R result = layout_leaf_.Get(data, GetCount(data), key);

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    ReleasePage<shared::LockMode::None>(page);
                    return result;
                }
                else
                {
                    page_id new_pid = layout_inter_.Get(data, GetCount(data), key);

                    if (!shared::Unlock<LM>(page))
                        goto retry;

                    pid = new_pid;
                }

                ReleasePage<shared::LockMode::None>(page);

            } while (true);

            DB7_UNREACHABLE();
            return layout_inter_.UNDEFINED;
        }

    public:
        BTreeIndex(storage::BufferPool *buffer_pool, storage::DiskManagerAsync *disk_mng, table_id tbl_id)
            : root_id_(1), buffer_pool_(buffer_pool), disk_mng_(disk_mng), tbl_id_(tbl_id),
              layout_inter_(), layout_leaf_()
        {
            if (!disk_mng_->CreateOpenFile(tbl_id_, 1))
            {
                // TODO handle error
                DB7_ASSERT(false, "Table could not be created/opened");
            }

            storage::Page *page = buffer_pool_->Reserve(tbl_id);

            layout_leaf_.InitHeader(page->GetData(), 0, 0);

            ReleasePage<shared::LockMode::None>(page);
        }

        ~BTreeIndex() = default;

        bool Insert(T key, R value)
        {
            shared::TlState::Clear();
            storage::Page *page = DropToLevel(key);
            return InsertInternal(page, key, value);
        }

        bool Delete(/* ... */) { return false; }

        R Get(T key)
        {
            shared::TlState::Clear();
            return InternalGet(key);
        }
    };
}
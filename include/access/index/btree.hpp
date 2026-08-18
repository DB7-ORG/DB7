#pragma once

#include "common.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "shared/thread_local_util.hpp"
#include "access/index/header.hpp"
#include "access/index/layouts/varlen/varlen_layout_intermediate.hpp"
#include "access/index/layouts/varlen/varlen_layout_leaf.hpp"

#include <atomic>
#include <mutex>

namespace db7::access
{
    template <typename ValTyp>
    class BTreeIndex
    {
        // TODO assert ValTyp is correct type

    private:
        std::mutex root_mtx_;
        std::atomic<page_id> root_id_;
        storage::BufferPool *buffer_pool_;
        storage::DiskManagerAsync *disk_mng_;
        table_id tbl_id_;
        BtreeVarlenLayoutIntermediate<page_id> layout_inter_;
        BtreeVarlenLayoutLeaf<ValTyp> layout_leaf_;
        std::vector<TypeSize> attrs_;
        u16 key_buffer_size_;

        static constexpr size_t ALLOC_CONST = 3;

        template <shared::LockMode Mode>
        storage::Page *GetNode(page_id id)
        {
            storage::Page *page = buffer_pool_->Pin({tbl_id_, id});
            page->WaitIO();
            shared::Lock<Mode>(page);
            return page;
        }

        template <shared::LockMode Mode>
        void ReleaseNode(storage::Page *page)
        {
            shared::Unlock<Mode>(page);
            buffer_pool_->Unpin(page);
        }

        storage::Page *ReserveNode()
        {
            auto page = buffer_pool_->Reserve(tbl_id_);
            shared::Lock<shared::LockMode::Write>(page);
            return page;
        }

        void CreateNewRoot(u8 level, Key key, page_id pid, page_id new_pid)
        {
            storage::Page *new_root_page = ReserveNode();

            byte *new_root_data = new_root_page->GetData();

            layout_inter_.InitHeader(new_root_data, 1, level + 1, new_root_page->GetPageId());

            layout_inter_.CreateRoot(new_root_data, key, pid, new_pid);

            root_id_.store(new_root_page->GetPageId());

            DB7_ASSERT(BaseLyHeader::GetLevel(new_root_data) == level + 1, "root level clobbered");

            ReleaseNode<shared::LockMode::Write>(new_root_page);
        }

        // copy/send sentinel up
        void CopyKey(std::unique_ptr<byte[]> *sentinel_copy, Key &sentinel)
        {
            *sentinel_copy = std::make_unique_for_overwrite<byte[]>(sentinel.len);
            std::memcpy(sentinel_copy->get(), sentinel.data, sentinel.len);
            sentinel.data = sentinel_copy->get();
        }

        ResultObj<Key> SplitLeaf(byte *data, page_id &new_pid, Key key, std::unique_ptr<byte[]> *sentinel_copy)
        {
            auto *right_page = ReserveNode();

            new_pid = right_page->GetPageId();

            byte *right_data = right_page->GetData();

            layout_leaf_.InitHeader(right_data, 0, 0, new_pid);

            ResultObj<Key> sentinel_obj = layout_leaf_.Split(data, right_data, new_pid, key);

            Key &sentinel = sentinel_obj.value;
            CopyKey(sentinel_copy, sentinel);

            ReleaseNode<shared::LockMode::Write>(right_page);

            return sentinel_obj;
        }

        Key SplitInter(byte *data, page_id &new_pid, Key key, page_id value, std::unique_ptr<byte[]> *sentinel_copy)
        {
            auto *right_page = ReserveNode();

            new_pid = right_page->GetPageId();

            byte *right_data = right_page->GetData();

            layout_inter_.InitHeader(right_data, 0, BaseLyHeader::GetLevel(data), new_pid);

            Key sentinel = layout_inter_.Split(data, right_data, new_pid, key, value);

            CopyKey(sentinel_copy, sentinel);

            ReleaseNode<shared::LockMode::Write>(right_page);

            return sentinel;
        }

        storage::Page *GoRightInter(storage::Page *page, Key key)
        {
            constexpr shared::LockMode LM = shared::LockMode::Write;
            do
            {
                auto *data = page->GetData();

                if (!layout_inter_.HasSplit(data, key))
                {
                    return page;
                }
                else
                {
                    page_id new_pid = layout_inter_.GetRLink(data);

                    ReleaseNode<LM>(page);

                    page = GetNode<LM>(new_pid);
                }
            } while (true);

            DB7_UNREACHABLE();
        }

        storage::Page *GoRightLeaf(storage::Page *page, Key key)
        {
            constexpr shared::LockMode LM = shared::LockMode::Write;
            do
            {
                auto *data = page->GetData();

                if (!layout_leaf_.HasSplit(data, key))
                {
                    return page;
                }
                else
                {
                    page_id new_pid = layout_leaf_.GetRLink(data);

                    ReleaseNode<LM>(page);

                    page = GetNode<LM>(new_pid);
                }
            } while (true);

            DB7_UNREACHABLE();
        }

        page_id GetRoot()
        {
            return root_id_.load();
        }

        storage::Page *DropToLevel(Key key)
        {
            page_id pid = GetRoot();
            DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

            storage::Page *page = GetNode<shared::LockMode::None>(pid);
            auto *data = page->GetData();

            do
            {
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

                constexpr shared::LockMode LM = shared::LockMode::Optimistic;
                shared::Lock<LM>(page);

                u8 level = BaseLyHeader::GetLevel(data);
                if (level <= 0)
                {
                    if (!shared::Unlock<LM>(page))
                        continue;

                    return page;
                }
                else if (layout_inter_.HasSplit(data, key))
                {
                    page_id new_pid = layout_inter_.GetRLink(data);

                    if (!shared::Unlock<LM>(page))
                        continue;

                    pid = new_pid;
                }
                else
                {
                    page_id new_pid = layout_inter_.Get(data, BaseLyHeader::GetCount(data), key);

                    if (!shared::Unlock<LM>(page))
                        continue;

                    shared::TlState::Push(pid); // TODO should probably store a pointer and keep pages pinned
                    pid = new_pid;
                    level--;
                }

                /* Unlock prev page */
                ReleaseNode<shared::LockMode::None>(page);

                /* Fetch a new page page */
                page = GetNode<shared::LockMode::None>(pid);
                data = page->GetData();

            } while (true);

            DB7_UNREACHABLE();

            return nullptr;
        }

        void DropToLevel(Key key, u8 drop_level)
        {
            page_id pid = GetRoot();
            DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

            storage::Page *page = GetNode<shared::LockMode::None>(pid);
            auto *data = page->GetData();
            u8 level = BaseLyHeader::GetLevel(data);

            do
            {
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");
                DB7_ASSERT(level == BaseLyHeader::GetLevel(data), "invalid level node");

                constexpr shared::LockMode LM = shared::LockMode::Optimistic;
                shared::Lock<LM>(page);

                if (level <= drop_level)
                {
                    if (!shared::Unlock<LM>(page))
                        continue;

                    shared::TlState::Push(pid);

                    return;
                }
                else if (layout_inter_.HasSplit(data, key))
                {
                    page_id new_pid = layout_inter_.GetRLink(data);

                    if (!shared::Unlock<LM>(page))
                        continue;

                    pid = new_pid;
                }
                else
                {
                    page_id new_pid = layout_inter_.Get(data, BaseLyHeader::GetCount(data), key);

                    if (!shared::Unlock<LM>(page))
                        continue;

                    shared::TlState::Push(pid); // TODO should probably store a pointer and keep pages pinned
                    pid = new_pid;
                    level--;
                }

                /* Unlock prev page */
                ReleaseNode<shared::LockMode::None>(page);

                /* Fetch a new page page */
                page = GetNode<shared::LockMode::None>(pid);
                data = page->GetData();
            } while (true);

            DB7_UNREACHABLE();
        }

        ResultObj<void> InsertInternal(storage::Page *page, Key key)
        {
            shared::Lock<shared::LockMode::Write>(page);

            page = GoRightLeaf(page, key);
            byte *data = page->GetData();
            page_id pid = page->GetPageId();

            if (layout_leaf_.HasSpace(data, key))
            {
                auto result = layout_leaf_.Insert(data, key);
                ReleaseNode<shared::LockMode::Write>(page);
                return result;
            }
            else
            {
                page_id new_pid;
                // NOTE: sentinel_unique just holds the buffer from sentinel key
                // so i dont forget to free it
                std::unique_ptr<byte[]> sentinel_unique;
                auto split_result = SplitLeaf(data, new_pid, key, &sentinel_unique);
                Key sentinel = split_result.value;
                u8 level = BaseLyHeader::GetLevel(data);
                ReleaseNode<shared::LockMode::Write>(page);

                if (!split_result.success)
                    return ResultObj<void>::Fail(split_result.message);

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

            return ResultObj<void>::Ok();
        }

        template <shared::LockMode Mode>
        void FreePages(std::vector<storage::Page *> &visited)
        {
            for (auto *ptr : visited)
            {
                ReleaseNode<Mode>(ptr);
            }
        }

        ResultObj<void> InsertInternalUnique(storage::Page *page, Key key, std::vector<storage::Page *> &visited)
        {
            constexpr shared::LockMode LM = shared::LockMode::Write;

            byte *data = page->GetData();
            page_id pid = page->GetPageId();

            if (layout_leaf_.HasSpace(data, key))
            {
                layout_leaf_.Insert(data, key);

                FreePages<LM>(visited);
            }
            else
            {
                page_id new_pid;
                // NOTE: sentinel_unique just holds the buffer from sentinel key
                // so i dont forget to free it
                std::unique_ptr<byte[]> sentinel_unique;
                auto split_result = SplitLeaf(data, new_pid, key, &sentinel_unique);
                Key sentinel = split_result.value;
                u8 level = BaseLyHeader::GetLevel(data);

                FreePages<LM>(visited);

                if (!split_result.success)
                    return ResultObj<void>::Fail(split_result.message);

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

            return ResultObj<void>::Ok();
        }

        ResultObj<void> PropagateInsert(Key key, page_id value)
        {
            while (!shared::TlState::IsEmpty())
            {
                page_id pid = shared::TlState::Pop();
                DB7_ASSERT(pid != std::numeric_limits<page_id>::max(), "invalid pid");

                constexpr shared::LockMode LM = shared::LockMode::Write;
                auto *page = GetNode<LM>(pid);
                page = GoRightInter(page, key);
                pid = page->GetPageId();

                auto *data = page->GetData();

                if (layout_inter_.HasSpace(data, key))
                {
                    layout_inter_.Insert(data, key, value);
                    ReleaseNode<LM>(page);
                    break;
                }
                else
                {
                    page_id new_pid;

                    // NOTE:
                    // sentinel_unique just holds the buffer from sentinel key
                    // so i dont forget to free it
                    std::unique_ptr<byte[]> sentinel_unique;
                    Key sentinel = SplitInter(data, new_pid, key, value, &sentinel_unique);

                    value = new_pid;
                    u8 level = BaseLyHeader::GetLevel(data);
                    ReleaseNode<LM>(page);

                    if (shared::TlState::IsEmpty())
                    {
                        root_mtx_.lock();
                        if (GetRoot() == pid)
                        {
                            CreateNewRoot(level, sentinel, pid, new_pid);
                            root_mtx_.unlock();
                            break;
                        }
                        else
                        {
                            root_mtx_.unlock();
                            DropToLevel(sentinel, level + 1);
                        }
                    }

                    key = sentinel;
                }
            }

            return ResultObj<void>::Ok();
        }

        ResultObj<void> InternalGet(storage::Page *page, Key key, shared::VectorValues<ValTyp> &results)
        {
            DB7_ASSERT(page->GetPageId() != std::numeric_limits<page_id>::max(), "invalid pid");
            auto *data = page->GetData();

            do
            {
                DB7_ASSERT(page->GetPageId() != std::numeric_limits<page_id>::max(), "invalid pid");

                constexpr shared::LockMode LM = shared::LockMode::Optimistic;
                shared::Lock<LM>(page);

                /* Check sentinel value */
                if (layout_leaf_.HasSplit(data, key))
                {
                    page_id new_pid = layout_leaf_.GetRLink(data);

                    if (!shared::Unlock<LM>(page))
                        continue;

                    ReleaseNode<shared::LockMode::None>(page);
                    page = GetNode<shared::LockMode::None>(new_pid);
                    data = page->GetData();
                }
                else
                {
                    auto tmp_results = shared::VectorValues<ValTyp>();
                    auto result = layout_leaf_.Get(data, BaseLyHeader::GetCount(data), key, tmp_results);

                    if (!shared::Unlock<LM>(page))
                        continue;

                    results.vec.insert(results.vec.end(), tmp_results.vec.begin(), tmp_results.vec.end());

                    if (tmp_results.proceed)
                    {
                        page_id new_pid = layout_leaf_.GetRLink(data);
                        ReleaseNode<shared::LockMode::None>(page);

                        if (new_pid == std::numeric_limits<page_id>::max())
                        {
                            return result;
                        }

                        page = GetNode<shared::LockMode::None>(new_pid);
                        data = page->GetData();
                    }
                    else
                    {
                        ReleaseNode<shared::LockMode::None>(page);
                        return result;
                    }
                }

            } while (true);

            DB7_UNREACHABLE();
        }

        ResultObj<void> DeleteInternal(storage::Page *page, Key key)
        {
            shared::Lock<shared::LockMode::Write>(page);

            page = GoRightLeaf(page, key);

            byte *data = page->GetData();

            auto result = layout_leaf_.Delete(data, key);

            ReleaseNode<shared::LockMode::Write>(page);

            return result;
        }

    public:
        BTreeIndex(
            storage::BufferPool *buffer_pool,
            storage::DiskManagerAsync *disk_mng,
            table_id tbl_id,
            std::vector<TypeSize> attr)
            : buffer_pool_(buffer_pool),
              disk_mng_(disk_mng),
              tbl_id_(tbl_id),
              layout_inter_(),
              layout_leaf_(),
              attrs_(std::move(attr))
        {

            if (!disk_mng_->CreateOpenFile(tbl_id_, 1))
            {
                throw IO_EXCEPTION("IO exception could not open file");
            }

            key_buffer_size_ = KeyNormEncoder::MaxKeyLen(attrs_);

            storage::Page *page = ReserveNode();

            layout_leaf_.InitHeader(page->GetData(), 0, 0, page->GetPageId());
            root_id_.store(page->GetPageId());

            ReleaseNode<shared::LockMode::Write>(page);
        }

        ~BTreeIndex() = default;

        ResultObj<void> Insert(DataChunk *chunk, ValTyp value)
        {
            auto ptr = std::make_unique_for_overwrite<byte[]>(key_buffer_size_); // TODO if i ever get larger strings ill need to change this
            Key key = access::KeyNormEncoder::BuildKey(ptr.get(), chunk, value, attrs_);
            shared::TlState::Clear();
            storage::Page *page = DropToLevel(key);
            return InsertInternal(page, key);
        }

        ResultObj<void> InsertUnique(transaction::TransactionContext *txn, DataChunk *chunk, ValTyp value)
        {

            auto ptr = std::make_unique_for_overwrite<byte[]>(key_buffer_size_); // TODO if i ever get larger strings ill need to change this
            Key key = access::KeyNormEncoder::BuildKey(ptr.get(), chunk, 0, attrs_);

            shared::TlState::Clear();

            constexpr shared::LockMode LM = shared::LockMode::Write;

            storage::Page *page = DropToLevel(key);
            shared::Lock<LM>(page);
            page = GoRightLeaf(page, key);

            byte *value_start = key.data + key.len - sizeof(ValTyp);
            access::KeyNormEncoder::EncodeUnsigned(value_start, value);

            storage::Page *insert_page = nullptr;

            std::vector<storage::Page *> visited;

            int idx = -1;
            do
            {
                byte *page_data = page->GetData();

                /**
                 * Save the visited pages so we can later release locks.
                 * 'Latch crabbing' is nessesary to prevent the race where
                 * 2 transactions want to insert the same key.
                 */
                visited.push_back(page);

                /**
                 * Checks mvcc of these tuples to determine whether duplicate entries
                 * exist in the tree. This is only necessary for unique indexes.
                 */
                auto res = layout_leaf_.CheckUnique(txn, page_data, key, idx);
                if (!res.success)
                {
                    FreePages<LM>(visited);
                    return ResultObj<void>::Fail(res.message);
                }

                /**
                 * Saves the page where we need to insert key if no conflict exists
                 */
                if (insert_page == nullptr && !layout_leaf_.HasSplit(page_data, key))
                {
                    insert_page = page;
                }

                if (!res.value)
                {
                    break;
                }

                idx = 0;
                page_id new_pid = layout_leaf_.GetRLink(page_data);
                page = GetNode<LM>(new_pid);
            } while (true);

            /* We can freely insert since there is no conflict */
            return InsertInternalUnique(insert_page, key, visited);
        }

        /**
         * The api needs to know value also since the tree can store
         * multiple copies of the same key on different locations in teh heap
         */
        ResultObj<void> Delete(DataChunk *chunk, ValTyp value)
        {
            auto ptr = std::make_unique_for_overwrite<byte[]>(key_buffer_size_);
            Key key = access::KeyNormEncoder::BuildKey(ptr.get(), chunk, value, attrs_);
            shared::TlState::Clear();
            storage::Page *page = DropToLevel(key);
            return DeleteInternal(page, key);
        }

        ResultObj<void> Get(DataChunk *chunk, shared::VectorValues<ValTyp> &results)
        {
            auto ptr = std::make_unique_for_overwrite<byte[]>(key_buffer_size_);
            Key key = access::KeyNormEncoder::BuildKey(ptr.get(), chunk, 0, attrs_);
            shared::TlState::Clear();
            auto *page = DropToLevel(key);
            return InternalGet(page, key, results);
        }
    };
}
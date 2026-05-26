#include "access/index/btree_index.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>

namespace db7::access
{
    thread_local u64 tl_version = 0;
    thread_local u32 tl_tries = 0;
    thread_local page_id tl_state_buf[16];
    thread_local u32 tl_state_size = 0;

    void TlClearLocals()
    {
        tl_version = 0;
        tl_tries = 0;
        tl_state_size = 0;
    }

    void TlStatePush(page_id id)
    {
        DB7_ASSERT(tl_state_size < 16, "Invaliid size tried to push to full stack");
        tl_state_buf[tl_state_size++] = id;
    }

    page_id TlStateGet()
    {
        DB7_ASSERT(tl_state_size >= 1, "Invaliid size tried to pop from empty stack");
        return tl_state_buf[--tl_state_size];
    }

    bool TlStateIsEmpty()
    {
        return tl_state_size == 0;
    }

    /**
     * There are several ways to lock a page
     * - Exclusive (LockMode::Write)
     *      Taken by writers, so only one writer is allowed in a critical section
     * - Shared (LockMode::Read)
     *      Taken by readers usually in case of high contention where a thread
     *      tried several times (MAX_OPTIMISTIC_TRIES) to optimistically read a node.
     * - Optimistic (LockMode::Optimistic)
     *      Only taken by readers or writers while they are descending down the tree.
     *      Does not aquire any locks just reads a version field and hopes that version doesnt change.
     *      In case of reading a version while someone is holding an exclusive lock or at the end of
     *      the node scan it reads some different version then it increments tl_tries and tries again.
     *      If it fails to read optimistically for couple of runs then there is a fallback to SharedLock.
     * - None (LockMode::None)
     *      No locks taken at all. Noop.
     *      Useful when combined w reserve page, where no locks are taken.
     */
    template <LockMode Mode>
    void Lock(storage::Page *page)
    {
        if constexpr (Mode == LockMode::Write)
            page->WDataLock();
        else if constexpr (Mode == LockMode::Read)
            page->RDataLock();
        else if constexpr (Mode == LockMode::Optimistic)
        {
            while (tl_tries < MAX_OPTIMISTIC_TRIES && !page->ReadVersion(tl_version))
            {
                tl_tries++;
                // pause
            }

            if (tl_tries >= MAX_OPTIMISTIC_TRIES)
            {
                page->RDataLock();
            }
        }
        else if constexpr (Mode == LockMode::None)
            return;
        else
            DB7_UNREACHABLE();
    }

    template <LockMode Mode>
    bool Unlock(storage::Page *page)
    {
        if constexpr (Mode == LockMode::Write)
            page->WDataUnlock();
        else if constexpr (Mode == LockMode::Read)
            page->RDataUnlock();
        else if constexpr (Mode == LockMode::Optimistic)
        {
            if (tl_tries >= MAX_OPTIMISTIC_TRIES)
            {
                page->RDataUnlock();
                return true;
            }

            if (!page->ValidateVersion(tl_version))
            {
                tl_tries++;
                return false;
            }
        }
        else if constexpr (Mode == LockMode::None)
            return true;
        else
            DB7_UNREACHABLE();

        return true;
    }

    template <LockMode Mode>
    storage::Page *BTreeIndex::GetNode(storage::PageIdentifier id_)
    {
        storage::Page *page = buffer_pool_->Pin(id_);
        page->WaitIO();
        Lock<Mode>(page);
        return page;
    }

    template <LockMode Mode>
    void BTreeIndex::ReleasePage(storage::Page *page)
    {
        Unlock<Mode>(page);
        buffer_pool_->Unpin(page);
    }

    storage::Page *BTreeIndex::ReserveNode(table_id id_)
    {
        return buffer_pool_->Reserve(id_);
    }

    BtreeHeader<T> *GetHeader(byte *data)
    {
        return reinterpret_cast<BtreeHeader<T> *>(data);
    }

    void WriteHeader(BtreeHeader<T> *header, u64 rlink, u32 count, u8 level, T max_val)
    {
        header->rlink = rlink;
        header->count = count;
        header->level = level;
        header->max_val = max_val;
    }

    void IncrementHeaderSize(BtreeHeader<T> *header)
    {
        header->count++;
    }

    page_id BTreeIndex::GetRoot()
    {
        return root_id_.load();
    }

    T BTreeIndex::SplitLeaf(byte *data, page_id &new_pid, T key, R value)
    {
        auto *right_page = ReserveNode(tbl_id_);

        new_pid = right_page->GetPageId();

        byte *right_data = right_page->GetData();

        T sentinel = layout_leaf_.Split(data, right_data, new_pid, key, value);

        ReleasePage<LockMode::None>(right_page);

        return sentinel;
    }

    T BTreeIndex::SplitInter(byte *data, page_id &new_pid, T key, page_id value)
    {
        auto *right_page = ReserveNode(tbl_id_);

        new_pid = right_page->GetPageId();

        byte *right_data = right_page->GetData();

        T sentinel = layout_inter_.Split(data, right_data, new_pid, key, value);

        ReleasePage<LockMode::None>(right_page);

        return sentinel;
    }

    storage::Page *BTreeIndex::DropToLevel(T key)
    {
        page_id pid = GetRoot();
        do
        {
            storage::Page *page = GetNode<LockMode::None>(storage::PageIdentifier(tbl_id_, pid));
        retry:
            constexpr LockMode LM = LockMode::Optimistic;
            Lock<LM>(page);

            BtreeHeader<T> *header = GetHeader(page->GetData());

            if (header->level <= 0)
            {
                if (!Unlock<LM>(page))
                    goto retry;

                return page;
            }
            else if (layout_inter_.HasSplit(header, key))
            {
                page_id new_pid = header->rlink;

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }
            else
            {
                auto *data = page->GetData();
                page_id new_pid = layout_inter_.Get(data, header->count, key);

                if (!Unlock<LM>(page))
                    goto retry;

                TlStatePush(pid); // TODO should probably store a pointer and keep pages pinned
                pid = new_pid;
            }

            ReleasePage<LockMode::None>(page);
        } while (true);

        DB7_UNREACHABLE();
        return nullptr;
    }

    void BTreeIndex::DropToLevel(T key, u8 drop_level)
    {
        page_id pid = GetRoot();
        do
        {
            storage::Page *page = GetNode<LockMode::None>(storage::PageIdentifier(tbl_id_, pid));
        retry:
            constexpr LockMode LM = LockMode::Optimistic;
            Lock<LM>(page);

            BtreeHeader<T> *header = GetHeader(page->GetData());

            if (header->level <= drop_level)
            {
                if (!Unlock<LM>(page))
                    goto retry;

                TlStatePush(pid);

                return;
            }
            else if (layout_inter_.HasSplit(header, key))
            {
                page_id new_pid = header->rlink;

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }
            else
            {
                auto *data = page->GetData();
                page_id new_pid = layout_inter_.Get(data, header->count, key);

                if (!Unlock<LM>(page))
                    goto retry;

                TlStatePush(pid); // TODO should probably store a pointer and keep pages pinned
                pid = new_pid;
            }

            ReleasePage<LockMode::None>(page);

        } while (true);

        DB7_UNREACHABLE();
    }

    void BTreeIndex::GoRight(storage::Page *&page, BtreeHeader<T> *&header, T key)
    {
        while (layout_inter_.HasSplit(header, key))
        {
            page_id pid = header->rlink;
            ReleasePage<LockMode::Write>(page);
            page = GetNode<LockMode::Write>(storage::PageIdentifier(tbl_id_, pid));
            header = GetHeader(page->GetData());
        };
    }

    void BTreeIndex::CreateNewRoot(u8 level, T key, page_id pid, page_id new_pid)
    {
        storage::Page *new_root_page = ReserveNode(tbl_id_);

        byte *new_root_data = new_root_page->GetData();

        auto *header = GetHeader(new_root_data);
        WriteHeader(header, UNDEFINED, 1, level + 1, UNDEFINED);

        layout_inter_.CreateRoot(new_root_data, key, pid, new_pid);

        root_id_.store(new_root_page->GetPageId());

        ReleasePage<LockMode::None>(new_root_page);
    }

    bool BTreeIndex::PropagateInsert(T key, page_id value)
    {
        while (!TlStateIsEmpty())
        {
            page_id pid = TlStateGet();

            constexpr LockMode LM = LockMode::Write;
            auto *page = GetNode<LM>(storage::PageIdentifier(tbl_id_, pid));
            BtreeHeader<T> *header = GetHeader(page->GetData());
            GoRight(page, header, key);
            pid = page->GetPageId();

            auto *data = page->GetData();
            if (layout_inter_.HasSpace(header, key))
            {
                layout_inter_.Insert(data, header->count, key, value);
                IncrementHeaderSize(header);
                ReleasePage<LM>(page);
                break;
            }
            else
            {
                page_id new_pid;
                key = SplitInter(data, new_pid, key, value);
                value = new_pid;
                u8 level = header->level;
                ReleasePage<LM>(page);

                if (TlStateIsEmpty())
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

    bool BTreeIndex::InsertInternal(storage::Page *page, T key, R value)
    {
        Lock<LockMode::Write>(page);

        BtreeHeader<T> *header = GetHeader(page->GetData());
        GoRight(page, header, key);
        byte *data = page->GetData();
        page_id pid = page->GetPageId();

        if (layout_leaf_.HasSpace(header, key))
        {
            layout_leaf_.Insert(data, header->count, key, value);
            IncrementHeaderSize(header);
            ReleasePage<LockMode::Write>(page);
        }
        else
        {
            page_id new_pid;
            T sentinel = SplitLeaf(data, new_pid, key, value);
            u8 level = header->level;
            ReleasePage<LockMode::Write>(page);

            if (TlStateIsEmpty())
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

    R BTreeIndex::InternalGet(T key)
    {
        page_id pid = GetRoot();
        do
        {
            storage::Page *page = GetNode<LockMode::None>(storage::PageIdentifier(tbl_id_, pid));
        retry:
            constexpr LockMode LM = LockMode::Optimistic;
            Lock<LM>(page);

            BtreeHeader<T> *header = GetHeader(page->GetData());

            if (layout_inter_.HasSplit(header, key))
            {
                page_id new_pid = header->rlink;

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }
            else if (header->level == 0)
            {
                byte *data = page->GetData();
                BtreeHeader<T> *header = GetHeader(data);
                R result = layout_leaf_.Get(data, header->count, key);

                if (!Unlock<LM>(page))
                    goto retry;

                ReleasePage<LockMode::None>(page);
                return result;
            }
            else
            {
                auto *data = page->GetData();
                page_id new_pid = layout_inter_.Get(data, header->count, key);

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }

            ReleasePage<LockMode::None>(page);

        } while (true);

        DB7_UNREACHABLE();
        return UNDEFINED;
    }

    BTreeIndex::BTreeIndex(storage::BufferPool *buffer_pool, storage::DiskManagerAsync *disk_mng, table_id tbl_id)
        : root_id_(1), buffer_pool_(buffer_pool), disk_mng_(disk_mng), tbl_id_(tbl_id),
          layout_inter_(sizeof(BtreeHeader<T>)), layout_leaf_(sizeof(BtreeHeader<T>))
    {
        if (!disk_mng_->CreateOpenFile(tbl_id_, 1))
        {
            // TODO handle error
            DB7_ASSERT(false, "Table could not be created/opened");
        }

        storage::Page *page = buffer_pool_->Reserve(tbl_id);
        auto *header = GetHeader(page->GetData());
        WriteHeader(header, UNDEFINED, 0, 0, UNDEFINED);
        ReleasePage<LockMode::None>(page);
    }

    bool BTreeIndex::Insert(T key, R value)
    {
        TlClearLocals();
        storage::Page *page = DropToLevel(key);
        return InsertInternal(page, key, value);
    }

    R BTreeIndex::Get(T key)
    {
        TlClearLocals();
        return InternalGet(key);
    }
}
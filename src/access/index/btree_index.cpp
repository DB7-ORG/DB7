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

    u32 FindPosition(const T *data, const u32 count, const T value)
    {
        DB7_ASSERT(count != 0, "zero count node");
        u32 lo = 0, hi = count;
        while (lo < hi)
        {
            u32 mid = lo + (hi - lo) / 2;
            if (data[mid] <= value)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    }

    R FindKeyValue(byte *data, const u32 count, const T value)
    {
        DB7_ASSERT(count != 0, "zero count node");
        T *arr = reinterpret_cast<T *>(data + KEY_OFFSET);
        u32 lo = 0, hi = count;
        while (lo < hi)
        {
            u32 mid = (lo + hi) / 2;
            if (arr[mid] < value)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo < count && arr[lo] == value)
            return reinterpret_cast<R *>(data + REF_OFFSET_LEAF)[lo];
        return BTreeIndex::UNDEFINED;
    }

    template <typename Typ>
    u32 CopyUpperHalf(Typ *from, Typ *to, u32 count, bool isLeaf)
    {
        u32 mid = (count + 1) / 2;
        u32 inc = isLeaf ? 0 : 1;
        std::memcpy(to, from + mid + inc, (count - mid) * sizeof(Typ));
        return mid;
    }

    template <typename Typ>
    void ShiftRightInsert(Typ *data, u32 count, u32 idx, Typ value)
    {
        std::memmove(data + idx + 1, data + idx, (count - idx) * sizeof(Typ));
        data[idx] = value;
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

    byte *OffsetHeader(byte *data)
    {
        return data + KEY_OFFSET;
    }

    BtreeHeader *GetHeader(byte *data)
    {
        return reinterpret_cast<BtreeHeader *>(data);
    }

    void WriteHeader(BtreeHeader *header, byte *data)
    {
        std::memcpy(data, header, sizeof(BtreeHeader));
    }

    page_id BTreeIndex::GetRoot()
    {
        return root_id_.load();
    }

    template <bool IsLeaf, typename Typ>
    void NodeInsert(byte *data, u32 count, T key, Typ value)
    {
        u32 idx = FindPosition((T *)OffsetHeader(data), count, key);
        ShiftRightInsert((T *)OffsetHeader(data), count, idx, key);
        if constexpr (IsLeaf)
            ShiftRightInsert((Typ *)(data + REF_OFFSET_LEAF), count, idx, value);
        else
            ShiftRightInsert((Typ *)(data + REF_OFFSET_INTER), count + 1, idx + 1, value);
    }

    void NodeInsertInter(byte *data, BtreeHeader *header, T key, page_id value)
    {
        NodeInsert<false>(data, header->count, key, value);
        header->count++;
        WriteHeader(header, data);
    }

    void NodeInsertLeaf(byte *data, BtreeHeader *header, T key, R value)
    {
        if (UNLIKELY(header->count == 0))
        {
            *(T *)(data + KEY_OFFSET) = key;
            *(R *)(data + REF_OFFSET_LEAF) = value;
        }
        else
        {
            NodeInsert<true>(data, header->count, key, value);
        }
        header->count++;
        WriteHeader(header, data);
    }

    T BTreeIndex::SplitLeaf(BtreeHeader *header, byte *data, page_id &new_pid, T key, R value)
    {
        auto *right_page = ReserveNode(tbl_id_);

        new_pid = right_page->GetPageId();

        byte *right_data = right_page->GetData();

        u32 mid = CopyUpperHalf((T *)OffsetHeader(data), (T *)OffsetHeader(right_data), header->count, true);

        CopyUpperHalf((R *)(data + REF_OFFSET_LEAF), (R *)(right_data + REF_OFFSET_LEAF), header->count, true);

        T sentinel = ((T *)OffsetHeader(right_data))[0];

        auto right_header = BtreeHeader(header->rlink, header->count - mid, header->level, header->max_val);

        auto new_header = BtreeHeader(new_pid, mid, header->level, sentinel);

        if (key < sentinel)
        {
            NodeInsert<true>(data, new_header.count, key, value);
            new_header.count++;
        }
        else
        {
            NodeInsert<true>(right_data, right_header.count, key, value);
            right_header.count++;
        }

        WriteHeader(&right_header, right_data);

        WriteHeader(&new_header, data);

        ReleasePage<LockMode::None>(right_page);

        return sentinel;
    }

    T BTreeIndex::SplitInter(BtreeHeader *header, byte *data, page_id &new_pid, T key, R value)
    {
        auto *right_page = ReserveNode(tbl_id_);

        new_pid = right_page->GetPageId();

        byte *right_data = right_page->GetData();

        u32 mid = CopyUpperHalf((T *)OffsetHeader(data), (T *)OffsetHeader(right_data), header->count, false);

        CopyUpperHalf((page_id *)(data + REF_OFFSET_INTER), (page_id *)(right_data + REF_OFFSET_INTER), header->count, false);

        T sentinel = ((T *)OffsetHeader(right_data))[0];

        auto right_header = BtreeHeader(header->rlink, header->count - mid, header->level, header->max_val);

        auto new_header = BtreeHeader(new_pid, mid, header->level, sentinel);

        if (key < sentinel)
        {
            NodeInsert<false>(data, new_header.count, key, value);
            new_header.count++;
        }
        else
        {
            NodeInsert<false>(right_data, right_header.count, key, value);
            right_header.count++;
        }

        WriteHeader(&right_header, right_data);

        WriteHeader(&new_header, data);

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

            BtreeHeader *header = GetHeader(page->GetData());

            if (header->level <= 0)
            {
                if (!Unlock<LM>(page))
                    goto retry;

                return page;
            }
            else if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                page_id new_pid = header->rlink;

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }
            else
            {
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                page_id new_pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];

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

            BtreeHeader *header = GetHeader(page->GetData());

            if (header->level <= drop_level)
            {
                if (!Unlock<LM>(page))
                    goto retry;

                TlStatePush(pid);

                return;
            }
            else if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                page_id new_pid = header->rlink;

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }
            else
            {
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                page_id new_pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];

                if (!Unlock<LM>(page))
                    goto retry;

                TlStatePush(pid); // TODO should probably store a pointer and keep pages pinned
                pid = new_pid;
            }

            ReleasePage<LockMode::None>(page);

        } while (true);

        DB7_UNREACHABLE();
    }

    void BTreeIndex::GoRight(storage::Page *&page, BtreeHeader *&header, T key)
    {
        while (header->max_val != UNDEFINED && key >= header->max_val)
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

        auto h = BtreeHeader(UNDEFINED, 1, level + 1, UNDEFINED);

        WriteHeader(&h, new_root_data);

        *(T *)(new_root_data + KEY_OFFSET) = key;

        *(page_id *)(new_root_data + REF_OFFSET_INTER) = pid;

        *(page_id *)(new_root_data + REF_OFFSET_INTER + sizeof(page_id)) = new_pid;

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
            BtreeHeader *header = GetHeader(page->GetData());
            GoRight(page, header, key);
            pid = page->GetPageId();

            auto *data = page->GetData();
            if (header->count < MAX_COUNT_INTER)
            {
                NodeInsertInter(data, header, key, value);
                ReleasePage<LM>(page);
                break;
            }
            else
            {
                page_id new_pid;
                key = SplitInter(header, data, new_pid, key, value);
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

        BtreeHeader *header = GetHeader(page->GetData());
        GoRight(page, header, key);
        byte *data = page->GetData();
        page_id pid = page->GetPageId();

        if (header->count < MAX_COUNT_LEAF)
        {
            NodeInsertLeaf(data, header, key, value);
            ReleasePage<LockMode::Write>(page);
        }
        else
        {
            page_id new_pid;
            T sentinel = SplitLeaf(header, data, new_pid, key, value);
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

            BtreeHeader *header = GetHeader(page->GetData());

            if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                page_id new_pid = header->rlink;

                if (!Unlock<LM>(page))
                    goto retry;

                pid = new_pid;
            }
            else if (header->level == 0)
            {
                byte *data = page->GetData();
                BtreeHeader *header = GetHeader(data);
                R result = FindKeyValue(data, header->count, key);

                if (!Unlock<LM>(page))
                    goto retry;

                ReleasePage<LockMode::None>(page);

                return result;
            }
            else
            {
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                page_id new_pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];

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
        : root_id_(1), buffer_pool_(buffer_pool), disk_mng_(disk_mng), tbl_id_(tbl_id)
    {
        storage::Page *page = buffer_pool_->Reserve(tbl_id);
        BtreeHeader header(UNDEFINED, 0, 0, UNDEFINED);
        WriteHeader(&header, page->GetData());
        ReleasePage<LockMode::None>(page);

        if (!disk_mng_->CreateOpenFile(tbl_id_, 1))
        {
            // TODO handle error
            DB7_ASSERT(false, "Table could not be created/opened");
        }
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
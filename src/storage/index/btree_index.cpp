#include "storage/index/btree_index.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>

namespace db7::storage
{
    u32 FindPosition(const T *data, const u32 count, const T value)
    {
        DB7_ASSERT(count != 0, "zero count node");
        T cur;
        u32 i = 0;
        do
        {
            cur = data[i];
            if (cur > value)
            {
                break;
            }
            i++;
        } while (i < count);
        return i;
    }

    R FindKeyValue(byte *data, const u32 count, const T value)
    {
        DB7_ASSERT(count != 0, "zero count node");
        T cur;
        u32 i = 0;
        T *arr = reinterpret_cast<T *>(data + KEY_OFFSET);
        do
        {
            cur = arr[i];
            if (cur >= value)
            {
                break;
            }
            i++;
        } while (i < count);

        if (cur == value)
        {
            return reinterpret_cast<R *>(data + REF_OFFSET_LEAF)[i];
        }

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

    template <LockMode Mode>
    void Lock(Page *page)
    {
        if constexpr (Mode == LockMode::Write)
            page->WDataLock();
        else if constexpr (Mode == LockMode::Read)
            page->RDataLock();
        else if constexpr (Mode == LockMode::None)
            page->GetId();
        else
            throw std::runtime_error("Invalid type");
    }

    template <LockMode Mode>
    void Unlock(Page *page)
    {
        if constexpr (Mode == LockMode::Write)
            page->WDataUnlock();
        else if constexpr (Mode == LockMode::Read)
            page->RDataUnlock();
        else if constexpr (Mode == LockMode::None)
            page->GetId(); // TODO should handle optimistic
        else
            throw std::runtime_error("Invalid type");
    }

    template <LockMode Mode>
    Page *BTreeIndex::GetNode(PageIdentifier id_)
    {
        Page *page = buffer_pool_->Pin(id_);
        page->WaitIO();
        Lock<Mode>(page);
        return page;
    }

    template <LockMode Mode>
    void BTreeIndex::ReleasePage(Page *page)
    {
        Unlock<Mode>(page);
        buffer_pool_->Unpin(page);
    }

    Page *BTreeIndex::ReserveNode(table_id id_)
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

    Page *BTreeIndex::DropToLevel(std::vector<page_id> *state, T key)
    {
        page_id pid = GetRoot();
        do
        {
            Page *page = GetNode<LockMode::Read>(PageIdentifier(tbl_id_, pid)); // TODO should be optimistic
            BtreeHeader *header = GetHeader(page->GetData());

            if (header->level <= 0)
            {
                Unlock<LockMode::Read>(page);
                return page;
            }
            else if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                pid = header->rlink;
            }
            else
            {
                state->push_back(pid); // TODO should probably store a pointer and keep pages pinned
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];
            }
            ReleasePage<LockMode::Read>(page);

        } while (true);

        DB7_UNREACHABLE();
        return nullptr;
    }

    void BTreeIndex::DropToLevel(std::vector<page_id> *state, T key, u8 drop_level)
    {
        page_id pid = GetRoot();
        do
        {
            Page *page = GetNode<LockMode::Read>(PageIdentifier(tbl_id_, pid)); // TODO should be optimistic
            BtreeHeader *header = GetHeader(page->GetData());

            if (header->level <= drop_level)
            {
                Unlock<LockMode::Read>(page);
                state->push_back(pid);
                return;
            }
            else if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                pid = header->rlink;
            }
            else
            {
                state->push_back(pid); // TODO should probably store a pointer and keep pages pinned
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];
            }
            ReleasePage<LockMode::Read>(page);

        } while (true);

        DB7_UNREACHABLE();
    }

    void BTreeIndex::GoRight(Page *&page, BtreeHeader *&header, T key)
    {
        while (header->max_val != UNDEFINED && key >= header->max_val)
        {
            page_id pid = header->rlink;
            ReleasePage<LockMode::Write>(page);
            page = GetNode<LockMode::Write>(PageIdentifier(tbl_id_, pid));
            header = GetHeader(page->GetData());
        };
    }

    void BTreeIndex::CreateNewRoot(u8 level, T key, page_id pid, page_id new_pid)
    {
        Page *new_root_page = ReserveNode(tbl_id_);

        byte *new_root_data = new_root_page->GetData();

        auto h = BtreeHeader(UNDEFINED, 1, level + 1, UNDEFINED);

        WriteHeader(&h, new_root_data);

        *(T *)(new_root_data + KEY_OFFSET) = key;

        *(page_id *)(new_root_data + REF_OFFSET_INTER) = pid;

        *(page_id *)(new_root_data + REF_OFFSET_INTER + sizeof(page_id)) = new_pid;

        root_id_.store(new_root_page->GetPageId());

        ReleasePage<LockMode::None>(new_root_page);
    }

    bool BTreeIndex::PropagateInsert(std::vector<page_id> *state, T key, page_id value)
    {
        while (!state->empty())
        {
            page_id pid = state->back();
            state->pop_back();

            auto *page = GetNode<LockMode::Write>(PageIdentifier(tbl_id_, pid));
            BtreeHeader *header = GetHeader(page->GetData());
            GoRight(page, header, key);
            pid = page->GetPageId();

            auto *data = page->GetData();
            if (header->count < MAX_COUNT_INTER)
            {
                NodeInsertInter(data, header, key, value);
                ReleasePage<LockMode::Write>(page);
                break;
            }
            else
            {
                page_id new_pid;
                key = SplitInter(header, data, new_pid, key, value);
                value = new_pid;
                u8 level = header->level;
                ReleasePage<LockMode::Write>(page);

                if (state->empty())
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
                        DropToLevel(state, key, level + 1);
                    }
                }
            }
        }

        return true;
    }

    bool BTreeIndex::InsertInternal(std::vector<page_id> *state, Page *page, T key, R value)
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

            if (state->empty())
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
                    DropToLevel(state, sentinel, level + 1);
                    return PropagateInsert(state, sentinel, new_pid);
                }
            }
            else
            {
                return PropagateInsert(state, sentinel, new_pid);
            }
        }

        return true;
    }

    Page *BTreeIndex::InternalGet(T key)
    {
        page_id pid = GetRoot();
        do
        {
            Page *page = GetNode<LockMode::Read>(PageIdentifier(tbl_id_, pid));
            BtreeHeader *header = GetHeader(page->GetData());

            if (header->max_val != UNDEFINED && key >= header->max_val)
            {
                pid = header->rlink;
            }
            else if (header->level == 0)
            {
                return page;
            }
            else
            {
                auto *data = page->GetData();
                u32 idx = FindPosition((T *)OffsetHeader(data), header->count, key);
                pid = reinterpret_cast<page_id *>(data + REF_OFFSET_INTER)[idx];
            }
            ReleasePage<LockMode::Read>(page);
        } while (true);

        DB7_ASSERT(false, "unreachable");
        return nullptr;
    }

    BTreeIndex::BTreeIndex(BufferPool *buffer_pool, table_id tbl_id)
        : root_id_(1), buffer_pool_(buffer_pool), tbl_id_(tbl_id)
    {
        Page *page = buffer_pool_->Reserve(tbl_id);
        BtreeHeader header(UNDEFINED, 0, 0, UNDEFINED);
        WriteHeader(&header, page->GetData());
        ReleasePage<LockMode::None>(page);
    }

    bool BTreeIndex::Insert(T key, R value)
    {
        std::vector<page_id> state;
        state.reserve(3);
        Page *page = DropToLevel(&state, key);
        return InsertInternal(&state, page, key, value);
    }

    R BTreeIndex::Get(T key)
    {
        Page *page = InternalGet(key);
        byte *data = page->GetData();
        BtreeHeader *header = GetHeader(data);
        R result = FindKeyValue(data, header->count, key);
        ReleasePage<LockMode::Read>(page);
        return result;
    }
}
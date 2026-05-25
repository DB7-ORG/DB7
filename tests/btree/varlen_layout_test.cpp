#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

#include "access/index/btree_varlen_layout_leaf.hpp"

namespace db7::access
{

    // Helper to make a Key from a string
    Key MakeKey(const std::string &s)
    {
        return Key{(u16)s.size(), (byte *)s.data()};
    }

    // Initialize a fresh page with zeroed memory and proper headers
    void InitPage(byte *page)
    {
        std::memset(page, 0, PAGE_SIZE);
        auto *header = reinterpret_cast<BtreeHeader<u32> *>(page);
        header->count = 0;
        header->level = 0;
        header->rlink = UINT64_MAX;
        header->max_val = UINT32_MAX;
        // VarlenHeader follows BtreeHeader
        // heap_size starts at 0
    }

    void TestInsertAndGet()
    {
        alignas(16) byte page[PAGE_SIZE];
        BtreeVarlenLayoutLeaf layout(sizeof(BtreeHeader<u32>));
        InitPage(page, layout);

        auto *header = reinterpret_cast<BtreeHeader<u32> *>(page);

        // Insert single key
        layout.Insert(page, header->count, MakeKey("hello"), 42);
        header->count++;

        // Should find it
        u64 val = layout.Get(page, header->count, MakeKey("hello"));
        assert(val == 42);

        // Should not find missing key
        val = layout.Get(page, header->count, MakeKey("world"));
        assert(val == std::numeric_limits<u64>::max());

        printf("TestInsertAndGet PASSED\n");
    }

    void TestMultipleInserts()
    {
        alignas(16) byte page[PAGE_SIZE];
        BtreeVarlenLayoutLeaf layout(sizeof(BtreeHeader<u32>));
        InitPage(page, layout);

        auto *header = reinterpret_cast<BtreeHeader<u32> *>(page);

        std::vector<std::string> keys = {"delta", "alpha", "charlie", "bravo", "echo"};

        for (u32 i = 0; i < keys.size(); i++)
        {
            layout.Insert(page, header->count, MakeKey(keys[i]), i * 10);
            header->count++;
        }

        // All keys should be retrievable with correct values
        for (u32 i = 0; i < keys.size(); i++)
        {
            u64 val = layout.Get(page, header->count, MakeKey(keys[i]));
            assert(val == i * 10);
        }

        printf("TestMultipleInserts PASSED\n");
    }

    void TestSortedOrder()
    {
        alignas(16) byte page[PAGE_SIZE];
        BtreeVarlenLayoutLeaf layout(sizeof(BtreeHeader<u32>));
        InitPage(page, layout);

        auto *header = reinterpret_cast<BtreeHeader<u32> *>(page);

        // Insert out of order
        std::vector<std::pair<std::string, u64>> entries = {
            {"zebra", 4}, {"apple", 1}, {"mango", 3}, {"banana", 2}};

        for (auto &[k, v] : entries)
        {
            layout.Insert(page, header->count, MakeKey(k), v);
            header->count++;
        }

        // All lookups work = binary search works = slots are sorted
        for (auto &[k, v] : entries)
        {
            assert(layout.Get(page, header->count, MakeKey(k)) == v);
        }

        printf("TestSortedOrder PASSED\n");
    }
    void TestHasSpace()
    {
        alignas(16) byte page[PAGE_SIZE];
        BtreeVarlenLayoutLeaf layout(sizeof(BtreeHeader<u32>));
        InitPage(page, layout);

        auto *header = reinterpret_cast<BtreeHeader<u32> *>(page);

        // Fill page until full
        u32 i = 0;
        while (layout.HasSpace(header, MakeKey("key_" + std::to_string(i))))
        {
            layout.Insert(page, header->count, MakeKey("key_" + std::to_string(i)), i);
            header->count++;
            i++;
        }

        assert(header->count > 0);
        printf("TestHasSpace PASSED - fit %u entries\n", header->count);
    }

    void TestSplit()
    {
        alignas(16) byte left_page[PAGE_SIZE];
        alignas(16) byte right_page[PAGE_SIZE];
        BtreeVarlenLayoutLeaf layout(sizeof(BtreeHeader<u32>));
        InitPage(left_page);
        InitPage(right_page);

        auto *left_header = reinterpret_cast<BtreeHeader<u32> *>(left_page);

        // Fill left page
        std::vector<std::string> keys;
        u32 i = 0;
        while (layout.HasSpace(left_header, MakeKey("key_" + std::to_string(i))))
        {
            std::string k = "key_" + std::to_string(i);
            layout.Insert(left_page, left_header->count, MakeKey(k), i);
            left_header->count++;
            keys.push_back(k);
            i++;
        }

        u32 total = left_header->count;
        std::string new_key = "key_split";

        layout.Split(left_page, right_page, /*new_pid=*/99, MakeKey(new_key), 999);

        auto *right_header = reinterpret_cast<BtreeHeader<u32> *>(right_page);

        // All keys should still be findable across both pages
        u32 found_left = 0, found_right = 0;
        for (auto &k : keys)
        {
            u64 v = layout.Get(left_page, left_header->count, MakeKey(k));
            if (v != std::numeric_limits<u64>::max())
            {
                found_left++;
                continue;
            }
            v = layout.Get(right_page, right_header->count, MakeKey(k));
            assert(v != std::numeric_limits<u64>::max());
            found_right++;
        }

        // New key should be somewhere
        u64 v = layout.Get(left_page, left_header->count, MakeKey(new_key));
        if (v == std::numeric_limits<u64>::max())
            v = layout.Get(right_page, right_header->count, MakeKey(new_key));
        assert(v == 999);

        assert(found_left + found_right == total);
        assert(left_header->rlink == 99);

        printf("TestSplit PASSED - left=%u right=%u\n", left_header->count, right_header->count);
    }
}

int main()
{
    db7::access::TestInsertAndGet();
    db7::access::TestMultipleInserts();
    db7::access::TestSortedOrder();
    db7::access::TestHasSpace();
    db7::access::TestSplit();
    printf("\nAll tests passed!\n");
    return 0;
}
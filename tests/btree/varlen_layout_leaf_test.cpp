#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

#include "access/index/varlen_layout/btree_varlen_layout_leaf.hpp"
#include "debug/printer.hpp"

namespace db7::access
{
    namespace
    {
        constexpr u64 UNDEFINED = UINT64_MAX;

        VarlenHeader *CastHeader(byte *data)
        {
            return reinterpret_cast<VarlenHeader *>(data);
        }

        // Helper to make a Key from a string
        Key MakeKey(const std::string &s)
        {
            return Key{(u16)s.size(), (byte *)s.data()};
        }

        // Initialize a fresh page with zeroed memory and proper headers
        void InitPage(byte *page)
        {
            std::memset(page, 0, DB7_PAGE_SIZE);
            auto *header = CastHeader(page);
            header->count = 0;
            header->level = 0;
            header->rlink = UNDEFINED;
            header->max_val = UNDEFINED;
            // VarlenHeader follows BtreeHeader
            // heap_size starts at 0
        }

        void TestInsertAndGet()
        {
            alignas(16) byte page[DB7_PAGE_SIZE];
            BtreeVarlenLayoutLeaf layout;
            InitPage(page);

            auto *header = CastHeader(page);

            // Insert single key
            layout.Insert(page, MakeKey("hello"), 42);

            // shared::PrintVarlenLayout(page);

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
            alignas(16) byte page[DB7_PAGE_SIZE];
            BtreeVarlenLayoutLeaf layout;
            InitPage(page);

            auto *header = CastHeader(page);
            auto *var_header = CastHeader(page);

            std::vector<std::string> keys = {"delta", "alpha", "charlie", "bravo", "echo"};

            u32 tot_len = 0;
            for (u32 i = 0; i < keys.size(); i++)
            {
                layout.Insert(page, MakeKey(keys[i]), i * 10);
                tot_len += keys[i].length();
            }

            // All keys should be retrievable with correct values
            for (u32 i = 0; i < keys.size(); i++)
            {
                u64 val = layout.Get(page, header->count, MakeKey(keys[i]));
                assert(val == i * 10);
            }

            assert(header->count == keys.size());
            assert(header->level == 0);
            assert(header->max_val == UNDEFINED);
            assert(header->rlink == UNDEFINED);
            assert(var_header->heap_size > tot_len);

            printf("TestMultipleInserts PASSED\n");
        }

        void TestHasSpace()
        {
            alignas(16) byte page[DB7_PAGE_SIZE];
            BtreeVarlenLayoutLeaf layout;
            InitPage(page);

            auto *header = CastHeader(page);

            // Fill page until full
            u32 i = 0;
            while (layout.HasSpace(page, MakeKey("key_" + std::to_string(i))))
            {
                layout.Insert(page, MakeKey("key_" + std::to_string(i)), i);
                i++;
            }

            assert(header->count > 0);
            assert(header->level == 0);
            assert(header->max_val == UNDEFINED);
            assert(header->rlink == UNDEFINED);

            printf("TestHasSpace PASSED - fit %u entries\n", header->count);
        }

        void TestSplit()
        {
            alignas(16) byte left_page[DB7_PAGE_SIZE];
            alignas(16) byte right_page[DB7_PAGE_SIZE];
            BtreeVarlenLayoutLeaf layout;
            InitPage(left_page);
            InitPage(right_page);

            auto *left_header = CastHeader(left_page);

            // Fill left page
            std::vector<std::string> keys;
            u32 i = 0;
            while (layout.HasSpace(left_page, MakeKey("key_" + std::to_string(i))))
            {
                std::string k = "key_" + std::to_string(i);
                layout.Insert(left_page, MakeKey(k), i);
                keys.push_back(k);
                i++;
            }

            // shared::PrintVarlenLayout(left_page);

            u32 total = left_header->count;
            std::string new_key = "key_split";

            layout.Split(left_page, right_page, /*new_pid=*/99, MakeKey(new_key), 999);

            auto *right_header = CastHeader(right_page);

            // All keys should still be findable across both pages
            u32 found_left = 0;
            for (auto &k : keys)
            {
                u64 v = layout.Get(left_page, left_header->count, MakeKey(k)) & layout.Get(right_page, right_header->count, MakeKey(k));

                assert(v != std::numeric_limits<u64>::max());

                found_left++;
            }

            // New key should be somewhere
            u64 v = layout.Get(left_page, left_header->count, MakeKey(new_key)) & layout.Get(right_page, right_header->count, MakeKey(new_key));

            assert(v == 999);
            assert(found_left == total);
            assert(left_header->rlink == 99);

            printf("TestSplit PASSED - left=%u right=%u\n", left_header->count, right_header->count);
        }
    }
}

int TESTS_LAYOUT_LEAF()
{
    db7::access::TestInsertAndGet();
    db7::access::TestMultipleInserts();
    db7::access::TestHasSpace();
    db7::access::TestSplit();
    printf("\nAll leaf tests passed!\n");
    return 0;
}
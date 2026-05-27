#include <cstring>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

#include "access/index/varlen_layout/btree_varlen_layout_inter.hpp"
#include "debug/printer.hpp"

namespace db7::access
{
    namespace
    {
        constexpr u32 HSIZE = sizeof(BtreeHeader);
        constexpr u32 UNDEFINED = UINT32_MAX;

        // Helper to make a Key from a string
        Key MakeKey(const std::string &s)
        {
            return Key{(u16)s.size(), (byte *)s.data()};
        }

        // Initialize a fresh page with zeroed memory and proper headers
        void InitPage(byte *page)
        {
            std::memset(page, 0, PAGE_SIZE);
            auto *header = CastHeader(page);
            header->count = 0;
            header->level = 1; // intermediate nodes are level >= 1
            header->rlink = UNDEFINED;
            header->max_val = UNDEFINED;
        }

        void TestCreateRootAndRouting()
        {
            alignas(16) byte page[PAGE_SIZE];
            BtreeVarlenLayoutIntermediate layout(HSIZE);
            InitPage(page);

            auto *header = CastHeader(page);

            layout.CreateRoot(page, MakeKey("mmmm"), /*left=*/1, /*right=*/100);

            assert(header->count == 2);

            // Keys less than separator route to left child
            auto res1 = layout.Get(page, header->count, MakeKey("aaaa"));
            assert(res1 == 1);
            auto res2 = layout.Get(page, header->count, MakeKey("llll"));
            assert(res2 == 1);

            auto res_mid = layout.Get(page, header->count, MakeKey("mmmm"));
            assert(res_mid == 100);

            // Keys greater than or equal to separator route to right child
            auto res3 = layout.Get(page, header->count, MakeKey("nnnn"));
            assert(res3 == 100);
            auto res4 = layout.Get(page, header->count, MakeKey("zzzz"));
            assert(res4 == 100);

            printf("TestCreateRootAndRouting PASSED\n");
        }

        // Multiple separators: [apple|10] [cherry|20] [mango|30] [""->40]
        // Routes: <apple->10, <cherry->20, <mango->30, >=mango->40
        void TestMultipleSeparatorRouting()
        {
            alignas(16) byte page[PAGE_SIZE];
            BtreeVarlenLayoutIntermediate layout(HSIZE);
            InitPage(page);

            auto *header = CastHeader(page);

            // Build: separator "cherry" splits children 10 and 20
            layout.CreateRoot(page, MakeKey("cherry"), /*left=*/5, /*right=*/20);

            // Now insert more separators
            layout.Insert(page, header->count, MakeKey("mango"), 30);
            header->count++;
            layout.Insert(page, header->count, MakeKey("apple"), 10);
            header->count++;

            // Routing: key < "apple" -> 5, "apple" <= key < "cherry" -> 10,
            //          "cherry" <= key < "mango" -> 20, key >= "mango" -> 30
            auto val1 = layout.Get(page, header->count, MakeKey("aaa"));
            assert(val1 == 5);
            auto val2 = layout.Get(page, header->count, MakeKey("banana"));
            assert(val2 == 10);
            auto val3 = layout.Get(page, header->count, MakeKey("dog"));
            assert(val3 == 20);
            auto val4 = layout.Get(page, header->count, MakeKey("zebra"));
            assert(val4 == 30);

            printf("TestMultipleSeparatorRouting PASSED\n");
        }

        void TestHasSpace()
        {
            alignas(16) byte page[PAGE_SIZE];
            BtreeVarlenLayoutIntermediate layout(HSIZE);
            InitPage(page);

            auto *header = CastHeader(page);

            // Fill page until full
            u32 i = 0;
            while (layout.HasSpace(header, MakeKey("key_" + std::to_string(i))))
            {
                layout.Insert(page, header->count, MakeKey("key_" + std::to_string(i)), i);
                header->count++;
                i++;
            }

            assert(header->count > 0);
            assert(header->level == 1);

            printf("TestHasSpace PASSED - fit %u entries\n", header->count);
        }

        void TestSplit()
        {
            alignas(16) byte left_page[PAGE_SIZE];
            alignas(16) byte right_page[PAGE_SIZE];
            BtreeVarlenLayoutIntermediate layout(HSIZE);
            InitPage(left_page);
            InitPage(right_page);

            auto *left_header = CastHeader(left_page);

            // Create root with first key and dummy slot
            layout.CreateRoot(left_page, MakeKey("key_000"), /*left_pid=*/0, /*right_pid=*/1);
            left_header->level = 1;

            // Fill left page with sorted keys
            u32 i = 2;
            while (true)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "key_%03u", i);
                Key k = MakeKey(buf);
                if (!layout.HasSpace(left_header, k))
                    break;
                layout.Insert(left_page, left_header->count, k, i);
                left_header->count++;
                i++;
            }

            u32 total = left_header->count;

            // Verify Get works before split
            for (u32 j = 2; j < i; j++)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "key_%03u", j);
                page_id result = layout.Get(left_page, left_header->count, MakeKey(buf));
                assert(result == j);
            }

            // Split
            std::string new_key_str = "key_split";
            Key separator = layout.Split(left_page, right_page, /*new_pid=*/99, MakeKey(new_key_str), 999);

            auto *right_header = CastHeader(right_page);

            assert(left_header->count > 0);
            assert(right_header->count > 0);
            assert(left_header->count + right_header->count == total + 1);
            assert(left_header->rlink == 99);
            assert(left_header->level == 1);
            assert(right_header->level == 1);
            assert(separator.len > 0);

            u32 found_left = 0, found_right = 0;
            for (u32 j = 2; j < i; j++)
            {
                char buf[16];
                snprintf(buf, sizeof(buf), "key_%03u", j);
                Key k = MakeKey(buf);

                if (!layout.HasSplit(left_header, k))
                {
                    page_id result = layout.Get(left_page, left_header->count, k);
                    assert(result == j);
                    found_left++;
                }
                else
                {
                    page_id result = layout.Get(right_page, right_header->count, k);
                    assert(result == j);
                    found_right++;
                }
            }

            // Check the new key too
            page_id split_result;
            if (!layout.HasSplit(left_header, MakeKey(new_key_str)))
                split_result = layout.Get(left_page, left_header->count, MakeKey(new_key_str));
            else
                split_result = layout.Get(right_page, right_header->count, MakeKey(new_key_str));
            assert(split_result == 999);

            printf("TestSplit PASSED - left=%u right=%u found_left=%u found_right=%u separator_len=%u\n",
                   left_header->count, right_header->count, found_left, found_right, separator.len);
        }
    }
}

int TESTS_LAYOUT_INTER()
{
    db7::access::TestCreateRootAndRouting();
    db7::access::TestMultipleSeparatorRouting();
    db7::access::TestHasSpace();
    db7::access::TestSplit();
    printf("\nAll intermediate tests passed!\n");
    return 0;
}
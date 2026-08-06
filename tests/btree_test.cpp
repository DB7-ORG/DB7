#include <gtest/gtest.h>

#include "access/index/btree.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <variant>
#include <vector>

using namespace db7;

namespace
{
    using Rid = u64;
    using Tree = access::BTreeIndex<Rid>;

    // =========================================================================
    // 1. API ADAPTER
    //
    //    Every call into the index goes through this namespace, and nothing
    //    below it mentions `.success`, `VectorValues`, or the raw signatures.
    //    When the tree API changes, this is the only block you edit.
    // =========================================================================
    namespace api
    {
        inline bool Insert(Tree &tree, access::DataChunk *key, Rid rid)
        {
            return tree.Insert(key, rid).success;
        }

        inline bool Get(Tree &tree, access::DataChunk *key, std::vector<Rid> &out)
        {
            access::VectorValues<Rid> res;
            const bool ok = tree.Get(key, res).success;
            out.assign(res.vec.begin(), res.vec.end());
            return ok;
        }

        // TODO: when Delete/Scan land, add them here and the tests at the
        // bottom of this file stop being DISABLED_.
        //
        inline bool Delete(Tree &tree, access::DataChunk *key, Rid rid)
        {
            return tree.Delete(key, rid).success;
        }
        // inline std::vector<Rid> Scan(Tree &tree, DataChunk *lo, DataChunk *hi);

        /// ChunkDeleter: how a chunk from DataChunkLayout::CreateDataChunk()
        /// is released. Currently a no-op, which LEAKS. Fill in the real call
        /// (`delete chunk;`, `delete[] reinterpret_cast<byte *>(chunk);`, or a
        /// layout method) — a leak is loud under ASAN but harmless, whereas
        /// guessing wrong here is undefined behaviour.
        struct ChunkDeleter
        {
            void operator()(access::DataChunk *chunk) const noexcept
            {
                (void)chunk;
            }
        };
    } // namespace api

    using ChunkPtr = std::unique_ptr<access::DataChunk, api::ChunkDeleter>;

    // =========================================================================
    // 2. ROW MODEL
    // =========================================================================

    /// One column value. std::string (not const char *) so rows built from
    /// temporaries in a loop can't dangle.
    using Value = std::variant<
        bool,
        int8_t, int16_t, int32_t, int64_t,
        uint8_t, uint16_t, uint32_t, uint64_t,
        double,
        std::string>;

    using Row = std::vector<Value>;

    // Column positions, so tests say kStr instead of 2.
    enum ColIdx : size_t
    {
        kBool = 0,
        kInt = 1,
        kStr = 2,
        kDbl = 3,
    };

    /// The schema under test, declared once.
    std::vector<access::TypeSize> Schema()
    {
        return {
            {1, access::type_id::BOOLEAN},
            {2, access::type_id::INTEGER},
            {3, access::type_id::VARCHAR},
            {4, access::type_id::DOUBLE},
        };
    }

    /// The row every test starts from; mutate a copy to make a near-miss key.
    Row BaseRow()
    {
        return Row{true, int32_t{4}, std::string("Jovan123!!"), 3.56};
    }

    template <typename Proj>
    auto Project(std::span<const access::TypeSize> types, Proj proj)
    {
        std::vector<decltype(proj(types.front()))> out;
        out.reserve(types.size());
        for (const auto &t : types)
        {
            out.push_back(proj(t));
        }
        return out;
    }

    // =========================================================================
    // 3. FIXTURE
    // =========================================================================
    class BTreeTest : public ::testing::Test
    {
    protected:
        static constexpr const char *kDataDir = ".data_test";
        static inline u32 next_tbl_ = 1;

        std::unique_ptr<storage::DiskManagerAsync> dm_;
        std::unique_ptr<storage::DiskScheduler> sched_;
        std::unique_ptr<storage::PageVersionManager> vm_;
        std::unique_ptr<storage::BufferPool> bp_;
        std::unique_ptr<access::DataChunkLayout> layout_;
        std::unique_ptr<Tree> tree_;

        /// Wipe stale files once per binary run. Without this, table ids
        /// restart at 1 on every run and a second run reads the first run's
        /// pages — the classic "passes once, then flaky forever" failure.
        static void SetUpTestSuite()
        {
            std::error_code ec;
            std::filesystem::remove_all(kDataDir, ec);
        }

        static void TearDownTestSuite()
        {
            std::error_code ec;
            std::filesystem::remove_all(kDataDir, ec);
        }

        void SetUp() override
        {
            dm_ = std::make_unique<storage::DiskManagerAsync>(kDataDir);
            sched_ = std::make_unique<storage::DiskScheduler>(dm_.get());
            sched_->Start();
            vm_ = std::make_unique<storage::PageVersionManager>();
            static constexpr size_t kTestPoolPages = 256;
            bp_ = std::make_unique<storage::BufferPool>(sched_.get(), vm_.get(), kTestPoolPages);

            const std::vector<access::TypeSize> attr = Schema();

            const auto col_ids = Project(attr, [](const access::TypeSize &t)
                                         { return t.col_id; });
            const auto sizes = Project(attr, [](const access::TypeSize &t)
                                       { return t.size; });

            layout_ = std::make_unique<access::DataChunkLayout>(
                std::span<const catalog::col_oid_t>(col_ids),
                std::span<const u16>(sizes));

            tree_ = std::make_unique<Tree>(bp_.get(), dm_.get(), next_tbl_++, attr);
        }

        void TearDown() override
        {
            // Start() with no matching Stop() leaves the scheduler thread
            // running past the end of the test. Drop this line if your
            // scheduler has no Stop().
            if (sched_)
            {
                sched_->Stop();
            }
        }

        // ---- Chunk construction ---------------------------------------------

        static void FillChunk(access::DataChunk &chunk, const Row &row)
        {
            DB7_ASSERT(row.size() == chunk.GetColumnCount(), "Invalid row size");

            for (size_t i = 0; i < row.size(); ++i)
            {
                std::visit(
                    [&](const auto &value)
                    {
                        using T = std::decay_t<decltype(value)>;

                        if constexpr (std::is_same_v<T, std::string>)
                        {
                            storage::VarlenEntry entry;
                            entry.Set(std::span<const char>(value.data(), value.size()));
                            std::memcpy(chunk.Access(i), &entry, sizeof(entry));
                        }
                        else
                        {
                            static_assert(std::is_trivially_copyable_v<T>);
                            std::memcpy(chunk.Access(i), &value, sizeof(T));
                        }
                    },
                    row[i]);
            }
        }

        ChunkPtr MakeKey(const Row &row)
        {
            ChunkPtr chunk(layout_->CreateDataChunk());
            FillChunk(*chunk, row);
            return chunk;
        }

        // ---- Operations, in terms of Rows -----------------------------------

        [[nodiscard]] bool Insert(const Row &row, Rid rid)
        {
            ChunkPtr key = MakeKey(row);
            return api::Insert(*tree_, key.get(), rid);
        }

        [[nodiscard]] std::vector<Rid> Lookup(const Row &row)
        {
            ChunkPtr key = MakeKey(row);
            std::vector<Rid> out;
            api::Get(*tree_, key.get(), out);
            std::sort(out.begin(), out.end()); // duplicate order is unspecified
            return out;
        }

        // ---- Assertions ------------------------------------------------------

        void ExpectRids(const Row &row, std::vector<Rid> expected)
        {
            std::sort(expected.begin(), expected.end());
            EXPECT_EQ(Lookup(row), expected);
        }

        void ExpectAbsent(const Row &row)
        {
            EXPECT_TRUE(Lookup(row).empty());
        }
    };

    // =========================================================================
    // 4. BASIC CONTRACT
    // =========================================================================

    TEST_F(BTreeTest, LookupOnEmptyTreeFindsNothing)
    {
        ExpectAbsent(BaseRow());
    }

    TEST_F(BTreeTest, InsertThenGetReturnsRid)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1000));
        ExpectRids(BaseRow(), {1000});
    }

    TEST_F(BTreeTest, LookupOfAbsentKeyFindsNothing)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1000));

        Row other = BaseRow();
        other[kInt] = int32_t{99};
        ExpectAbsent(other);
    }

    TEST_F(BTreeTest, DistinctKeysDoNotCollide)
    {
        const std::vector<Row> rows = {
            Row{true, int32_t{4}, std::string("Jovan123!!"), 3.56},
            Row{false, int32_t{7}, std::string("Alice"), 8.9},
            Row{true, int32_t{12}, std::string("Bob"), 1.2},
        };

        for (size_t i = 0; i < rows.size(); ++i)
        {
            ASSERT_TRUE(Insert(rows[i], 100 + i)) << "insert #" << i;
        }
        for (size_t i = 0; i < rows.size(); ++i)
        {
            ExpectRids(rows[i], {100 + i});
        }
    }

    // =========================================================================
    // 5. KEY ENCODING
    //
    //    Each case differs from the base row in exactly one column. If the
    //    encoder drops a column, forgets a field separator, or compares only
    //    a prefix, one of these collapses onto the base key and fails.
    // =========================================================================

    TEST_F(BTreeTest, EveryColumnParticipatesInTheKey)
    {
        const Row base = BaseRow();
        ASSERT_TRUE(Insert(base, 1));

        struct Case
        {
            const char *name;
            ColIdx col;
            Value value;
        };

        const std::vector<Case> cases = {
            {"bool flipped", kBool, false},
            {"int changed", kInt, int32_t{5}},
            {"string changed", kStr, std::string("Jovan123!?")},
            {"double changed", kDbl, 3.57},
        };

        for (const auto &c : cases)
        {
            SCOPED_TRACE(c.name);
            Row variant = base;
            variant[c.col] = c.value;

            ExpectAbsent(variant);
            EXPECT_TRUE(Insert(variant, 2));
            ExpectRids(variant, {2});
            ExpectRids(base, {1}); // base must be untouched
        }
    }

    TEST_F(BTreeTest, StringPrefixIsNotAMatch)
    {
        Row shorter = BaseRow();
        shorter[kStr] = std::string("Jovan");

        ASSERT_TRUE(Insert(BaseRow(), 1));
        ExpectAbsent(shorter);

        ASSERT_TRUE(Insert(shorter, 2));
        ExpectRids(BaseRow(), {1});
        ExpectRids(shorter, {2});
    }

    TEST_F(BTreeTest, EmptyStringIsAValidKey)
    {
        Row empty = BaseRow();
        empty[kStr] = std::string("");

        ASSERT_TRUE(Insert(empty, 42));
        ExpectRids(empty, {42});
        ExpectAbsent(BaseRow());
    }

    TEST_F(BTreeTest, IntegerBoundaryValues)
    {
        const std::vector<int32_t> bounds = {
            std::numeric_limits<int32_t>::min(),
            -1,
            0,
            1,
            std::numeric_limits<int32_t>::max(),
        };

        for (size_t i = 0; i < bounds.size(); ++i)
        {
            Row r = BaseRow();
            r[kInt] = bounds[i];
            ASSERT_TRUE(Insert(r, 500 + i)) << "value " << bounds[i];
        }
        for (size_t i = 0; i < bounds.size(); ++i)
        {
            Row r = BaseRow();
            r[kInt] = bounds[i];
            SCOPED_TRACE(bounds[i]);
            ExpectRids(r, {500 + i});
        }
    }

    TEST_F(BTreeTest, NegativeZeroMatchesPositiveZero)
    {
        // IEEE-754 says -0.0 == 0.0. A memcmp-based key encoder disagrees.
        // If your engine deliberately treats them as distinct, invert this.
        Row pos = BaseRow();
        Row neg = BaseRow();
        pos[kDbl] = 0.0;
        neg[kDbl] = -0.0;

        ASSERT_TRUE(Insert(pos, 7));
        ExpectRids(neg, {7});
    }

    // =========================================================================
    // 6. SPLITS AND SCALE
    //
    //    B-tree bugs live in split paths, and which paths you hit depends on
    //    insertion order. Same data, three orders.
    // =========================================================================

    namespace
    {
        constexpr size_t kManyKeys = 20'000;

        Row NthRow(size_t i)
        {
            Row r = BaseRow();
            r[kBool] = (i % 2 == 0);
            r[kInt] = static_cast<int32_t>(i);
            r[kStr] = "key_" + std::to_string(i);
            r[kDbl] = static_cast<double>(i) * 1.5;
            return r;
        }

        constexpr Rid RidFor(size_t i) { return 10'000 + i; }
    } // namespace

    TEST_F(BTreeTest, ManyKeysAscending)
    {
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "insert " << i;
        }
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    TEST_F(BTreeTest, ManyKeysDescending)
    {
        for (size_t i = kManyKeys; i-- > 0;)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "insert " << i;
        }
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    TEST_F(BTreeTest, ManyKeysShuffled)
    {
        std::vector<size_t> order(kManyKeys);
        std::iota(order.begin(), order.end(), 0);
        std::shuffle(order.begin(), order.end(), std::mt19937{0xD7}); // fixed seed

        for (size_t i : order)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "insert " << i;
        }
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    TEST_F(BTreeTest, NeighbouringKeysStaySeparatedAcrossSplits)
    {
        // Keys that differ only in the last byte of the string, so any split
        // that truncates a separator key merges them.
        for (size_t i = 0; i < 500; ++i)
        {
            Row r = BaseRow();
            r[kStr] = "shared_" + std::string(3 - std::to_string(i).size(), '0') + std::to_string(i); // "shared_042", 10 bytes
            ASSERT_TRUE(Insert(r, RidFor(i)));
        }
        for (size_t i = 0; i < 500; ++i)
        {
            Row r = BaseRow();
            r[kStr] = "shared_" + std::string(3 - std::to_string(i).size(), '0') + std::to_string(i); // "shared_042", 10 bytes
            SCOPED_TRACE(i);
            ExpectRids(r, {RidFor(i)});
        }
    }

    // =========================================================================
    // 7. BEHAVIOURAL ASSUMPTIONS
    //
    //    These encode a guess about intended semantics. Read each one and
    //    either keep it (it now documents the contract) or invert it.
    // =========================================================================

    /// Assumes: non-unique index, duplicates accumulate under one key.
    /// If the index is unique, expect Insert to return false instead.
    TEST_F(BTreeTest, DuplicateKeyDistinctRids)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));
        ASSERT_TRUE(Insert(BaseRow(), 2));
        ExpectRids(BaseRow(), {1, 2});
    }

    /// Assumes: string keys are NFD-normalized before encoding, so precomposed
    /// U+00E9 and decomposed "e" + U+0301 are the same key.
    /// Delete this test if the engine stores raw bytes.
    TEST_F(BTreeTest, CanonicallyEquivalentStringsAreTheSameKey)
    {
        Row precomposed = BaseRow();
        Row decomposed = BaseRow();
        precomposed[kStr] = std::string("caf\u00e9"); // café
        decomposed[kStr] = std::string("cafe\u0301"); // cafe + combining acute

        ASSERT_TRUE(Insert(precomposed, 77));
        ExpectRids(decomposed, {77});
    }

    /// Assumes: the index is case-sensitive (no UTF8PROC_CASEFOLD).
    /// If keys are casefolded, flip this to ExpectRids(upper, {88}).
    TEST_F(BTreeTest, CaseSensitiveStringComparison)
    {
        Row lower = BaseRow();
        Row upper = BaseRow();
        lower[kStr] = std::string("straat");
        upper[kStr] = std::string("STRAAT");

        ASSERT_TRUE(Insert(lower, 88));
        ExpectAbsent(upper);
    }

    // =========================================================================
    // 8. DELETE
    // =========================================================================

    TEST_F(BTreeTest, DeleteRemovesOnlyTargetRid)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));
        ASSERT_TRUE(Insert(BaseRow(), 2));

        ChunkPtr key = MakeKey(BaseRow());
        ASSERT_TRUE(api::Delete(*tree_, key.get(), 1));

        ExpectRids(BaseRow(), {2});
    }

    TEST_F(BTreeTest, DeleteThenLookupFindsNothing)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));

        ChunkPtr key = MakeKey(BaseRow());
        ASSERT_TRUE(api::Delete(*tree_, key.get(), 1));

        ExpectAbsent(BaseRow());
    }

    TEST_F(BTreeTest, DeleteDoesNotDisturbNeighbours)
    {
        Row a = BaseRow();
        Row b = BaseRow();
        Row c = BaseRow();
        a[kInt] = int32_t{1};
        b[kInt] = int32_t{2};
        c[kInt] = int32_t{3};

        ASSERT_TRUE(Insert(a, 10));
        ASSERT_TRUE(Insert(b, 20));
        ASSERT_TRUE(Insert(c, 30));

        ChunkPtr key = MakeKey(b);
        ASSERT_TRUE(api::Delete(*tree_, key.get(), 20));

        ExpectRids(a, {10});
        ExpectAbsent(b);
        ExpectRids(c, {30});
    }

    TEST_F(BTreeTest, ReinsertAfterDelete)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));

        ChunkPtr key = MakeKey(BaseRow());
        ASSERT_TRUE(api::Delete(*tree_, key.get(), 1));

        ASSERT_TRUE(Insert(BaseRow(), 2));
        ExpectRids(BaseRow(), {2});
    }

    /// Empties the tree completely — exercises underflow, merge, and root
    /// collapse. Then refills it to make sure nothing was left corrupt.
    TEST_F(BTreeTest, DeleteAllThenRefill)
    {
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "insert " << i;
        }

        for (size_t i = 0; i < kManyKeys; ++i)
        {
            ChunkPtr key = MakeKey(NthRow(i));
            ASSERT_TRUE(api::Delete(*tree_, key.get(), RidFor(i))) << "delete " << i;
        }

        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectAbsent(NthRow(i));
        }

        for (size_t i = 0; i < kManyKeys; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "refill " << i;
        }
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    /// Interleaved churn in shuffled order — the pattern that finds
    /// merge/rebalance bugs that ordered deletion walks straight past.
    TEST_F(BTreeTest, InterleavedInsertDeleteChurn)
    {
        constexpr size_t kN = 500;
        std::mt19937 rng{0xD7};

        std::vector<size_t> live;
        for (size_t i = 0; i < kN; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i)));
            live.push_back(i);

            if (i % 3 == 2 && !live.empty())
            {
                std::uniform_int_distribution<size_t> pick(0, live.size() - 1);
                const size_t at = pick(rng);
                const size_t victim = live[at];

                ChunkPtr key = MakeKey(NthRow(victim));
                ASSERT_TRUE(api::Delete(*tree_, key.get(), RidFor(victim)))
                    << "delete " << victim;

                live.erase(live.begin() + at);
            }
        }

        std::sort(live.begin(), live.end());
        for (size_t i = 0; i < kN; ++i)
        {
            SCOPED_TRACE(i);
            const bool present = std::binary_search(live.begin(), live.end(), i);
            if (present)
            {
                ExpectRids(NthRow(i), {RidFor(i)});
            }
            else
            {
                ExpectAbsent(NthRow(i));
            }
        }
    }

    TEST_F(BTreeTest, DISABLED_RangeScanReturnsKeysInOrder)
    {
        GTEST_SKIP() << "Scan not yet in api::";
    }

} // namespace
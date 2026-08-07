#include <gtest/gtest.h>

#include "access/index/btree.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"
#include "concurrency.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <variant>
#include <vector>

using namespace db7;
using namespace db7::access;
using namespace db7::test;

namespace
{
    using Rid = u64;
    using Tree = BTreeIndex<Rid>;

    // =========================================================================
    // 1. API ADAPTER
    //
    //    Every call into the index goes through this namespace, and nothing
    //    below it mentions `.success`, `VectorValues`, or the raw signatures.
    //    When the tree API changes, this is the only block you edit.
    // =========================================================================
    namespace api
    {
        inline bool Insert(Tree &tree, DataChunk *key, Rid rid)
        {
            return tree.Insert(key, rid).success;
        }

        inline bool Get(Tree &tree, DataChunk *key, std::vector<Rid> &out)
        {
            VectorValues<Rid> res;
            const bool ok = tree.Get(key, res).success;
            out.assign(res.vec.begin(), res.vec.end());
            return ok;
        }

        inline bool Delete(Tree &tree, DataChunk *key, Rid rid)
        {
            return tree.Delete(key, rid).success;
        }

        // TODO: when Scan lands, add it here and the test at the bottom of this
        // file stops being DISABLED_.
        // inline std::vector<Rid> Scan(Tree &tree, DataChunk *lo, DataChunk *hi);

        /// CreateDataChunk() returns `new byte[total_size_]` reinterpreted as a
        /// DataChunk*, so it must be released as a byte array — deleting
        /// through the DataChunk* is the wrong type and is undefined behaviour.
        struct ChunkDeleter
        {
            void operator()(DataChunk *chunk) const noexcept
            {
                delete[] reinterpret_cast<byte *>(chunk);
            }
        };
    } // namespace api

    using ChunkPtr = std::unique_ptr<DataChunk, api::ChunkDeleter>;

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
    std::vector<TypeSize> Schema()
    {
        return {
            {1, type_id::BOOLEAN},
            {2, type_id::INTEGER},
            {3, type_id::VARCHAR},
            {4, type_id::DOUBLE},
        };
    }

    /// The row every test starts from; mutate a copy to make a near-miss key.
    Row BaseRow()
    {
        return Row{true, int32_t{4}, std::string("Jovan123!!"), 3.56};
    }

    constexpr size_t kManyKeys = 20'000;

    /// Row i, distinct in every column. The string is zero-padded so its
    /// lexicographic order matches the integer order — without padding
    /// "key_10" sorts before "key_9" and an "ascending" test isn't ascending.
    Row NthRow(size_t i)
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "k_%08zu", i);

        Row r = BaseRow();
        r[kBool] = (i % 2 == 0);
        r[kInt] = static_cast<int32_t>(i);
        r[kStr] = std::string(buf);
        r[kDbl] = static_cast<double>(i) * 1.5;
        return r;
    }

    /// Never 0 — tid 0 is reserved for the [encoded][0] search sentinel.
    constexpr Rid RidFor(size_t i) { return 10'000 + i; }

    template <typename Proj>
    auto Project(std::span<const TypeSize> types, Proj proj)
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
    //
    //    THREAD SAFETY: Insert(), Delete(), Lookup(), Contains() and MakeKey()
    //    contain no gtest macros and are safe to call from worker threads.
    //    ExpectRids() and ExpectAbsent() use EXPECT_* and are main-thread only.
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
        std::unique_ptr<DataChunkLayout> layout_;
        std::unique_ptr<Tree> tree_;

        /// Concurrent tests override this. A 256-page pool under 12 threads is
        /// mostly a test of the eviction path, which hides tree bugs behind
        /// buffer-pool contention.
        virtual size_t PoolPages() const { return 256; }

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
            if (::testing::Test::HasFailure())
            {
                return; // leave the files behind for inspection
            }
            std::error_code ec;
            std::filesystem::remove_all(kDataDir, ec);
        }

        void SetUp() override
        {
            dm_ = std::make_unique<storage::DiskManagerAsync>(kDataDir);
            sched_ = std::make_unique<storage::DiskScheduler>(dm_.get());
            sched_->Start();
            vm_ = std::make_unique<storage::PageVersionManager>();
            bp_ = std::make_unique<storage::BufferPool>(sched_.get(), vm_.get(),
                                                        PoolPages());

            const std::vector<TypeSize> attr = Schema();

            const auto col_ids = Project(attr, [](const TypeSize &t)
                                         { return t.col_id; });
            const auto sizes = Project(attr, [](const TypeSize &t)
                                       { return t.size; });

            layout_ = std::make_unique<DataChunkLayout>(
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

        // ---- Chunk construction (thread-safe) --------------------------------

        static void FillChunk(DataChunk &chunk, const Row &row)
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

        /// Safe from worker threads: CreateDataChunk only reads the layout
        /// header and allocates fresh memory.
        ChunkPtr MakeKey(const Row &row)
        {
            ChunkPtr chunk(layout_->CreateDataChunk());
            FillChunk(*chunk, row);
            return chunk;
        }

        // ---- Operations, in terms of Rows (thread-safe) ----------------------

        [[nodiscard]] bool Insert(const Row &row, Rid rid)
        {
            ChunkPtr key = MakeKey(row);
            return api::Insert(*tree_, key.get(), rid);
        }

        [[nodiscard]] bool Delete(const Row &row, Rid rid)
        {
            ChunkPtr key = MakeKey(row);
            return api::Delete(*tree_, key.get(), rid);
        }

        [[nodiscard]] std::vector<Rid> Lookup(const Row &row)
        {
            ChunkPtr key = MakeKey(row);
            std::vector<Rid> out;
            api::Get(*tree_, key.get(), out);
            std::sort(out.begin(), out.end()); // duplicate order is unspecified
            return out;
        }

        [[nodiscard]] bool Contains(const Row &row, Rid rid)
        {
            const auto rids = Lookup(row);
            return std::binary_search(rids.begin(), rids.end(), rid);
        }

        // ---- Assertions (MAIN THREAD ONLY) -----------------------------------

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
        // Keys sharing a long prefix and differing only at the end, so any
        // split that truncates a separator key merges them.
        auto shared_row = [](size_t i)
        {
            char buf[24];
            std::snprintf(buf, sizeof(buf), "shared_%03zu", i);
            Row r = BaseRow();
            r[kStr] = std::string(buf);
            return r;
        };

        for (size_t i = 0; i < 500; ++i)
        {
            ASSERT_TRUE(Insert(shared_row(i), RidFor(i))) << "insert " << i;
        }
        for (size_t i = 0; i < 500; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(shared_row(i), {RidFor(i)});
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

    // TODO for now difficult to test in this test
    // /// Assumes: the index is case-sensitive (no UTF8PROC_CASEFOLD).
    // /// If keys are casefolded, flip this to ExpectRids(upper, {88}).
    // TEST_F(BTreeTest, CaseSensitiveStringComparison)
    // {
    //     Row lower = BaseRow();
    //     Row upper = BaseRow();
    //     lower[kStr] = std::string("straat");
    //     upper[kStr] = std::string("STRAAT");

    //     ASSERT_TRUE(Insert(lower, 88));
    //     ExpectAbsent(upper);
    // }

    // =========================================================================
    // 8. DELETE
    // =========================================================================

    TEST_F(BTreeTest, DeleteRemovesOnlyTargetRid)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));
        ASSERT_TRUE(Insert(BaseRow(), 2));
        ASSERT_TRUE(Delete(BaseRow(), 1));
        ExpectRids(BaseRow(), {2});
    }

    TEST_F(BTreeTest, DeleteThenLookupFindsNothing)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));
        ASSERT_TRUE(Delete(BaseRow(), 1));
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

        ASSERT_TRUE(Delete(b, 20));

        ExpectRids(a, {10});
        ExpectAbsent(b);
        ExpectRids(c, {30});
    }

    TEST_F(BTreeTest, ReinsertAfterDelete)
    {
        ASSERT_TRUE(Insert(BaseRow(), 1));
        ASSERT_TRUE(Delete(BaseRow(), 1));
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
            ASSERT_TRUE(Delete(NthRow(i), RidFor(i))) << "delete " << i;
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

                ASSERT_TRUE(Delete(NthRow(victim), RidFor(victim)))
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

    // =========================================================================
    // 9. CONCURRENCY
    //
    //    Every test here is built so the expected end state is DETERMINISTIC
    //    despite the interleaving: threads either work on disjoint key ranges,
    //    or contribute disjoint rids to one key. A test whose expected result
    //    depends on ordering can't tell a race from a legal outcome.
    //
    //    gtest's ASSERT_* macros expand to `return;`, so inside a worker lambda
    //    they abandon that thread's remaining work instead of failing the test,
    //    and reporting from a non-main thread isn't portable. Workers therefore
    //    record into an ErrorLog and the main thread calls ExpectNoFailures().
    // =========================================================================

    /// 256 frames under 12 threads mostly measures eviction. Give the
    /// concurrent tests enough that page contention is about the tree.
    class BTreeConcurrencyTest : public BTreeTest
    {
    protected:
        size_t PoolPages() const override { return 4096; }
    };

    // ---- write / write ------------------------------------------------------

    /// Threads insert disjoint key ranges. Catches lost inserts from split
    /// races: a separator that never reaches its parent leaves a page
    /// unreachable, and its keys vanish with no error reported anywhere.
    TEST_F(BTreeConcurrencyTest, ConcurrentInsertDisjointKeys)
    {
        const unsigned T = DefaultThreads();
        ErrorLog log;

        RunParallel(T,
                    [&](unsigned t)
                    {
                        const Range r = PartitionRange(t, T, kManyKeys);
                        for (size_t i = r.lo; i < r.hi; ++i)
                        {
                            if (!Insert(NthRow(i), RidFor(i)))
                            {
                                log.Failf("insert failed for key ", i);
                            }
                        }
                    });

        ExpectNoFailures(log);

        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    /// Same, but each thread walks its range backwards, so splits happen on the
    /// left edge of pages rather than the right.
    TEST_F(BTreeConcurrencyTest, ConcurrentInsertDescendingWithinRange)
    {
        const unsigned T = DefaultThreads();
        ErrorLog log;

        RunParallel(T,
                    [&](unsigned t)
                    {
                        const Range r = PartitionRange(t, T, kManyKeys);
                        for (size_t i = r.hi; i-- > r.lo;)
                        {
                            if (!Insert(NthRow(i), RidFor(i)))
                            {
                                log.Failf("insert failed for key ", i);
                            }
                        }
                    });

        ExpectNoFailures(log);

        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    /// All threads hammer ONE key with distinct rids. Every entry shares an
    /// encoded prefix and differs only in the tid suffix, so this stresses the
    /// (key,tid) total order, the cross-page `proceed` scan, and separator
    /// uniqueness in intermediate nodes.
    TEST_F(BTreeConcurrencyTest, ConcurrentInsertSameKeyDistinctRids)
    {
        const unsigned T = DefaultThreads();
        constexpr size_t kPerThread = 250;
        const Row key = NthRow(7);
        ErrorLog log;

        RunParallel(T,
                    [&](unsigned t)
                    {
                        for (size_t j = 0; j < kPerThread; ++j)
                        {
                            const Rid rid = RidFor(t * kPerThread + j);
                            if (!Insert(key, rid))
                            {
                                log.Failf("insert failed for rid ", rid);
                            }
                        }
                    });

        ExpectNoFailures(log);

        std::vector<Rid> expected;
        expected.reserve(T * kPerThread);
        for (unsigned t = 0; t < T; ++t)
        {
            for (size_t j = 0; j < kPerThread; ++j)
            {
                expected.push_back(RidFor(t * kPerThread + j));
            }
        }
        std::sort(expected.begin(), expected.end());

        EXPECT_EQ(Lookup(key), expected);
    }

    // ---- read / write -------------------------------------------------------

    /// The direct test for optimistic-read validation. Readers only ever look
    /// up keys committed BEFORE any writer started, so those rids must always
    /// be visible. A reader that observes a page mid-compaction and isn't
    /// caught by the version check comes back with the key missing.
    TEST_F(BTreeConcurrencyTest, ReadersNeverMissPrefilledKeysDuringWrites)
    {
        constexpr size_t kPrefill = 2'000;
        constexpr size_t kWritten = kManyKeys;
        constexpr size_t kReadsPerThread = 20'000;

        for (size_t i = 0; i < kPrefill; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "prefill " << i;
        }

        const unsigned T = DefaultThreads();
        const unsigned writers = std::max(1u, T / 2);
        const unsigned readers = std::max(1u, T - writers);

        ErrorLog log;

        RunParallelMixed(
            writers,
            [&](unsigned t)
            {
                const Range r = PartitionRange(t, writers, kWritten, kPrefill);
                for (size_t i = r.lo; i < r.hi; ++i)
                {
                    if (!Insert(NthRow(i), RidFor(i)))
                    {
                        log.Failf("writer: insert failed for key ", i);
                    }
                }
            },
            readers,
            [&](unsigned t)
            {
                std::mt19937 rng{0xD7u + t};
                std::uniform_int_distribution<size_t> pick(0, kPrefill - 1);

                for (size_t n = 0; n < kReadsPerThread; ++n)
                {
                    const size_t i = pick(rng);
                    const auto rids = Lookup(NthRow(i));

                    if (!std::binary_search(rids.begin(), rids.end(), RidFor(i)))
                    {
                        log.Failf("reader: key ", i, " lost rid ", RidFor(i),
                                  " (saw ", rids.size(), " rids)");
                    }
                }
            });

        ExpectNoFailures(log);

        for (size_t i = 0; i < kWritten; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    /// Pure readers, no writers. If this ever fails the problem is in the read
    /// path itself rather than in write interference — a useful bisection.
    TEST_F(BTreeConcurrencyTest, ConcurrentReadersOnStableTree)
    {
        constexpr size_t kN = 5'000;
        for (size_t i = 0; i < kN; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "prefill " << i;
        }

        const unsigned T = DefaultThreads();
        ErrorLog log;

        RunParallel(T,
                    [&](unsigned t)
                    {
                        std::mt19937 rng{0xBEEFu + t};
                        std::uniform_int_distribution<size_t> pick(0, kN - 1);

                        for (size_t n = 0; n < kN; ++n)
                        {
                            const size_t i = pick(rng);
                            const auto rids = Lookup(NthRow(i));
                            if (rids.size() != 1 || rids[0] != RidFor(i))
                            {
                                log.Failf("key ", i, " returned ", rids.size(),
                                          " rids");
                            }
                        }
                    });

        ExpectNoFailures(log);
    }

    // ---- delete -------------------------------------------------------------

    TEST_F(BTreeConcurrencyTest, ConcurrentDeleteDisjointKeys)
    {
        for (size_t i = 0; i < kManyKeys; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "prefill " << i;
        }

        const unsigned T = DefaultThreads();
        ErrorLog log;

        RunParallel(T,
                    [&](unsigned t)
                    {
                        const Range r = PartitionRange(t, T, kManyKeys);
                        for (size_t i = r.lo; i < r.hi; ++i)
                        {
                            if (!Delete(NthRow(i), RidFor(i)))
                            {
                                log.Failf("delete failed for key ", i);
                            }
                        }
                    });

        ExpectNoFailures(log);

        for (size_t i = 0; i < kManyKeys; ++i)
        {
            SCOPED_TRACE(i);
            ExpectAbsent(NthRow(i));
        }
    }

    /// Half the threads delete existing keys while the rest insert new ones
    /// into a disjoint range. Deletes shrink pages while inserts split them,
    /// which is where compaction and split race each other.
    TEST_F(BTreeConcurrencyTest, ConcurrentInsertAndDeleteDisjointRanges)
    {
        constexpr size_t kOld = 10'000;    // [0, kOld)     — deleted
        constexpr size_t kNew = kManyKeys; // [kOld, kNew)  — inserted

        for (size_t i = 0; i < kOld; ++i)
        {
            ASSERT_TRUE(Insert(NthRow(i), RidFor(i))) << "prefill " << i;
        }

        const unsigned T = DefaultThreads();
        const unsigned deleters = std::max(1u, T / 2);
        const unsigned inserters = std::max(1u, T - deleters);

        ErrorLog log;

        RunParallelMixed(
            deleters,
            [&](unsigned t)
            {
                const Range r = PartitionRange(t, deleters, kOld);
                for (size_t i = r.lo; i < r.hi; ++i)
                {
                    if (!Delete(NthRow(i), RidFor(i)))
                    {
                        log.Failf("delete failed for key ", i);
                    }
                }
            },
            inserters,
            [&](unsigned t)
            {
                const Range r = PartitionRange(t, inserters, kNew, kOld);
                for (size_t i = r.lo; i < r.hi; ++i)
                {
                    if (!Insert(NthRow(i), RidFor(i)))
                    {
                        log.Failf("insert failed for key ", i);
                    }
                }
            });

        ExpectNoFailures(log);

        for (size_t i = 0; i < kOld; ++i)
        {
            SCOPED_TRACE(i);
            ExpectAbsent(NthRow(i));
        }
        for (size_t i = kOld; i < kNew; ++i)
        {
            SCOPED_TRACE(i);
            ExpectRids(NthRow(i), {RidFor(i)});
        }
    }

    /// Each thread owns its key range and churns it with a random op mix, so
    /// the final state is still exactly computable per thread.
    TEST_F(BTreeConcurrencyTest, MixedOperationsOnPerThreadRanges)
    {
        const unsigned T = DefaultThreads();
        constexpr size_t kOpsPerThread = 4'000;

        ErrorLog log;
        std::vector<std::vector<size_t>> live(T);

        RunParallel(T,
                    [&](unsigned t)
                    {
                        const Range r = PartitionRange(t, T, kManyKeys);
                        if (r.empty())
                        {
                            return;
                        }

                        std::mt19937 rng{0xC0FFEEu + t};
                        std::uniform_int_distribution<size_t> pick(r.lo, r.hi - 1);
                        std::uniform_int_distribution<int> op(0, 2);

                        std::vector<bool> present(r.size(), false);

                        for (size_t n = 0; n < kOpsPerThread; ++n)
                        {
                            const size_t i = pick(rng);
                            const size_t at = i - r.lo;

                            switch (op(rng))
                            {
                            case 0: // insert if absent
                                if (!present[at])
                                {
                                    if (!Insert(NthRow(i), RidFor(i)))
                                        log.Failf("insert failed for key ", i);
                                    else
                                        present[at] = true;
                                }
                                break;

                            case 1: // delete if present
                                if (present[at])
                                {
                                    if (!Delete(NthRow(i), RidFor(i)))
                                        log.Failf("delete failed for key ", i);
                                    else
                                        present[at] = false;
                                }
                                break;

                            default: // read back and check against our record
                            {
                                const bool found = Contains(NthRow(i), RidFor(i));
                                if (found != present[at])
                                {
                                    log.Failf("key ", i, ": expected present=",
                                              present[at], " but found=", found);
                                }
                                break;
                            }
                            }
                        }

                        for (size_t at = 0; at < present.size(); ++at)
                        {
                            if (present[at])
                            {
                                live[t].push_back(r.lo + at);
                            }
                        }
                    });

        ExpectNoFailures(log);

        for (unsigned t = 0; t < T; ++t)
        {
            const Range r = PartitionRange(t, T, kManyKeys);
            for (size_t i = r.lo; i < r.hi; ++i)
            {
                SCOPED_TRACE(i);
                const bool expected =
                    std::binary_search(live[t].begin(), live[t].end(), i);
                if (expected)
                {
                    ExpectRids(NthRow(i), {RidFor(i)});
                }
                else
                {
                    ExpectAbsent(NthRow(i));
                }
            }
        }
    }

    // ---- repetition ---------------------------------------------------------

    /// A race that fires one run in fifty passes a single run. Small and fast
    /// so it can be repeated; raise kIterations when you suspect a race.
    /// Calls TearDown/SetUp directly for a fresh tree each pass — if your
    /// DiskScheduler can't be restarted, delete this and use --gtest_repeat.
    TEST_F(BTreeConcurrencyTest, RepeatedSmallConcurrentInsert)
    {
        constexpr int kIterations = 50;
        constexpr size_t kSmall = 2'000;
        const unsigned T = DefaultThreads();

        Repeat(kIterations,
               [&](int iteration)
               {
                   TearDown();
                   SetUp();

                   ErrorLog log;
                   RunParallel(T,
                               [&](unsigned t)
                               {
                                   const Range r = PartitionRange(t, T, kSmall);
                                   for (size_t i = r.lo; i < r.hi; ++i)
                                   {
                                       if (!Insert(NthRow(i), RidFor(i)))
                                       {
                                           log.Failf("iter ", iteration,
                                                     ": insert failed for key ", i);
                                       }
                                   }
                               });

                   ExpectNoFailures(log);

                   for (size_t i = 0; i < kSmall; ++i)
                   {
                       const auto rids = Lookup(NthRow(i));
                       if (rids.size() != 1 || rids[0] != RidFor(i))
                       {
                           ADD_FAILURE() << "iter " << iteration << ": key " << i
                                         << " returned " << rids.size() << " rids";
                           break;
                       }
                   }
               });
    }

    // =========================================================================
    // 10. PENDING API
    // =========================================================================

    TEST_F(BTreeTest, DISABLED_RangeScanReturnsKeysInOrder)
    {
        GTEST_SKIP() << "Scan not yet in api::";
    }

} // namespace
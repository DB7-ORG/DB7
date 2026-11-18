#include <gtest/gtest.h>

#include "storage/columns_chunk.hpp"
#include "storage/data_chunk.hpp"
#include "storage/layouts/storage_column_desc.hpp"
#include "storage/varlen_entry.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

using namespace db7;

namespace {

// =========================================================================
// 1. HELPERS
// =========================================================================

struct ChunkDeleter {
  void operator()(DataChunk *chunk) const noexcept { DataChunk::Destroy(chunk); }
};
using ChunkPtr = std::unique_ptr<DataChunk, ChunkDeleter>;

/// Varlen slot layout these tests rely on:
///   [u16 len][6-byte prefix][u64 ref]   long string, ref = offset (page) or pointer (chunk)
///   [u16 len][up to 14 inline bytes]    short string
constexpr size_t kSlot = storage::VarlenEntry::slot_size;
constexpr size_t kInline = storage::VarlenEntry::inline_bytes;
constexpr size_t kPrefix = kSlot - sizeof(u16) - sizeof(u64);
constexpr size_t kRefOff = sizeof(u16) + kPrefix;
static_assert(kSlot == 16 && kInline == 14 && kPrefix == 6, "varlen layout changed, fix tests");

/// Fake page memory. make_unique<byte[]> is 16-byte aligned, which VarlenEntry::Cast requires.
std::unique_ptr<byte[]> Page(size_t n = 1024) { return std::make_unique<byte[]>(n); }

/// ColumnsChunk only reads slot_size, isVarlen and isNullable, so build any valid
/// descriptor and overwrite those. `type` stays VARCHAR, which nothing here reads.
storage::StorageColumnDesc Desc(column_t oid, u16 slot, bool varlen, bool nullable) {
  storage::StorageColumnDesc d(oid, type_id::VARCHAR);
  d.slot_size = slot;
  d.isVarlen = varlen;
  d.isNullable = nullable;
  return d;
}

/// The chunk keeps a column's NULL bitmap right after its 2048 slots.
ChunkBitmap NullsOf(ColumnsChunk &chunk, u16 idx, u8 slot_size) {
  return ChunkBitmap(chunk.GetColumnByIdx(idx) + STANDARD_VECTOR_SIZE * slot_size);
}

u32 ReadU32(ColumnsChunk &chunk, u16 idx, u16 row) {
  u32 v;
  std::memcpy(&v, chunk.GetColumnByIdx(idx) + row * sizeof(u32), sizeof(v));
  return v;
}

byte *SlotOf(ColumnsChunk &chunk, u16 idx, u16 row) {
  return chunk.GetColumnByIdx(idx) + row * kSlot;
}

const byte *RefAsPtr(const byte *slot) {
  u64 ref;
  std::memcpy(&ref, slot + kRefOff, sizeof(ref));
  return reinterpret_cast<const byte *>(static_cast<uintptr_t>(ref));
}

void WriteInlineSlot(byte *slot, std::string_view s) {
  ASSERT_LE(s.size(), kInline);
  std::memset(slot, 0, kSlot);
  const u16 len = static_cast<u16>(s.size());
  std::memcpy(slot, &len, sizeof(len));
  std::memcpy(slot + sizeof(u16), s.data(), s.size());
}

/// Long string in page form: payload goes into the page heap at `heap_off`.
void WriteLongSlot(byte *page, byte *slot, std::string_view s, u16 heap_off) {
  ASSERT_GT(s.size(), kInline);
  std::memset(slot, 0, kSlot);
  const u16 len = static_cast<u16>(s.size());
  std::memcpy(slot, &len, sizeof(len));
  std::memcpy(slot + sizeof(u16), s.data(), kPrefix);
  const u64 ref = heap_off;
  std::memcpy(slot + kRefOff, &ref, sizeof(ref));
  std::memcpy(page + heap_off, s.data(), s.size());
}

std::string_view View(const byte *p, size_t n) { return {reinterpret_cast<const char *>(p), n}; }

// =========================================================================
// 2. FIXTURE
// =========================================================================

class ColumnsChunkTest : public ::testing::Test {
protected:
  /// Mixed widths in an order that forces padding, non-sequential oids.
  static std::vector<ChunkColumn> Schema() {
    return {
        {10, 1},  // kU8
        {11, 8},  // kU64
        {12, 4},  // kU32
        {13, 16}, // kStr, varlen slot
    };
  }

  enum ColIdx : u16 { kU8 = 0, kU64 = 1, kU32 = 2, kStr = 3 };

  static constexpr column_t kUnknownOid = 999;

  /// u32 source vector with values 1000 + i, optionally with a NULL bitmap.
  struct U32Source {
    std::unique_ptr<byte[]> page = Page();
    storage::ColumnVector vec;

    U32Source(u16 count, bool with_bitmap) {
      byte *data = page.get() + 64;
      for (u32 i = 0; i < count; ++i) {
        const u32 v = 1000 + i;
        std::memcpy(data + i * sizeof(u32), &v, sizeof(v));
      }
      ChunkBitmap bm = with_bitmap ? ChunkBitmap(page.get()) : ChunkBitmap();
      vec = storage::ColumnVector(page.get(), data, bm, 12, count, sizeof(u32), false);
    }
  };
};

// =========================================================================
// 3. LAYOUT
// =========================================================================

TEST_F(ColumnsChunkTest, ColumnsAreAlignedAndDisjoint) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);

  ASSERT_EQ(chunk.GetColCount(), cols.size());
  const byte *prev_end = nullptr;
  for (u16 i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    const byte *col = chunk.GetColumnByIdx(i);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(col) % cols[i].size, 0u) << "misaligned for its width";
    if (prev_end) EXPECT_GE(col, prev_end) << "overlaps previous column or its bitmap";
    prev_end = col + cols[i].size * STANDARD_VECTOR_SIZE + CHUNK_BITMAP_BYTES;
  }
}

TEST_F(ColumnsChunkTest, OidLookupMatchesIndexLookup) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);

  for (u16 i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(chunk.GetColumnByOid(cols[i].oid), chunk.GetColumnByIdx(i));
  }
}

TEST_F(ColumnsChunkTest, UnknownOidThrows) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  U32Source src(4, false);
  u32 v = 1;

  EXPECT_ANY_THROW(chunk.GetColumnByOid(kUnknownOid));
  EXPECT_ANY_THROW(chunk.AppendColumnDataByOid(kUnknownOid, src.vec, 0, 1));
  EXPECT_ANY_THROW(chunk.EmplaceByOid(kUnknownOid, 0, reinterpret_cast<byte *>(&v), nullptr,
                                      Desc(kUnknownOid, 4, false, true)));
}

// =========================================================================
// 4. COUNTS
// =========================================================================

TEST_F(ColumnsChunkTest, FreshChunkIsEmpty) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);

  EXPECT_EQ(chunk.GetTupleCount(), 0);
  EXPECT_TRUE(chunk.HasSpace());
  EXPECT_EQ(chunk.SelCount(), 0);
}

TEST_F(ColumnsChunkTest, AdvanceFillsAndResetEmpties) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);

  chunk.AdvanceTupleCount(STANDARD_VECTOR_SIZE - 1);
  EXPECT_TRUE(chunk.HasSpace());
  chunk.AdvanceTupleCount(1);
  EXPECT_FALSE(chunk.HasSpace());

  chunk.AppendSel(0);
  chunk.ResetTuples();
  EXPECT_EQ(chunk.GetTupleCount(), 0);
  EXPECT_EQ(chunk.SelCount(), 0);
  EXPECT_TRUE(chunk.HasSpace());
}

// =========================================================================
// 5. APPEND, FIXED WIDTH
// =========================================================================

/// Second append must land right after the first one, reading from `start`.
TEST_F(ColumnsChunkTest, AppendCopiesRangeIntoNextRows) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  U32Source src(100, false);

  chunk.AppendColumnDataByIdx(kU32, src.vec, 10, 20); // src 10..29 -> rows 0..19
  chunk.AdvanceTupleCount(20);
  chunk.AppendColumnDataByIdx(kU32, src.vec, 50, 5); // src 50..54 -> rows 20..24
  chunk.AdvanceTupleCount(5);

  for (u16 r = 0; r < 20; ++r) {
    SCOPED_TRACE(r);
    EXPECT_EQ(ReadU32(chunk, kU32, r), 1010u + r);
  }
  for (u16 r = 20; r < 25; ++r) {
    SCOPED_TRACE(r);
    EXPECT_EQ(ReadU32(chunk, kU32, r), 1050u + (r - 20));
  }
}

/// Source and destination offsets that aren't multiples of 8, so NULL bits
/// have to shift across byte boundaries.
TEST_F(ColumnsChunkTest, AppendShiftsNullBitsCorrectly) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  U32Source src(20, true);
  for (u16 r : {3, 4, 9, 15}) src.vec.bitmap_.SetNull(r, true);

  chunk.AppendColumnDataByIdx(kU32, src.vec, 0, 5); // src 0..4  -> rows 0..4
  chunk.AdvanceTupleCount(5);
  chunk.AppendColumnDataByIdx(kU32, src.vec, 3, 13); // src 3..15 -> rows 5..17
  chunk.AdvanceTupleCount(13);

  auto nulls = NullsOf(chunk, kU32, sizeof(u32));
  const std::vector<u16> expected_null = {3, 4, 5, 6, 11, 17};
  for (u16 r = 0; r < 18; ++r) {
    SCOPED_TRACE(r);
    const bool want =
        std::find(expected_null.begin(), expected_null.end(), r) != expected_null.end();
    EXPECT_EQ(nulls.IsNull(r), want);
  }
}

/// A reused chunk still has NULL bits from the previous batch. A source
/// without a bitmap has no NULLs, so every appended row must be cleared.
TEST_F(ColumnsChunkTest, AppendWithoutSourceBitmapClearsStaleNulls) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  auto nulls = NullsOf(chunk, kU32, sizeof(u32));
  for (u16 r = 0; r < 10; ++r) nulls.SetNull(r, true);

  U32Source src(10, false);
  chunk.AppendColumnDataByIdx(kU32, src.vec, 0, 10);
  chunk.AdvanceTupleCount(10);

  for (u16 r = 0; r < 10; ++r) {
    SCOPED_TRACE(r);
    EXPECT_FALSE(nulls.IsNull(r));
  }
}

// =========================================================================
// 6. APPEND, VARLEN
// =========================================================================

TEST_F(ColumnsChunkTest, AppendSwizzlesOnlyLongNonNullStrings) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);

  auto page = Page();
  byte *slots = page.get() + 64;
  ChunkBitmap src_nulls(page.get() + 32);

  const std::string_view s_short = "hi";
  const std::string_view s_long1 = "this string is long A";
  const std::string_view s_long3 = "another long string BB";

  WriteInlineSlot(slots + 0 * kSlot, s_short);
  WriteLongSlot(page.get(), slots + 1 * kSlot, s_long1, 900);
  std::memset(slots + 2 * kSlot, 0xAB, kSlot); // NULL slot: garbage must stay untouched
  src_nulls.SetNull(2, true);
  WriteLongSlot(page.get(), slots + 3 * kSlot, s_long3, 800);

  storage::ColumnVector vec(page.get(), slots, src_nulls, 13, 4, kSlot, true);
  chunk.AppendColumnDataByIdx(kStr, vec, 0, 4);
  chunk.AdvanceTupleCount(4);

  // Inline: byte-for-byte copy.
  EXPECT_EQ(std::memcmp(SlotOf(chunk, kStr, 0), slots, kSlot), 0);

  // Long: length and prefix kept, ref now points into the page heap.
  for (auto [row, s, off] :
       {std::tuple{u16(1), s_long1, u16(900)}, std::tuple{u16(3), s_long3, u16(800)}}) {
    SCOPED_TRACE(row);
    const byte *slot = SlotOf(chunk, kStr, row);
    EXPECT_EQ(std::memcmp(slot, slots + row * kSlot, kRefOff), 0) << "len/prefix changed";
    EXPECT_EQ(RefAsPtr(slot), page.get() + off);
    EXPECT_EQ(View(RefAsPtr(slot), s.size()), s);
  }

  // NULL: not swizzled, bit carried over.
  EXPECT_EQ(std::memcmp(SlotOf(chunk, kStr, 2), slots + 2 * kSlot, kSlot), 0);
  EXPECT_TRUE(NullsOf(chunk, kStr, kSlot).IsNull(2));
}

// =========================================================================
// 7. EMPLACE
// =========================================================================

TEST_F(ColumnsChunkTest, EmplaceOverwritesOneRowAndClearsNull) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  U32Source src(10, false);
  chunk.AppendColumnDataByIdx(kU32, src.vec, 0, 10);
  chunk.AdvanceTupleCount(10);

  auto nulls = NullsOf(chunk, kU32, sizeof(u32));
  nulls.SetNull(7, true);

  u32 v = 42;
  chunk.EmplaceByIdx(kU32, 7, reinterpret_cast<byte *>(&v), nullptr, Desc(12, 4, false, true));

  EXPECT_EQ(ReadU32(chunk, kU32, 7), 42u);
  EXPECT_FALSE(nulls.IsNull(7));
  EXPECT_EQ(ReadU32(chunk, kU32, 6), 1006u) << "neighbour changed";
  EXPECT_EQ(ReadU32(chunk, kU32, 8), 1008u) << "neighbour changed";
}

TEST_F(ColumnsChunkTest, EmplaceNullSetsBitAndKeepsSlot) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  U32Source src(10, false);
  chunk.AppendColumnDataByIdx(kU32, src.vec, 0, 10);
  chunk.AdvanceTupleCount(10);

  chunk.EmplaceByIdx(kU32, 3, nullptr, nullptr, Desc(12, 4, false, true));

  EXPECT_TRUE(NullsOf(chunk, kU32, sizeof(u32)).IsNull(3));
  EXPECT_EQ(ReadU32(chunk, kU32, 3), 1003u);
}

TEST_F(ColumnsChunkTest, EmplaceVarlenSwizzlesAgainstGivenBase) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  chunk.AdvanceTupleCount(1);

  auto buf = Page();
  const std::string_view s = "a long string for emplace";
  byte *slot = buf.get() + 16;
  WriteLongSlot(buf.get(), slot, s, 200);

  chunk.EmplaceByIdx(kStr, 0, slot, buf.get(), Desc(13, kSlot, true, true));

  EXPECT_EQ(RefAsPtr(SlotOf(chunk, kStr, 0)), buf.get() + 200);
  EXPECT_EQ(View(RefAsPtr(SlotOf(chunk, kStr, 0)), s.size()), s);
}

/// An undo delta holds the columns the update changed, which can include
/// columns this scan doesn't project. Those must be skipped, not thrown on.
TEST_F(ColumnsChunkTest, EmplaceDataChunkSkipsUnprojectedColumns) {
  std::vector<ChunkColumn> proj = {{0, 4}, {2, 16}}; // scan reads oids 0 and 2
  ColumnsChunk chunk(proj);
  chunk.AdvanceTupleCount(6);

  std::vector<ChunkColumn> delta_cols = {{0, 4}, {1, 8}, {2, 16}}; // oid 1 not projected
  ChunkPtr layout(DataChunk::BuildDataChunkInitializer(delta_cols));
  ChunkPtr delta(DataChunk::BuildDataChunk(layout.get(), 64));

  const u32 v = 77;
  const u64 other = 0xDEADBEEF;
  const std::string_view s = "replacement long string";
  delta->WriteByIdx(0, {reinterpret_cast<const byte *>(&v), sizeof(v)});
  delta->WriteByIdx(1, {reinterpret_cast<const byte *>(&other), sizeof(other)});
  delta->WriteVarlenByIdx(2, {reinterpret_cast<const byte *>(s.data()), s.size()});

  const std::vector<storage::StorageColumnDesc> descs = {
      Desc(0, 4, false, true), Desc(1, 8, false, true), Desc(2, kSlot, true, true)};

  ASSERT_NO_THROW(chunk.EmplaceDataChunk(5, delta.get(), descs));

  EXPECT_EQ(ReadU32(chunk, 0, 5), 77u);
  const byte *p = RefAsPtr(SlotOf(chunk, 1, 5));
  EXPECT_GE(p, delta->Bytes());
  EXPECT_LT(p, delta->Bytes() + delta->GetSize()) << "must point into the delta's own heap";
  EXPECT_EQ(View(p, s.size()), s);
}

TEST_F(ColumnsChunkTest, EmplaceDataChunkCarriesNulls) {
  std::vector<ChunkColumn> proj = {{0, 4}};
  ColumnsChunk chunk(proj);
  chunk.AdvanceTupleCount(3);

  std::vector<ChunkColumn> delta_cols = {{0, 4}};
  ChunkPtr layout(DataChunk::BuildDataChunkInitializer(delta_cols));
  ChunkPtr delta(DataChunk::BuildDataChunk(layout.get(), 0));
  delta->SetNull(0, true);

  const std::vector<storage::StorageColumnDesc> descs = {Desc(0, 4, false, true)};
  chunk.EmplaceDataChunk(2, delta.get(), descs);

  EXPECT_TRUE(NullsOf(chunk, 0, sizeof(u32)).IsNull(2));
}

// =========================================================================
// 8. SELECTION
// =========================================================================

TEST_F(ColumnsChunkTest, AppendSelRecordsRowsInOrder) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  chunk.AdvanceTupleCount(10);

  chunk.AppendSel(1);
  chunk.AppendSel(4);
  chunk.AppendSel(9);

  ASSERT_EQ(chunk.SelCount(), 3);
  EXPECT_EQ(chunk.SelData()[0], 1);
  EXPECT_EQ(chunk.SelData()[1], 4);
  EXPECT_EQ(chunk.SelData()[2], 9);
}

// =========================================================================
// 9. BEHAVIOURAL ASSUMPTIONS
//
//    These encode the intended contract. Keep them or invert them.
// =========================================================================

/// Assumes: appending a column doesn't advance the row count. The caller
/// appends every column with the same (start, n) and then advances once.
TEST_F(ColumnsChunkTest, AppendDoesNotAdvanceCount) {
  auto cols = Schema();
  ColumnsChunk chunk(cols);
  U32Source src(10, false);

  chunk.AppendColumnDataByIdx(kU32, src.vec, 0, 10);
  EXPECT_EQ(chunk.GetTupleCount(), 0);
}

} // namespace
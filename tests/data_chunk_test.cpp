#include <gtest/gtest.h>

#include "storage/data_chunk.hpp"

#include <cstring>
#include <memory>
#include <span>
#include <vector>

using namespace db7;

namespace {

// =========================================================================
// 1. HELPERS
// =========================================================================

/// BuildDataChunk() returns `new byte[size]` reinterpreted as a DataChunk*,
/// so it must be released as a byte array, never through `delete chunk`.
struct ChunkDeleter {
  void operator()(DataChunk *chunk) const noexcept { DataChunk::Destroy(chunk); }
};

using ChunkPtr = std::unique_ptr<DataChunk, ChunkDeleter>;

/// Distinct, non-zero byte pattern per seed, so a column that ends up holding
/// its neighbour's bytes (or zeros) is caught by a memcmp.
std::vector<byte> Pattern(size_t n, unsigned seed) {
  std::vector<byte> v(n);
  for (size_t i = 0; i < n; ++i) { v[i] = static_cast<byte>((seed * 37 + i * 7 + 1) & 0xFF); }
  return v;
}

std::span<const byte> Span(const std::vector<byte> &v) { return {v.data(), v.size()}; }

/// Header as documented: [size u32][count u16][ids][offsets][null bitmap]
size_t ExpectedHeaderSize(size_t n) {
  return sizeof(DataChunk) + n * sizeof(column_t) + n * sizeof(u16) + (n + 7) / 8;
}

/// Byte offset of column `idx` from the start of the chunk.
size_t RelOffset(const DataChunk &chunk, u16 idx) {
  return static_cast<size_t>(chunk.GetByIdx(idx) - reinterpret_cast<const byte *>(&chunk));
}

// =========================================================================
// 2. FIXTURE
// =========================================================================

class DataChunkTest : public ::testing::Test {
protected:
  /// Mixed widths in an order that forces padding (1 -> 8 needs 7 bytes),
  /// with non-sequential oids so idx and oid can't be confused.
  static std::vector<ChunkColumn> Schema() {
    return {
        {10, 1},  // kU8
        {11, 8},  // kU64
        {12, 2},  // kU16
        {13, 4},  // kU32
        {14, 16}, // kRaw, varlen-slot sized
    };
  }

  enum ColIdx : u16 { kU8 = 0, kU64 = 1, kU16 = 2, kU32 = 3, kRaw = 4 };

  static constexpr column_t kUnknownOid = 999;

  static ChunkPtr Build(std::vector<ChunkColumn> cols) {
    return ChunkPtr(DataChunk::BuildDataChunk(cols));
  }

  /// n one-byte columns: the null bitmap spans several bytes.
  static ChunkPtr Wide(size_t n) {
    std::vector<ChunkColumn> cols;
    for (size_t i = 0; i < n; ++i) { cols.emplace_back(static_cast<column_t>(100 + i), 1); }
    return Build(cols);
  }

  /// Writes a distinct pattern into every column, full width. Returns them.
  static std::vector<std::vector<byte>> FillAll(DataChunk &chunk,
                                                const std::vector<ChunkColumn> &cols) {
    std::vector<std::vector<byte>> patterns;
    for (u16 i = 0; i < cols.size(); ++i) {
      patterns.push_back(Pattern(cols[i].size, i + 1));
      chunk.WriteByIdx(i, Span(patterns.back()));
    }
    return patterns;
  }

  static void ExpectHolds(const DataChunk &chunk, u16 idx, const std::vector<byte> &bytes) {
    EXPECT_EQ(std::memcmp(chunk.GetByIdx(idx), bytes.data(), bytes.size()), 0)
        << "column " << idx << " does not hold its own bytes";
  }
};

// =========================================================================
// 3. LAYOUT
//
//    If the header or offsets are wrong, every other test reads garbage.
//    These pin the layout down first.
// =========================================================================

TEST_F(DataChunkTest, HeaderMatchesDocumentedLayout) {
  const auto cols = Schema();
  ChunkPtr chunk = Build(cols);

  ASSERT_EQ(chunk->GetColCount(), cols.size());
  for (size_t i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(chunk->ColumnIds()[i], cols[i].oid);
  }

  EXPECT_EQ(chunk->GetHeaderSpan().size(), ExpectedHeaderSize(cols.size()));
  EXPECT_EQ(chunk->GetSize() % sizeof(u32), 0u) << "total size must be u32-aligned";
}

TEST_F(DataChunkTest, ColumnsAreAlignedDisjointAndInBounds) {
  const auto cols = Schema();
  ChunkPtr chunk = Build(cols);

  size_t prev_end = ExpectedHeaderSize(cols.size());
  for (u16 i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    const size_t off = RelOffset(*chunk, i);

    EXPECT_GE(off, prev_end) << "overlaps header or previous column";
    EXPECT_EQ(off % cols[i].size, 0u) << "misaligned for its width";
    prev_end = off + cols[i].size;
  }
  EXPECT_LE(prev_end, chunk->GetSize()) << "last column runs past the chunk";
}

// =========================================================================
// 4. VALUES
// =========================================================================

TEST_F(DataChunkTest, TypedValuesRoundTrip) {
  ChunkPtr chunk = Build(Schema());

  chunk->Set<u8>(kU8, u8{0xAB}, false);
  chunk->Set<u64>(kU64, u64{0x0123456789ABCDEFull}, false);
  chunk->Set<u16>(kU16, u16{0xBEEF}, false);
  chunk->Set<u32>(kU32, u32{0xDEADBEEF}, false);

  bool is_null = true;
  EXPECT_EQ(chunk->Get<u8>(kU8, is_null), u8{0xAB});
  EXPECT_FALSE(is_null);
  EXPECT_EQ(chunk->Get<u64>(kU64, is_null), u64{0x0123456789ABCDEFull});
  EXPECT_FALSE(is_null);
  EXPECT_EQ(chunk->Get<u16>(kU16, is_null), u16{0xBEEF});
  EXPECT_FALSE(is_null);
  EXPECT_EQ(chunk->Get<u32>(kU32, is_null), u32{0xDEADBEEF});
  EXPECT_FALSE(is_null);
}

/// Full-width writes into every column. If two offsets overlap, or a column
/// overlaps the bitmap, a later write clobbers an earlier one.
TEST_F(DataChunkTest, WritesDoNotBleedIntoNeighbours) {
  const auto cols = Schema();
  ChunkPtr chunk = Build(cols);

  const auto patterns = FillAll(*chunk, cols);

  for (u16 i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    ExpectHolds(*chunk, i, patterns[i]);
    EXPECT_FALSE(chunk->IsNull(i)) << "data write flipped a null bit";
  }
}

// =========================================================================
// 5. NULLS
// =========================================================================

TEST_F(DataChunkTest, FreshChunkHasNoNulls) {
  const auto cols = Schema();
  ChunkPtr chunk = Build(cols);

  for (u16 i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(chunk->IsNull(i));
    EXPECT_NE(chunk->GetByIdxNullCheck(i), nullptr);
  }
}

/// 20 columns -> 3 bitmap bytes. Catches wrong byte/bit indexing and bits
/// that leak into neighbouring columns or across byte boundaries.
TEST_F(DataChunkTest, NullBitsAreIndependentAcrossBitmapBytes) {
  constexpr u16 kN = 20;
  ChunkPtr chunk = Wide(kN);
  auto is_target = [](u16 i) { return i % 3 == 0; };

  for (u16 i = 0; i < kN; ++i) {
    if (is_target(i)) { chunk->SetNull(i, true); }
  }
  for (u16 i = 0; i < kN; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(chunk->IsNull(i), is_target(i));
    EXPECT_EQ(chunk->GetByIdxNullCheck(i) == nullptr, is_target(i));
  }

  for (u16 i = 0; i < kN; ++i) { chunk->SetNull(i, false); }
  for (u16 i = 0; i < kN; ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(chunk->IsNull(i));
  }
}

/// The bitmap sits directly before the data in a chunk of 1-byte columns,
/// so this is the tightest place for an off-by-one between the two.
TEST_F(DataChunkTest, NullBitmapAndDataDoNotOverlap) {
  constexpr u16 kN = 20;
  ChunkPtr chunk = Wide(kN);
  const std::vector<byte> ones(1, static_cast<byte>(0xFF));

  for (u16 i = 0; i < kN; ++i) { chunk->WriteByIdx(i, Span(ones)); }
  for (u16 i = 0; i < kN; ++i) {
    SCOPED_TRACE(i);
    EXPECT_FALSE(chunk->IsNull(i)) << "data write landed in the bitmap";
  }

  for (u16 i = 0; i < kN; ++i) { chunk->SetNull(i, true); }
  for (u16 i = 0; i < kN; ++i) {
    SCOPED_TRACE(i);
    ExpectHolds(*chunk, i, ones);
  }
}

TEST_F(DataChunkTest, TypedSetAndGetTrackNull) {
  ChunkPtr chunk = Build(Schema());
  bool is_null = false;

  chunk->Set<u32>(kU32, u32{7}, true);
  EXPECT_EQ(chunk->Get<u32>(kU32, is_null), u32{0}) << "null must read as T{}";
  EXPECT_TRUE(is_null);

  chunk->Set<u32>(kU32, u32{7}, false);
  EXPECT_EQ(chunk->Get<u32>(kU32, is_null), u32{7});
  EXPECT_FALSE(is_null);
}

TEST_F(DataChunkTest, NullCheckedWriteOfNullptrSetsNull) {
  ChunkPtr chunk = Build(Schema());

  chunk->WriteByIdxNullCheck(kU32, {});
  EXPECT_TRUE(chunk->IsNull(kU32));

  chunk->WriteByOidNullCheck(Schema()[kU16].oid, {});
  EXPECT_TRUE(chunk->IsNull(kU16));

  EXPECT_FALSE(chunk->IsNull(kU8)) << "other columns must be untouched";
}

// =========================================================================
// 6. OID LOOKUP
// =========================================================================

TEST_F(DataChunkTest, OidLookupMatchesIndexLookup) {
  const auto cols = Schema();
  ChunkPtr chunk = Build(cols);

  for (u16 i = 0; i < cols.size(); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(chunk->GetByOid(cols[i].oid), chunk->GetByIdx(i));
  }

  const auto bytes = Pattern(cols[kU32].size, 42);
  chunk->WriteByOid(cols[kU32].oid, Span(bytes));
  ExpectHolds(*chunk, kU32, bytes);

  chunk->SetNull(kU64, true);
  EXPECT_EQ(chunk->GetByOidNullCheck(cols[kU64].oid), nullptr);
  EXPECT_NE(chunk->GetByOidNullCheck(cols[kU32].oid), nullptr);
}

TEST_F(DataChunkTest, UnknownOidThrows) {
  ChunkPtr chunk = Build(Schema());
  const auto bytes = Pattern(4, 1);

  EXPECT_ANY_THROW(chunk->GetByOid(kUnknownOid));
  EXPECT_ANY_THROW(chunk->GetByOidNullCheck(kUnknownOid));
  EXPECT_ANY_THROW(chunk->WriteByOid(kUnknownOid, Span(bytes)));
}

// =========================================================================
// 7. COPY
//
//    MakeKey() in the B-tree tests copies a layout chunk and fills it, so a
//    shallow or truncated Copy() corrupts every key.
// =========================================================================

TEST_F(DataChunkTest, CopyIsByteIdenticalAndIndependent) {
  const auto cols = Schema();
  ChunkPtr original = Build(cols);
  const auto patterns = FillAll(*original, cols);
  original->SetNull(kU16, true);

  ChunkPtr copy(original->Copy());
  ASSERT_EQ(copy->GetSize(), original->GetSize());
  EXPECT_EQ(std::memcmp(copy.get(), original.get(), original->GetSize()), 0);

  // Mutate the copy, the original must not move.
  const auto other = Pattern(cols[kU32].size, 99);
  copy->WriteByIdx(kU32, Span(other));
  copy->SetNull(kU16, false);
  copy->SetNull(kU8, true);

  ExpectHolds(*original, kU32, patterns[kU32]);
  EXPECT_TRUE(original->IsNull(kU16));
  EXPECT_FALSE(original->IsNull(kU8));
}

// =========================================================================
// 8. BEHAVIOURAL ASSUMPTIONS
//
//    These encode a guess about intended semantics. Read each one and
//    either keep it (it now documents the contract) or invert it.
// =========================================================================

/// Assumes: writing a value makes the column non-null, like Set<T>(.., false)
/// already does. Without this, a chunk reused for a second row keeps the
/// first row's NULLs, and copies of a layout that had a NULL stay NULL.
TEST_F(DataChunkTest, WritingAValueClearsNull) {
  const auto cols = Schema();
  ChunkPtr chunk = Build(cols);
  const auto bytes = Pattern(cols[kU32].size, 5);

  chunk->SetNull(kU32, true);
  chunk->WriteByIdx(kU32, Span(bytes));
  EXPECT_TRUE(chunk->IsNull(kU32)) << "WriteByIdx";

  chunk->SetNull(kU32, true);
  chunk->WriteByIdxNullCheck(kU32, Span(bytes));
  EXPECT_FALSE(chunk->IsNull(kU32)) << "WriteByIdxNullCheck";

  chunk->SetNull(kU32, true);
  chunk->WriteByOid(cols[kU32].oid, Span(bytes));
  EXPECT_TRUE(chunk->IsNull(kU32)) << "WriteByOid";

  chunk->SetNull(kU32, true);
  chunk->WriteByOidNullCheck(cols[kU32].oid, Span(bytes));
  EXPECT_FALSE(chunk->IsNull(kU32)) << "WriteByOidNullCheck";
}

} // namespace
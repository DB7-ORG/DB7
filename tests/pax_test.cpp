#include <gtest/gtest.h>

#include "storage/layouts/pax.hpp" // adjust to wherever Pax / PaxHeader live

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

using namespace db7;
using namespace db7::storage;

namespace {

// =========================================================================
// 1. SCHEMAS AND VALUES
//
//    Position in a Schema is the column id: ADD COLUMN appends, DROP COLUMN
//    only sets `dropped`, so index c is the same column in every version.
// =========================================================================

using Null = std::monostate;
using Value = std::variant<Null, bool, int32_t, double, std::string>;

struct ColDef {
  type_id type{};
  bool nullable = false;
  bool dropped = false;
  Value dflt{}; ///< stored for transactions that predate the column
};
using Schema = std::vector<ColDef>;

/// A transaction's row by column id; nullopt where its schema dropped the column.
using TxnRow = std::vector<std::optional<Value>>;

ColDef Col(type_id t, bool nullable, Value dflt) {
  return ColDef{t, nullable, false, std::move(dflt)};
}

enum BaseCol : size_t { kFlag, kId, kName, kScore, kCount, kNote };

/// Mixed widths force padding before every minipage; two nullable columns
/// put bitmaps in the middle and at the end.
Schema BaseSchema() {
  return {Col(type_id::BOOLEAN, false, false),         Col(type_id::INTEGER, false, int32_t{0}),
          Col(type_id::VARCHAR, false, std::string()), Col(type_id::DOUBLE, true, 0.0),
          Col(type_id::INTEGER, false, int32_t{0}),    Col(type_id::VARCHAR, true, std::string())};
}

// =========================================================================
// 2. API ADAPTER: every call into Pax, and every encoding assumption.
// =========================================================================
namespace api {

/// ASSUMPTION: a string is [u16 len][bytes] in the chunk and in the page.
/// It stays in the slot if it fits; otherwise the slot holds its first
/// kSlot-2 bytes plus a u16 heap offset, and the rest is at that offset.
using LenPrefix = u16;
constexpr size_t kSlot = SizeOf(type_id::VARCHAR);
constexpr size_t kInline = kSlot - sizeof(u16);
constexpr size_t kMaxInlineLen = kSlot - sizeof(LenPrefix);

u16 EncodedSize(type_id t, const Value &v) {
  if (t != type_id::VARCHAR) { return SizeOf(t); }
  if (std::holds_alternative<Null>(v)) { return sizeof(LenPrefix); }
  return static_cast<u16>(sizeof(LenPrefix) + std::get<std::string>(v).size());
}

struct Column {
  column_t oid;
  type_id type;
  Value value;
};

std::vector<PaxInsertInfo> Infos(const std::vector<Column> &cols) {
  std::vector<PaxInsertInfo> out;
  for (const Column &c : cols) {
    out.push_back({EncodedSize(c.type, c.value), static_cast<u16>(SizeOf(c.type)), c.type});
  }
  return out;
}

void Init(byte *page, const Schema &schema) {
  std::vector<ColumnStatistics> stats;
  for (const ColDef &c : schema) {
    // avg_size includes the length prefix and sits a bit above the slot so Init reserves heap.
    const u16 avg = c.type == type_id::VARCHAR ? static_cast<u16>(kSlot + 8) : SizeOf(c.type);
    stats.emplace_back(c.type, c.dropped, avg, c.nullable);
  }
  Pax::Init(page, stats);
}

/// `catalog_cols` = the inserting transaction's column count, dropped included.
bool Insert(byte *page, const std::vector<Column> &cols, u16 catalog_cols) {
  std::vector<ChunkColumn> layout;
  for (const Column &c : cols) { layout.emplace_back(c.oid, EncodedSize(c.type, c.value)); }
  std::unique_ptr<DataChunk, void (*)(DataChunk *)> chunk(DataChunk::BuildDataChunk(layout),
                                                          &DataChunk::Destroy);

  for (u16 i = 0; i < cols.size(); ++i) {
    std::visit(
        [&](const auto &x) {
          using T = std::decay_t<decltype(x)>;
          if constexpr (std::is_same_v<T, Null>) {
            chunk->SetNull(i, true);
          } else if constexpr (std::is_same_v<T, std::string>) {
            std::vector<byte> enc(sizeof(LenPrefix) + x.size());
            const auto len = static_cast<LenPrefix>(x.size());
            std::memcpy(enc.data(), &len, sizeof(len));
            std::memcpy(enc.data() + sizeof(len), x.data(), x.size());
            chunk->WriteByIdx(i, enc);
          } else {
            chunk->WriteByIdx(i,
                              std::span<const byte>(reinterpret_cast<const byte *>(&x), sizeof(T)));
          }
        },
        cols[i].value);
  }

  std::vector<PaxInsertInfo> info = Infos(cols);
  return Pax::Insert(page, chunk.get(), info, Pax::CalcHeapBytes(info), catalog_cols);
}

/// ASSUMPTION: the layer above Pax shapes the chunk to the PAGE's live
/// columns (oid = column id), as the comment on Pax::Insert describes:
/// dropped page columns are left out, columns the txn predates get their
/// default, and columns the txn dropped get a placeholder. Columns the page
/// predates aren't in the chunk at all; Pax sees them through the catalog
/// column count instead.
std::vector<Column> ShapeForPage(const Schema &page, const Schema &txn, const TxnRow &row) {
  std::vector<Column> out;
  for (size_t c = 0; c < page.size(); ++c) {
    if (page[c].dropped) { continue; }
    Value v;
    if (c >= txn.size()) {
      v = page[c].dflt; // txn predates the column
    } else if (txn[c].dropped) {
      v = page[c].nullable ? Value{Null{}} : page[c].dflt; // dead column, any value will do
    } else {
      v = *row[c];
    }
    out.push_back({static_cast<column_t>(c), page[c].type, std::move(v)});
  }
  return out;
}

} // namespace api

// =========================================================================
// 3. PAGE READER, written only from the layout comment in pax.hpp.
// =========================================================================
struct PageView {
  byte *page;

  u16 Raw(size_t c) const {
    u16 r;
    std::memcpy(&r, PaxHeader::Offsets(page) + c, sizeof(r));
    return r;
  }
  bool Nullable(size_t c) const { return (Raw(c) & PaxHeader::offset_nullable_mask_) != 0; }
  size_t Offset(size_t c) const {
    return static_cast<u16>(Raw(c) & ~PaxHeader::offset_nullable_mask_);
  }

  /// ASSUMPTION: tuple i is bit (i % 8) of byte (i / 8).
  bool IsNull(size_t c, type_id t, size_t i) const {
    const byte *bitmap = page + Offset(c) + size_t{PaxHeader::Capacity(page)} * SizeOf(t);
    return ((static_cast<u8>(bitmap[i / 8]) >> (i % 8)) & 1u) != 0;
  }

  Value Read(size_t c, type_id t, size_t i) const {
    if (Nullable(c) && IsNull(c, t, i)) { return Null{}; }
    const byte *slot = page + Offset(c) + i * SizeOf(t);
    switch (t) {
    case type_id::BOOLEAN: return static_cast<u8>(slot[0]) != 0;
    case type_id::INTEGER: {
      int32_t v;
      std::memcpy(&v, slot, 4);
      return v;
    }
    case type_id::DOUBLE: {
      double v;
      std::memcpy(&v, slot, 8);
      return v;
    }
    case type_id::VARCHAR: return ReadVarlen(slot);
    default: return Null{};
    }
  }

  std::string ReadVarlen(const byte *slot) const {
    api::LenPrefix len;
    std::memcpy(&len, slot, sizeof(len));
    std::string enc(sizeof(len) + len, '\0');
    if (enc.size() <= api::kSlot) {
      std::memcpy(enc.data(), slot, enc.size());
    } else {
      u16 heap;
      std::memcpy(&heap, slot + api::kInline, sizeof(heap));
      if (heap < PaxHeader::CurrHeapOffset(page) ||
          heap + enc.size() - api::kInline > DB7_PAGE_SIZE) {
        return "<bad heap offset " + std::to_string(heap) + ">";
      }
      std::memcpy(enc.data(), slot, api::kInline);
      std::memcpy(enc.data() + api::kInline, page + heap, enc.size() - api::kInline);
    }
    return enc.substr(sizeof(len));
  }
};

// =========================================================================
// 4. ROWS AND FIXTURE
// =========================================================================

std::string Pattern(size_t len, size_t seed) {
  std::string s(len, 'a');
  for (size_t k = 0; k < len; ++k) { s[k] = static_cast<char>('a' + (seed * 31 + k * 13) % 26); }
  return s;
}

/// Row i for a transaction on `txn`. Every 5th row is NULL in nullable
/// columns; strings fit in their slot unless `spill`.
TxnRow NthRow(const Schema &txn, size_t i, bool spill = false) {
  TxnRow row(txn.size());
  for (size_t c = 0; c < txn.size(); ++c) {
    if (txn[c].dropped) { continue; }
    if (txn[c].nullable && i % 5 == 3) {
      row[c] = Null{};
      continue;
    }
    switch (txn[c].type) {
    case type_id::BOOLEAN: row[c] = (i + c) % 2 == 0; break;
    case type_id::INTEGER: row[c] = static_cast<int32_t>(i * 2654435761u + c); break;
    case type_id::DOUBLE: row[c] = i * 1.5 + c; break;
    default:
      row[c] = Pattern(spill ? 2 * api::kSlot + i % 7 : std::min<size_t>(api::kMaxInlineLen, 10),
                       i * 8 + c);
    }
  }
  return row;
}

class PaxTest : public ::testing::Test {
protected:
  alignas(64) byte page_[DB7_PAGE_SIZE];
  Schema page_schema_;

  /// Starts from garbage, as a recycled buffer-pool frame would.
  void InitPage(Schema s) {
    std::memset(page_, 0xA5, sizeof(page_));
    page_schema_ = std::move(s);
    api::Init(page_, page_schema_);
  }

  size_t Count() { return PaxHeader::TupleCount(page_); }
  size_t Capacity() { return PaxHeader::Capacity(page_); }

  /// Inserts the row; if Pax refuses it, checks the page is byte-identical.
  [[nodiscard]] bool Insert(const Schema &txn, const TxnRow &row) {
    const std::vector<byte> before(page_, page_ + sizeof(page_));
    const bool ok =
        api::Insert(page_, api::ShapeForPage(page_schema_, txn, row), static_cast<u16>(txn.size()));
    if (!ok) {
      EXPECT_EQ(std::memcmp(before.data(), page_, sizeof(page_)), 0)
          << "refused insert changed the page";
    }
    return ok;
  }

  /// Header and minipages match page_schema_ and the layout doc.
  void ExpectValidLayout() {
    const PageView v{page_};
    ASSERT_EQ(size_t{PaxHeader::ColumnCount(page_)}, page_schema_.size());
    EXPECT_EQ(Count(), 0u);
    ASSERT_GT(Capacity(), 0u);
    EXPECT_EQ(size_t{PaxHeader::CurrHeapOffset(page_)}, size_t{DB7_PAGE_SIZE});

    size_t prev_end = PaxHeader::fixed_header_size_ + page_schema_.size() * sizeof(u16);
    for (size_t c = 0; c < page_schema_.size(); ++c) {
      SCOPED_TRACE(::testing::Message() << "column " << c);
      const ColDef &col = page_schema_[c];
      if (col.dropped) {
        EXPECT_EQ(v.Raw(c), u16{0});
        continue;
      }
      const size_t off = v.Offset(c), width = SizeOf(col.type);
      EXPECT_EQ(v.Nullable(c), col.nullable);
      EXPECT_EQ(off % width, 0u) << "unaligned minipage";
      EXPECT_GE(off, prev_end) << "minipage overlaps header/previous minipage";
      prev_end = off + Capacity() * width + (col.nullable ? (Capacity() + 7) / 8 : 0);
    }
    EXPECT_LE(prev_end, size_t{PaxHeader::MaxHeapOffset(page_)}) << "minipages run into the heap";
  }

  /// Contract for one stored tuple, per column (page state x txn state):
  ///   page live,    txn live    -> txn's value
  ///   page live,    txn dropped -> anything (dead column)
  ///   page live,    txn older   -> column default
  ///   page dropped              -> no minipage
  void ExpectTuple(size_t i, const Schema &txn, const TxnRow &row) {
    const PageView v{page_};
    for (size_t c = 0; c < page_schema_.size(); ++c) {
      SCOPED_TRACE(::testing::Message() << "tuple " << i << ", column " << c);
      const ColDef &col = page_schema_[c];
      if (col.dropped) {
        EXPECT_EQ(v.Raw(c), u16{0});
      } else if (c >= txn.size()) {
        EXPECT_EQ(v.Read(c, col.type, i), col.dflt);
      } else if (!txn[c].dropped) {
        EXPECT_EQ(v.Read(c, col.type, i), *row[c]);
      }
    }
  }

  /// ASSUMPTION: Pax refuses the row when the catalog has more columns
  /// than the page (the page predates an ADD COLUMN), even if that column
  /// was dropped again since. Fewer is fine: the chunk carries defaults.
  /// If Pax demands an exact match, make this `txn.size() == page.size()`.
  static bool PageCanTake(const Schema &page, const Schema &txn) {
    return txn.size() <= page.size();
  }
};

// =========================================================================
// 5. SAME SCHEMA
// =========================================================================

TEST_F(PaxTest, InitProducesValidLayout) {
  InitPage(BaseSchema());
  ExpectValidLayout();
}

/// Short strings use no heap, so the page must take exactly Capacity rows.
TEST_F(PaxTest, FillsExactlyToCapacity) {
  InitPage(BaseSchema());
  size_t n = 0;
  while (n <= Capacity() && Insert(page_schema_, NthRow(page_schema_, n))) { ++n; }
  EXPECT_EQ(n, Capacity());
  EXPECT_EQ(Count(), n);
  for (size_t i = 0; i < n; ++i) { ExpectTuple(i, page_schema_, NthRow(page_schema_, i)); }
}

/// Spilling strings exhaust the heap first; it must never grow into the minipages.
TEST_F(PaxTest, StopsWhenHeapIsFull) {
  InitPage(BaseSchema());
  size_t n = 0;
  while (n <= Capacity() && Insert(page_schema_, NthRow(page_schema_, n, true))) { ++n; }
  EXPECT_GT(n, 0u);
  EXPECT_LE(n, Capacity());
  EXPECT_GE(PaxHeader::CurrHeapOffset(page_), PaxHeader::MaxHeapOffset(page_));
  for (size_t i = 0; i < n; ++i) { ExpectTuple(i, page_schema_, NthRow(page_schema_, i, true)); }
}

/// Inline vs spill off-by-ones live right at the slot boundary.
TEST_F(PaxTest, VarlenAtSlotBoundary) {
  using api::kMaxInlineLen;
  const std::vector<size_t> lens = {
      0, 1, kMaxInlineLen - 1, kMaxInlineLen, kMaxInlineLen + 1, api::kSlot, 2 * api::kSlot, 1000};
  InitPage(BaseSchema());
  std::vector<TxnRow> rows;
  for (size_t i = 0; i < lens.size(); ++i) {
    rows.push_back(NthRow(page_schema_, i * 5)); // i*5 % 5 != 3: no NULLs
    rows.back()[kName] = Pattern(lens[i], i);
    ASSERT_TRUE(Insert(page_schema_, rows.back())) << "length " << lens[i];
  }
  for (size_t i = 0; i < lens.size(); ++i) { ExpectTuple(i, page_schema_, rows[i]); }
}

/// NULLs either side of bitmap byte boundaries, in two columns.
TEST_F(PaxTest, NullsSetOnlyTheirOwnBit) {
  InitPage(BaseSchema());
  const std::vector<size_t> nulls = {0, 7, 8, 15, 16};
  std::vector<TxnRow> rows;
  for (size_t i = 0; i < 20; ++i) {
    const bool is_null = std::find(nulls.begin(), nulls.end(), i) != nulls.end();
    rows.push_back(NthRow(page_schema_, i * 5));
    rows.back()[kScore] = is_null ? Value{Null{}} : Value{i * 0.5};
    rows.back()[kNote] = (i == 8) ? Value{Null{}} : Value{Pattern(3, i)};
    ASSERT_TRUE(Insert(page_schema_, rows.back())) << "insert " << i;
  }
  for (size_t i = 0; i < rows.size(); ++i) { ExpectTuple(i, page_schema_, rows[i]); }
}

/// NOT NULL column, or a page written before ALTER ... DROP NOT NULL:
/// either way there's no bitmap, so the NULL is refused.
TEST_F(PaxTest, NullWithoutBitmapIsRefused) {
  Schema old_s = BaseSchema();
  old_s[kScore].nullable = false;
  const Schema new_s = BaseSchema();
  InitPage(old_s);

  TxnRow row = NthRow(new_s, 0);
  row[kScore] = Null{};
  EXPECT_FALSE(Insert(new_s, row));

  row[kScore] = 1.0;
  row[kId] = Null{};
  EXPECT_FALSE(Insert(new_s, row));

  EXPECT_EQ(Count(), 0u);
  ASSERT_TRUE(Insert(new_s, NthRow(new_s, 1))); // non-NULL rows still fit
  ExpectTuple(0, new_s, NthRow(new_s, 1));
}

// =========================================================================
// 6. SCHEMA EVOLUTION
//
//    Each scenario turns BaseSchema() (old) into a new schema. The page is
//    Init'd on one of them, then old and new transactions take turns
//    inserting until the page is full. That covers all four combinations:
//    page old/new x transaction old/new.
// =========================================================================

struct Evolution {
  std::string name;
  std::vector<size_t> drops;
  std::vector<ColDef> adds;
  bool page_on_new;
};

Schema Apply(Schema s, const Evolution &e) {
  for (const ColDef &c : e.adds) { s.push_back(c); }
  for (size_t c : e.drops) { s[c].dropped = true; }
  return s;
}

std::vector<Evolution> Evolutions() {
  const ColDef i32 = Col(type_id::INTEGER, false, int32_t{42});
  const ColDef str =
      Col(type_id::VARCHAR, false, std::string(3 * api::kSlot, 'd')); // spilling default
  const ColDef nul = Col(type_id::BOOLEAN, true, true);

  const std::vector<Evolution> base = {
      {"DropFirst", {kFlag}, {}, false},
      {"DropMiddleVarlen", {kName}, {}, false},
      {"DropLastNullable", {kNote}, {}, false},
      {"DropMany", {kFlag, kName, kCount}, {}, false},
      {"AddOne", {}, {i32}, false},
      {"AddMany", {}, {i32, str, nul}, false},
      {"DropAndAdd", {kName, kScore}, {i32, str}, false},
      {"AddThenDropIt", {6}, {i32}, false},
  };
  std::vector<Evolution> out;
  for (Evolution e : base) {
    out.push_back(e);
    e.page_on_new = true;
    out.push_back(e);
  }
  return out;
}

class PaxEvolutionTest : public PaxTest, public ::testing::WithParamInterface<Evolution> {};

TEST_P(PaxEvolutionTest, OldAndNewTransactionsShareOnePage) {
  const Schema old_s = BaseSchema();
  const Schema new_s = Apply(old_s, GetParam());
  InitPage(GetParam().page_on_new ? new_s : old_s);
  ExpectValidLayout();

  std::vector<std::pair<const Schema *, TxnRow>> stored;
  for (size_t i = 0; i < 2 * Capacity() + 2; ++i) {
    const Schema &txn = i % 2 ? new_s : old_s;
    TxnRow row = NthRow(txn, i);
    const bool ok = Insert(txn, row);
    if (!PageCanTake(page_schema_, txn)) {
      EXPECT_FALSE(ok) << "row " << i << " has a column this page predates";
      continue;
    }
    if (!ok) { break; } // full
    stored.emplace_back(&txn, std::move(row));
  }

  EXPECT_GT(stored.size(), 0u);
  EXPECT_EQ(Count(), stored.size());
  for (size_t i = 0; i < stored.size(); ++i) { ExpectTuple(i, *stored[i].first, stored[i].second); }
}

INSTANTIATE_TEST_SUITE_P(SchemaEvolution, PaxEvolutionTest, ::testing::ValuesIn(Evolutions()),
                         [](const auto &info) {
                           return info.param.name +
                                  (info.param.page_on_new ? "_PageNew" : "_PageOld");
                         });

} // namespace
#include <gtest/gtest.h>

#include "storage/layouts/pax.hpp"

#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

using namespace db7;
using namespace db7::storage;

namespace db7::storage {
inline void PrintTo(PaxInsertResponse r, std::ostream *os) {
  static const char *names[] = {"OK",
                                "NO_SPACE_HEAP",
                                "NO_SPACE_CAPACITY",
                                "PAGE_SCHEMA_OLDER",
                                "WRITER_SCHEMA_STALE",
                                "COLUMN_DROPPED"};
  *os << names[r];
}
} // namespace db7::storage

namespace {

// ---------------------------------------------------------------------------
// Model. A schema is a list of columns indexed by column id. ADD COLUMN
// appends, DROP COLUMN only marks the column dropped, so an id means the same
// column in every schema version.
// ---------------------------------------------------------------------------

using Null = std::monostate;
using Value = std::variant<Null, bool, int32_t, double, std::string>;
using Row = std::map<column_t, Value>; // live columns only, by column id

struct Col {
  type_id type;
  bool nullable = false;
  bool dropped = false;
  Value dflt = Null{}; // what readers get on pages that predate the column
};
using Schema = std::vector<Col>;

enum : column_t { kFlag, kId, kName, kScore, kAdded };

Schema Base() {
  return {
      {type_id::BOOLEAN}, {type_id::INTEGER}, {type_id::VARCHAR, true}, {type_id::DOUBLE, true}};
}
Schema Add(Schema s, Col c) {
  s.push_back(std::move(c));
  return s;
}
Schema Drop(Schema s, column_t id) {
  s[id].dropped = true;
  return s;
}
Schema NotNull(Schema s, column_t id) {
  s[id].nullable = false;
  return s;
}

Row BaseRow() {
  return {{kFlag, true}, {kId, int32_t{7}}, {kName, std::string("hello")}, {kScore, 2.5}};
}
Row With(Row r, column_t id, Value v) {
  r[id] = std::move(v);
  return r;
}
Row Without(Row r, column_t id) {
  r.erase(id);
  return r;
}
Row Nth(size_t i, size_t name_len) {
  return {{kFlag, i % 2 == 0},
          {kId, static_cast<int32_t>(i)},
          {kName, std::string(name_len, static_cast<char>('a' + i % 26))},
          {kScore, i * 0.5}};
}

const Col kAddedInt{type_id::INTEGER, false, false, int32_t{42}};

// ---------------------------------------------------------------------------
// Encoding and printing
// ---------------------------------------------------------------------------

/** nullopt for NULL; strings are raw payload bytes. */
std::optional<std::vector<byte>> Encode(const Value &v) {
  return std::visit(
      [](const auto &x) -> std::optional<std::vector<byte>> {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Null>) {
          return std::nullopt;
        } else if constexpr (std::is_same_v<T, bool>) {
          return std::vector<byte>{static_cast<byte>(x ? 1 : 0)};
        } else if constexpr (std::is_same_v<T, std::string>) {
          const auto *p = reinterpret_cast<const byte *>(x.data());
          return std::vector<byte>(p, p + x.size());
        } else {
          const auto *p = reinterpret_cast<const byte *>(&x);
          return std::vector<byte>(p, p + sizeof(T));
        }
      },
      v);
}

Value Decode(DataChunk &chunk, u16 i, type_id type) {
  if (chunk.IsNull(i)) { return Null{}; }
  const byte *p = chunk.GetByIdx(i);
  switch (type) {
  case type_id::BOOLEAN: {
    u8 b;
    std::memcpy(&b, p, 1);
    return b != 0;
  }
  case type_id::INTEGER: {
    int32_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
  }
  case type_id::DOUBLE: {
    double v;
    std::memcpy(&v, p, sizeof(v));
    return v;
  }
  case type_id::VARCHAR: {
    const auto d = VarlenEntry::Cast(p)->Data(chunk.Bytes());
    return std::string(reinterpret_cast<const char *>(d.data()), d.size());
  }
  default: ADD_FAILURE() << "unsupported type in test"; return Null{};
  }
}

std::string Show(const Value &v) {
  std::ostringstream os;
  std::visit(
      [&](const auto &x) {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Null>)
          os << "NULL";
        else if constexpr (std::is_same_v<T, bool>)
          os << (x ? "true" : "false");
        else if constexpr (std::is_same_v<T, std::string>)
          os << '"' << x << '"';
        else
          os << x;
      },
      v);
  return os.str();
}

std::string Show(const std::optional<Row> &r) {
  if (!r) { return "<page not visible>"; }
  std::ostringstream os;
  os << "{";
  for (const auto &[id, v] : *r) { os << " " << id << "=" << Show(v); }
  os << " }";
  return os.str();
}

using ChunkPtr = std::unique_ptr<DataChunk, void (*)(DataChunk *)>;
ChunkPtr Own(DataChunk *c) { return ChunkPtr(c, &DataChunk::Destroy); }

// ---------------------------------------------------------------------------
// Fixture: the layer above Pax, reduced to Init / Write / Read.
// ---------------------------------------------------------------------------

constexpr u16 kAvgString = 24; // payload bytes the catalog expects per string

class PaxTest : public ::testing::Test {
protected:
  alignas(64) byte page_[DB7_PAGE_SIZE];

  void InitPage(const Schema &schema) {
    std::memset(page_, 0xA5, sizeof(page_)); // a recycled frame is never clean
    std::vector<ColumnStatistics> stats;
    for (const Col &c : schema) {
      const u16 avg = c.type == type_id::VARCHAR ? kAvgString : u16(SizeOf(c.type));
      stats.emplace_back(c.type, c.dropped, avg, c.nullable);
    }
    Pax::Init(page_, stats);
  }

  /** Inserts `row` as a writer on `writer`. Missing live columns are NULL. */
  PaxInsertResponse Write(const Schema &writer, const Row &row) {
    std::vector<ChunkColumn> layout;
    std::vector<PaxInsertInfo> infos;
    std::vector<std::optional<std::vector<byte>>> values;
    for (column_t id = 0; id < writer.size(); ++id) {
      const Col &c = writer[id];
      if (c.dropped) { continue; }
      const auto it = row.find(id);
      auto bytes = Encode(it == row.end() ? Value{Null{}} : it->second);
      const u16 slot = u16(SizeOf(c.type));
      const u16 size = c.type == type_id::VARCHAR ? u16(bytes ? bytes->size() : 0) : slot;
      layout.emplace_back(id, slot);
      infos.push_back({size, slot, c.type});
      values.push_back(std::move(bytes));
    }

    const size_t heap = Pax::CalcHeapBytes(infos);
    auto init = Own(DataChunk::BuildDataChunkInitializer(layout));
    auto chunk = Own(DataChunk::BuildDataChunk(init.get(), heap));
    for (u16 i = 0; i < values.size(); ++i) {
      if (!values[i])
        chunk->SetNull(i, true);
      else if (infos[i].type == type_id::VARCHAR)
        chunk->WriteVarlen(i, *values[i]);
      else
        chunk->WriteByIdx(i, *values[i]);
    }

    const std::vector<byte> before(page_, page_ + sizeof(page_));
    const auto resp =
        Pax::Insert(page_, chunk.get(), infos, heap, static_cast<column_t>(writer.size()));
    if (resp != PaxInsertResponse::OK) {
      EXPECT_EQ(std::memcmp(before.data(), page_, sizeof(page_)), 0)
          << "refused insert modified the page";
    }
    return resp;
  }

  /** Reads tuple `idx` as a reader on `reader`; nullopt if the page is not visible. */
  std::optional<Row> Read(const Schema &reader, u16 idx) {
    static byte empty_default{}; // "" must not be passed as nullptr, which means NULL
    std::vector<ChunkColumn> layout;
    std::vector<PaxGetInfo> infos;
    std::vector<std::optional<std::vector<byte>>> defaults;
    defaults.reserve(reader.size());
    for (column_t id = 0; id < reader.size(); ++id) {
      const Col &c = reader[id];
      if (c.dropped) { continue; }
      auto &d = defaults.emplace_back(Encode(c.dflt));
      const bool varlen = c.type == type_id::VARCHAR;
      const u16 slot = u16(SizeOf(c.type));
      byte *ptr = !d ? nullptr : d->empty() ? &empty_default : d->data();
      const u16 size = varlen ? u16(d ? d->size() : 0) : slot;
      layout.emplace_back(id, slot);
      infos.push_back({id, slot, c.type, size, ptr, varlen});
    }

    const auto ncols = static_cast<column_t>(reader.size());
    if (!Pax::IsPageVisible(page_, infos, ncols)) { return std::nullopt; }
    auto init = Own(DataChunk::BuildDataChunkInitializer(layout));
    auto out = Own(Pax::Get(page_, idx, infos, ncols, init.get()));
    Row row;
    for (u16 i = 0; i < infos.size(); ++i) {
      row[infos[i].column_id] = Decode(*out, i, infos[i].type);
    }
    return row;
  }

  /** Writes Nth rows until the page refuses one; returns how many fit. */
  size_t Fill(size_t name_len, PaxInsertResponse &stop) {
    size_t n = 0;
    while ((stop = Write(Base(), Nth(n, name_len))) == PaxInsertResponse::OK) {
      if (++n > DB7_PAGE_SIZE) {
        ADD_FAILURE() << "page never filled up";
        break;
      }
    }
    return n;
  }

  void ExpectNthRows(size_t n, size_t name_len) {
    for (size_t i = 0; i < n; ++i) {
      EXPECT_EQ(Show(Read(Base(), u16(i))), Show(Nth(i, name_len))) << "row " << i;
    }
  }
};

// ---------------------------------------------------------------------------
// Values round-trip
// ---------------------------------------------------------------------------

TEST_F(PaxTest, StringLengthsAroundTheSlotBoundary) {
  constexpr size_t k = VarlenEntry::inline_bytes;
  constexpr size_t slot = VarlenEntry::slot_size;
  const std::vector<size_t> lens = {0, 1, k - 1, k, k + 1, slot, slot + 1, 200};
  InitPage(Base());
  for (size_t i = 0; i < lens.size(); ++i) {
    ASSERT_EQ(Write(Base(), Nth(i, lens[i])), PaxInsertResponse::OK) << "length " << lens[i];
  }
  for (size_t i = 0; i < lens.size(); ++i) {
    EXPECT_EQ(Show(Read(Base(), u16(i))), Show(Nth(i, lens[i]))) << "length " << lens[i];
  }
}

TEST_F(PaxTest, NullBitsAreIndependentPerRowAndColumn) {
  auto row = [](size_t i) {
    Row r = Nth(i, 3);
    if (i % 3 == 0) r[kName] = Null{};
    if (i % 4 == 1) r[kScore] = Null{};
    return r;
  };
  InitPage(Base());
  for (size_t i = 0; i < 20; ++i) { ASSERT_EQ(Write(Base(), row(i)), PaxInsertResponse::OK); }
  for (size_t i = 0; i < 20; ++i) {
    EXPECT_EQ(Show(Read(Base(), u16(i))), Show(row(i))) << "row " << i;
  }
}

// ---------------------------------------------------------------------------
// Filling a page
// ---------------------------------------------------------------------------

TEST_F(PaxTest, ShortStringsFillToCapacity) {
  InitPage(Base());
  PaxInsertResponse stop;
  const size_t n = Fill(5, stop);
  EXPECT_EQ(stop, PaxInsertResponse::NO_SPACE_CAPACITY);
  EXPECT_GT(n, 0u);
  ExpectNthRows(n, 5);
}

TEST_F(PaxTest, LongStringsRunOutOfHeap) {
  InitPage(Base());
  PaxInsertResponse stop;
  const size_t n = Fill(3 * kAvgString, stop); // far above the estimate Init used
  EXPECT_EQ(stop, PaxInsertResponse::NO_SPACE_HEAP);
  EXPECT_GT(n, 0u);
  ExpectNthRows(n, 3 * kAvgString);
}

// ---------------------------------------------------------------------------
// Writer compatibility: a writer must never put a row on a page whose layout
// it doesn't match. On OK, the row must read back with the writer's schema.
// ---------------------------------------------------------------------------

struct WriteCase {
  std::string name;
  Schema page;
  Schema writer;
  Row row;
  PaxInsertResponse expect;
};
void PrintTo(const WriteCase &c, std::ostream *os) { *os << c.name; }

std::vector<WriteCase> WriteCases() {
  using R = PaxInsertResponse;
  const Schema added = Add(Base(), kAddedInt);
  return {
      {"SameSchema", Base(), Base(), BaseRow(), R::OK},
      {"WriterPredatesAddColumn", added, Base(), BaseRow(), R::WRITER_SCHEMA_STALE},
      {"WriterPredatesDropColumn", Drop(Base(), kName), Base(), BaseRow(), R::WRITER_SCHEMA_STALE},
      {"WriterPredatesDropOfAddedColumn", Drop(added, kAdded), added,
       With(BaseRow(), kAdded, int32_t{1}), R::WRITER_SCHEMA_STALE},
      {"PagePredatesAddColumn", Base(), added, With(BaseRow(), kAdded, int32_t{1}),
       R::PAGE_SCHEMA_OLDER},
      {"PagePredatesDropNotNull_Null", NotNull(Base(), kScore), Base(),
       With(BaseRow(), kScore, Null{}), R::PAGE_SCHEMA_OLDER},
      {"PagePredatesDropNotNull_Value", NotNull(Base(), kScore), Base(), BaseRow(), R::OK},
      {"PagePredatesDropColumn", Base(), Drop(Base(), kName), Without(BaseRow(), kName), R::OK},
      {"PagePredatesDropOfAddedColumn", added, Drop(added, kAdded), BaseRow(), R::OK},
  };
}

class PaxWriteTest : public PaxTest, public ::testing::WithParamInterface<WriteCase> {};

TEST_P(PaxWriteTest, Compatibility) {
  const WriteCase &c = GetParam();
  InitPage(c.page);
  EXPECT_EQ(Write(c.writer, c.row), c.expect);
  if (c.expect == PaxInsertResponse::OK) { EXPECT_EQ(Show(Read(c.writer, 0)), Show(c.row)); }
}

INSTANTIATE_TEST_SUITE_P(Pax, PaxWriteTest, ::testing::ValuesIn(WriteCases()),
                         [](const auto &info) { return info.param.name; });

// ---------------------------------------------------------------------------
// Reader compatibility: a reader can read any page whose rows it could see.
// Columns the page predates come back as the reader's default; pages created
// by a schema change the reader doesn't see are reported as not visible.
// ---------------------------------------------------------------------------

struct ReadCase {
  std::string name;
  Schema page; // also the writer's schema
  Row written;
  Schema reader;
  std::optional<Row> expect; // nullopt = page not visible
};
void PrintTo(const ReadCase &c, std::ostream *os) { *os << c.name; }

std::vector<ReadCase> ReadCases() {
  const Row row = BaseRow();
  auto added_with = [](Value dflt, type_id t = type_id::VARCHAR, bool nullable = false) {
    return Add(Base(), Col{t, nullable, false, std::move(dflt)});
  };
  const std::string spill(3 * VarlenEntry::slot_size, 'd');
  return {
      {"SameSchema", Base(), row, Base(), row},
      {"ReaderDroppedColumn", Base(), row, Drop(Base(), kName), Without(row, kName)},
      {"ReaderAddedColumn_IntDefault", Base(), row, Add(Base(), kAddedInt),
       With(row, kAdded, int32_t{42})},
      {"ReaderAddedColumn_NullDefault", Base(), row, added_with(Null{}, type_id::DOUBLE, true),
       With(row, kAdded, Null{})},
      {"ReaderAddedColumn_InlineStringDefault", Base(), row, added_with(std::string("abc")),
       With(row, kAdded, std::string("abc"))},
      {"ReaderAddedColumn_SpillingStringDefault", Base(), row, added_with(spill),
       With(row, kAdded, spill)},
      {"ReaderAddedColumn_EmptyStringDefault", Base(), row, added_with(std::string()),
       With(row, kAdded, std::string())},
      {"ReaderPredatesAddColumn", Add(Base(), kAddedInt), With(row, kAdded, int32_t{1}), Base(),
       std::nullopt},
      {"ReaderPredatesDropColumn", Drop(Base(), kName), Without(row, kName), Base(), std::nullopt},
  };
}

class PaxReadTest : public PaxTest, public ::testing::WithParamInterface<ReadCase> {};

TEST_P(PaxReadTest, Compatibility) {
  const ReadCase &c = GetParam();
  InitPage(c.page);
  ASSERT_EQ(Write(c.page, c.written), PaxInsertResponse::OK);
  EXPECT_EQ(Show(Read(c.reader, 0)), Show(c.expect));
}

INSTANTIATE_TEST_SUITE_P(Pax, PaxReadTest, ::testing::ValuesIn(ReadCases()),
                         [](const auto &info) { return info.param.name; });

} // namespace
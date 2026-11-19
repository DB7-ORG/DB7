#pragma once

#include "access/data_table.hpp"
#include "execution/sources/source_type.hpp"
#include "execution/sources/table_local_state.hpp"

namespace db7::execution {

using SourceResult = PollResult;

class BaseSource {
protected:
  SourceType type_;

public:
  BaseSource(SourceType type) : type_(type) {}

  virtual std::unique_ptr<SourceLocalState> InitLocal() = 0;
  virtual SourceResult Next(SourceLocalState &l, void *out) = 0;

  virtual ~BaseSource() = default;

  template <class T>
  T &Cast() {
    DB7_ASSERT(type_ == T::kType, "Invalid source cast");
    return static_cast<T &>(*this);
  }
  template <class T>
  const T &Cast() const {
    DB7_ASSERT(type_ == T::kType, "Invalid source cast");
    return static_cast<const T &>(*this);
  }
};

class TableSource : public BaseSource {
public:
  static constexpr SourceType kType = SourceType::TABLE;

private:
  access::DataTable &table_;
  std::unique_ptr<storage::ParallelScanShared> shared_;

public:
  TableSource(transaction::TransactionContext &context, access::DataTable &t,
              const std::vector<storage::StorageColumnDesc> &columns)
      : BaseSource(TABLE), table_(t), shared_(table_.InitParallelScan(context, columns)) {}
  ~TableSource() override = default;

  std::unique_ptr<SourceLocalState> InitLocal() override {
    auto l = std::make_unique<TableLocalState>();
    l->scan = table_.InitLocalScan(*shared_);
    return l;
  }

  SourceResult Next(SourceLocalState &l, void *out) override {
    auto &state = l.Cast<TableLocalState>();
    return table_.Scan(*state.scan.get());
  }
};
} // namespace db7::execution
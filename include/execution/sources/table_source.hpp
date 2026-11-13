#pragma once

#include "access/data_table.hpp"
#include "execution/sources/table_local_state.hpp"

namespace db7::execution {
enum SourceType { TABLE };
enum class SourceResult : u8 { HaveMore, Finished, Blocked };

class BaseSource {
protected:
  SourceType type_;
  access::DataTable &table_;

public:
  BaseSource(access::DataTable &t) : table_(t) {}

  virtual std::unique_ptr<SourceLocalState> InitLocal() = 0;
  virtual SourceResult Next(SourceLocalState &l, void *out) = 0;

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
  std::unique_ptr<storage::ParallelScanShared> shared_;

public:
  TableSource(access::DataTable &t) : BaseSource(t) { shared_ = table_.InitParallelScan(); }

  std::unique_ptr<SourceLocalState> InitLocal() override {
    auto l = std::make_unique<TableLocalState>();
    l->scan = table_.InitLocalScan(*shared_);
    return l;
  }

  SourceResult Next(TableLocalState *local_, void *out) {
    storage::PinnedPage page = local_->scan->Stream().Next();
    // TODO transform it to correct representation
  }
};
} // namespace db7::execution
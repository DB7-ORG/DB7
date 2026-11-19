#pragma once

#include "execution/sources/source_type.hpp"
#include "storage/models/scan_local.hpp"

namespace db7::execution {
struct SourceLocalState {
protected:
  SourceType type_;

public:
  explicit SourceLocalState(SourceType type) : type_(type) {}
  virtual ~SourceLocalState() = default;

  SourceType GetType() const { return type_; }

  template <class T>
  T &Cast() {
    DB7_ASSERT(type_ == T::kType, "Invalid source local state cast");
    return static_cast<T &>(*this);
  }
  template <class T>
  const T &Cast() const {
    DB7_ASSERT(type_ == T::kType, "Invalid source local state cast");
    return static_cast<const T &>(*this);
  }
};

struct TableLocalState final : SourceLocalState {
  static constexpr SourceType kType = SourceType::TABLE;

  TableLocalState() : SourceLocalState(kType) {}

  std::unique_ptr<storage::ScanLocal> scan;
};

} // namespace db7::execution
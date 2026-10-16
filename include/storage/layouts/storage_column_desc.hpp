#pragma once

#include "common.hpp"
#include "shared/types/type_defs.hpp"

#include <span>

namespace db7::storage {
class DefaultValue {
private:
  std::span<const byte> value_;

public:
  DefaultValue(std::span<const byte> value) : value_(value) {}
  DefaultValue() = default;

  const byte *data() const { return value_.data(); }
  size_t size() const { return value_.size(); }
  bool empty() const { return value_.empty(); }

  auto begin() const { return value_.begin(); }
  auto end() const { return value_.end(); }

  std::span<const byte> Span() const { return value_; }
};

struct StorageColumnDesc {
  column_t column_id;
  u16 slot_size;
  type_id type;
  bool isVarlen;
  // default value for column, used for insert
  DefaultValue default_val;
  bool isDropped;
  bool isNullable;

  StorageColumnDesc(column_t column_id, type_id type, DefaultValue def = {}, bool dropped = false,
                    bool isNullable = false)
      : column_id(column_id), slot_size(SizeOf(type)), type(type), isVarlen(IsVarlen(type)),
        default_val(def), isDropped(dropped), isNullable(isNullable) {}
};
} // namespace db7::storage
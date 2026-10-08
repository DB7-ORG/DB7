#pragma once

#include "access/index/index.hpp"
#include "catalog/catalog_common.hpp"

#include <memory>

namespace db7::access {

class AddTableIndexInfo {
public:
  std::unique_ptr<Index> index_;
  catalog::IndexConstraintType type_;

  AddTableIndexInfo(std::unique_ptr<Index> index, catalog::IndexConstraintType type)
      : index_(std::move(index)), type_(type) {}
};
} // namespace db7::access
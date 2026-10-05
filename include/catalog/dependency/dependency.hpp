#pragma once

#include "catalog/catalog_common.hpp"
#include "shared/identifier.hpp"

namespace db7::catalog {
class CatalogEntryInfo {
public:
  CatalogType type;
  Identifier schema;
  Identifier name;

public:
  bool operator==(const CatalogEntryInfo &other) const {
    if (other.type != type) { return false; }
    if (other.schema != schema) { return false; }
    if (other.name != name) { return false; }
    return true;
  }
};

struct DependencyFlags {
private:
  uint8_t value;

public:
  DependencyFlags() : value(0) {}
  DependencyFlags(const DependencyFlags &other) : value(other.value) {}
  virtual ~DependencyFlags() = default;
  DependencyFlags &operator=(const DependencyFlags &other) {
    value = other.value;
    return *this;
  }
  bool operator==(const DependencyFlags &other) const { return other.value == value; }
  bool operator!=(const DependencyFlags &other) const { return !(*this == other); }

public:
  virtual std::string ToString() const = 0;

protected:
  template <uint8_t BIT>
  bool IsSet() const {
    static const uint8_t FLAG = (1 << BIT);
    return (value & FLAG) == FLAG;
  }
  template <uint8_t BIT>
  void Set() {
    static const uint8_t FLAG = (1 << BIT);
    value |= FLAG;
  }
  void Merge(uint8_t other) { value |= other; }
  uint8_t Value() { return value; }
};

struct DependencySubjectFlags : public DependencyFlags {
private:
  /**
   * Flag showcasing ownership. Good example is the sequence that is owned by the table
   * but is still positioned in subjects.
   */
  static constexpr uint8_t OWNERSHIP = 0;

public:
  DependencySubjectFlags &Apply(DependencySubjectFlags other) {
    Merge(other.Value());
    return *this;
  }

public:
  bool IsOwnership() const { return IsSet<OWNERSHIP>(); }

public:
  DependencySubjectFlags &SetOwnership() {
    Set<OWNERSHIP>();
    return *this;
  }

public:
  std::string ToString() const override {
    std::string result;
    if (IsOwnership()) { result += "OWNS"; }
    return result;
  }
};

struct DependencyDependentFlags : public DependencyFlags {
private:
  static constexpr uint8_t BLOCKING = 0;
  static constexpr uint8_t OWNED_BY = 1;

public:
  DependencyDependentFlags &Apply(DependencyDependentFlags other) {
    Merge(other.Value());
    return *this;
  }

public:
  bool IsBlocking() const { return IsSet<BLOCKING>(); }
  bool IsOwnedBy() const { return IsSet<OWNED_BY>(); }

public:
  DependencyDependentFlags &SetBlocking() {
    Set<BLOCKING>();
    return *this;
  }
  DependencyDependentFlags &SetOwnedBy() {
    Set<OWNED_BY>();
    return *this;
  }

public:
  std::string ToString() const override {
    std::string result;
    if (IsBlocking()) {
      result += "REGULAR";
    } else {
      result += "AUTOMATIC";
    }
    result += " | ";
    if (IsOwnedBy()) { result += "OWNED BY"; }
    return result;
  }
};

} // namespace db7::catalog
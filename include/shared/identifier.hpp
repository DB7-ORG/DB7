#pragma once

#include "catalog/catalog_common.hpp"

#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace db7 {

//! An Identifier represents a SQL identifier (e.g. a column name, table name, alias, ...).
//! Unlike a regular std::string, identifiers compare case-insensitively (using
//! std::stringUtil::CIEquals). Internally the identifier is stored as-is (preserving the original
//! casing), but all comparisons, hashing and ordering are case-insensitive.
class Identifier {

private:
  std::string value;

public:
  Identifier() = default;
  //! Construction from a std::string literal is implicit: literals in the source are identifiers by
  //! intent.
  Identifier(const char *str)
      : value(str) { // NOLINT: implicit conversion from literals is intentional
  }
  //! Construction from a runtime std::string is explicit: an Identifier carries case-insensitive
  //! semantics, so promoting a runtime std::string must be a deliberate choice at the call site.
  explicit Identifier(const std::string &str) : value(str) {}
  explicit Identifier(std::string &&str) : value(std::move(str)) {}

  //! Named constructors for well-known identifiers
  static Identifier DefaultSchema() { return Identifier(DEFAULT_SCHEMA); }
  static Identifier InvalidSchema() { return Identifier(INVALID_SCHEMA); }
  static Identifier InvalidCatalog() { return Identifier(INVALID_CATALOG); }
  static Identifier SystemCatalog() { return Identifier(SYSTEM_CATALOG); }
  static Identifier TempCatalog() { return Identifier(TEMP_CATALOG); }

  //! Conversion back to a std::string is explicit: it discards the case-insensitive semantics, so
  //! callers must opt in (use GetIdentifierName() for the raw value). Keeping this explicit is what
  //! makes the Identifier type safe.
  explicit operator const std::string &() const { return value; }

  //! The raw underlying std::string (preserving original casing)
  const std::string &GetIdentifierName() const { return value; }

  bool empty() const { // NOLINT: match std::std::string interface
    return value.empty();
  }
  void clear() { // NOLINT: match std::std::string interface
    value.clear();
  }
  idx_t size() const { // NOLINT: match std::std::string interface
    return value.size();
  }
  const char *c_str() const { // NOLINT: match std::std::string interface
    return value.c_str();
  }

  //! Case-insensitive hash of the identifier
  hash_t Hash() const;
};

//! Equality (case-insensitive)
bool operator==(const Identifier &a, const Identifier &b);
bool operator==(const Identifier &a, const std::string &b);
bool operator==(const std::string &a, const Identifier &b);
bool operator==(const Identifier &a, const char *b);
bool operator==(const char *a, const Identifier &b);

inline bool operator!=(const Identifier &a, const Identifier &b) { return !(a == b); }
inline bool operator!=(const Identifier &a, const std::string &b) { return !(a == b); }
inline bool operator!=(const std::string &a, const Identifier &b) { return !(a == b); }
inline bool operator!=(const Identifier &a, const char *b) { return !(a == b); }
inline bool operator!=(const char *a, const Identifier &b) { return !(a == b); }

//! Ordering (case-insensitive)
bool operator<(const Identifier &a, const Identifier &b);

//! Streaming an identifier writes its raw name (without quotes) - this mirrors writing the
//! underlying std::string
std::ostream &operator<<(std::ostream &os, const Identifier &id);

//! std::string concatenation (std::operator+ is a template and cannot use the implicit conversion,
//! so we provide our own)
inline std::string operator+(const Identifier &a, const std::string &b) {
  return a.GetIdentifierName() + b;
}
inline std::string operator+(const std::string &a, const Identifier &b) {
  return a + b.GetIdentifierName();
}
inline std::string operator+(const Identifier &a, const char *b) {
  return a.GetIdentifierName() + b;
}
inline std::string operator+(const char *a, const Identifier &b) {
  return a + b.GetIdentifierName();
}
inline std::string operator+(const Identifier &a, const Identifier &b) {
  return a.GetIdentifierName() + b.GetIdentifierName();
}
inline std::string operator+(const Identifier &a, char b) { return a.GetIdentifierName() + b; }
inline std::string operator+(char a, const Identifier &b) { return a + b.GetIdentifierName(); }

//! Appending an identifier to a std::string appends the raw name
inline std::string &operator+=(std::string &a, const Identifier &b) {
  a += b.GetIdentifierName();
  return a;
}

struct IdentifierHashFunction {
  uint64_t operator()(const Identifier &id) const { return id.Hash(); }
};

struct IdentifierEquality {
  bool operator()(const Identifier &a, const Identifier &b) const { return a == b; }
};

struct IdentifierCompare {
  bool operator()(const Identifier &a, const Identifier &b) const { return a < b; }
};

template <typename T>
using identifier_map_t =
    std::unordered_map<Identifier, T, IdentifierHashFunction, IdentifierEquality>;

using identifier_set_t = std::unordered_set<Identifier, IdentifierHashFunction, IdentifierEquality>;

template <typename T>
using identifier_tree_t = std::map<Identifier, T, IdentifierCompare>;

//! Helper to convert a vector of identifiers to a vector of (raw) std::strings (for interop with
//! std::string-based APIs)
inline std::vector<std::string> IdentifiersTostrings(const std::vector<Identifier> &identifiers) {
  std::vector<std::string> result;
  result.reserve(identifiers.size());
  for (auto &identifier : identifiers) { result.push_back(identifier.GetIdentifierName()); }
  return result;
}

//! Helper to convert a vector of (raw) std::strings to a vector of identifiers (to be removed at
//! the end of the rework)
inline std::vector<Identifier> stringsToIdentifiers(const std::vector<std::string> &strings) {
  std::vector<Identifier> result;
  result.reserve(strings.size());
  for (auto &str : strings) { result.emplace_back(str); }
  return result;
}

//! Identifier-aware overloads of the invalid-catalog/schema checks. These live here (rather than
//! next to the std::string versions in constants.hpp) because constants.hpp is a dependency of this
//! header.
inline bool IsInvalidCatalog(const Identifier &catalog) { return catalog.empty(); }
inline bool IsInvalidSchema(const Identifier &schema) { return schema.empty(); }

} // namespace db7

namespace std {
template <>
struct hash<db7::Identifier> {
  size_t operator()(const db7::Identifier &id) const { return id.Hash(); }
};
} // namespace std

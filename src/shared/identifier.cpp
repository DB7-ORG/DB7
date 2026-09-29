#include "shared/identifier.hpp"
#include "common.hpp"
#include "shared/string_util.hpp"

#include <ostream>

namespace db7 {

hash_t Identifier::Hash() const { return StringUtil::CIHash(value); }

bool operator==(const Identifier &a, const Identifier &b) {
  return StringUtil::CIEquals(a.GetIdentifierName(), b.GetIdentifierName());
}
bool operator==(const Identifier &a, const std::string &b) {
  return StringUtil::CIEquals(a.GetIdentifierName(), b);
}
bool operator==(const std::string &a, const Identifier &b) {
  return StringUtil::CIEquals(a, b.GetIdentifierName());
}
bool operator==(const Identifier &a, const char *b) {
  return StringUtil::CIEquals(a.GetIdentifierName(), std::string(b));
}
bool operator==(const char *a, const Identifier &b) {
  return StringUtil::CIEquals(std::string(a), b.GetIdentifierName());
}

bool operator<(const Identifier &a, const Identifier &b) {
  return StringUtil::CILessThan(a.GetIdentifierName(), b.GetIdentifierName());
}

std::ostream &operator<<(std::ostream &os, const Identifier &id) {
  return os << id.GetIdentifierName();
}

} // namespace db7

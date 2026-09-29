#pragma once

#include "common.hpp"

#include <string>

namespace db7 {
class StringUtil {
private:
  static const u8 ASCII_TO_LOWER_MAP[];
  static const u8 ASCII_TO_UPPER_MAP[];

public:
  //! Case insensitive hash
  static u64 CIHash(const std::string &str);
  static u64 CIHash(const char *str, idx_t size);
  //! Case insensitive equals
  static bool CIEquals(const std::string &l1, const std::string &l2);
  //! Case insensitive equals (null-terminated strings)
  static bool CIEquals(const char *l1, idx_t l1_size, const char *l2, idx_t l2_size);
  //! Case insensitive compare
  static bool CILessThan(const std::string &l1, const std::string &l2);

  static bool CIStartsWith(const std::string &str, const std::string &prefix);
};
} // namespace db7
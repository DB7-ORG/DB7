#include "shared/string_util.hpp"

namespace db7 {

static char CharacterToLower(char c) {
  if (c >= 'A' && c <= 'Z') { return static_cast<char>(c + ('a' - 'A')); }
  return c;
}

uint64_t StringUtil::CIHash(const std::string &str) {
  return StringUtil::CIHash(str.c_str(), str.size());
}

uint64_t StringUtil::CIHash(const char *str, idx_t size) {
  uint32_t hash = 0;
  for (idx_t i = 0; i < size; i++) {
    hash += static_cast<uint32_t>(CharacterToLower(static_cast<char>(str[i])));
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

bool StringUtil::CIEquals(const char *l1, idx_t l1_size, const char *l2, idx_t l2_size) {
  if (l1_size != l2_size) { return false; }
  const auto charmap = ASCII_TO_LOWER_MAP;
  for (idx_t c = 0; c < l1_size; c++) {
    if (charmap[(uint8_t)l1[c]] != charmap[(uint8_t)l2[c]]) { return false; }
  }
  return true;
}

bool StringUtil::CIEquals(const std::string &l1, const std::string &l2) {
  return CIEquals(l1.c_str(), l1.size(), l2.c_str(), l2.size());
}

bool StringUtil::CIStartsWith(const std::string &str, const std::string &prefix) {
  if (prefix.size() > str.size()) { return false; }
  return CIEquals(str.c_str(), prefix.size(), prefix.c_str(), prefix.size());
}

bool StringUtil::CILessThan(const std::string &s1, const std::string &s2) {
  const auto charmap = ASCII_TO_UPPER_MAP;

  unsigned char u1{}, u2{};

  idx_t length = std::min<idx_t>(s1.length(), s2.length());
  length += s1.length() != s2.length();
  for (idx_t i = 0; i < length; i++) {
    u1 = (unsigned char)s1[i];
    u2 = (unsigned char)s2[i];
    if (charmap[u1] != charmap[u2]) { break; }
  }
  return (charmap[u1] - charmap[u2]) < 0;
}

const uint8_t StringUtil::ASCII_TO_UPPER_MAP[] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,
    19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,
    38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
    57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,
    76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,
    95,  96,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,
    82,  83,  84,  85,  86,  87,  88,  89,  90,  123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170,
    171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
    190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208,
    209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227,
    228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
    247, 248, 249, 250, 251, 252, 253, 254, 255};

const uint8_t StringUtil::ASCII_TO_LOWER_MAP[] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,
    19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,
    38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,
    57,  58,  59,  60,  61,  62,  63,  64,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107,
    108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91,  92,  93,  94,
    95,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170,
    171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
    190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208,
    209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227,
    228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
    247, 248, 249, 250, 251, 252, 253, 254, 255};

} // namespace db7
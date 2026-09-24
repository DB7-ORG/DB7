#include "parser/expressions/value_util.hpp"
#include "parser/expressions/value.hpp"

namespace db7::parser {

std::pair<StringVal, std::unique_ptr<byte[]>>
ValueUtil::CreateStringVal(const shared::ManagedPointer<const char> string, const uint32_t length) {
  return {StringVal(string.Get(), length), nullptr};
}

std::pair<StringVal, std::unique_ptr<byte[]>>
ValueUtil::CreateStringVal(const std::string &string) {
  return CreateStringVal(shared::ManagedPointer(string.data()), string.length());
}

std::pair<StringVal, std::unique_ptr<byte[]>>
ValueUtil::CreateStringVal(const std::string_view string) {
  return CreateStringVal(shared::ManagedPointer(string.data()), string.length());
}

std::pair<StringVal, std::unique_ptr<byte[]>> ValueUtil::CreateStringVal(const StringVal string) {
  return CreateStringVal(shared::ManagedPointer(string.GetContent()), string.GetLength());
}

} // namespace db7::parser

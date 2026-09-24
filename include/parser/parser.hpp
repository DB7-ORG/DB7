#pragma once

#include <fmt/core.h>
#include <pg_query.h>

namespace db7::parser {
class Parser {
public:
  void Parse(const char *query) {
    PgQueryParseResult result = pg_query_parse(query);

    if (result.error) {
      fmt::print("parse error: {} at {}\n", result.error->message,
                 result.error->cursorpos);
    } else {
      fmt::print("{}\n", result.parse_tree); // JSON parse tree
    }

    pg_query_free_parse_result(result);
  }
};
} // namespace db7::parser
#pragma once

#include <memory>

#include "common.hpp"
#include "parse_result.hpp"

namespace noisepage::parser
{

    /**
     * PostgresParser obtains and transforms the Postgres parse tree into our Terrier parse tree.
     * In the future, we definitely want to replace this with our own parser.
     */
    class PostgresParser
    {
        /*
         * To modify this file, examine:
         *    List and ListCell in pg_list.h,
         *    Postgres types in nodes.h.
         *
         * To add new Statement support, find the parsenode in:
         *    third_party/libpg_query/src/postgres/include/nodes/parsenodes.h,
         *    third_party/libpg_query/src/postgres/include/nodes/primnodes.h,
         * then copy to src/include/parser/parsenodes.h and add the corresponding helper function.
         */

    public:
        PostgresParser() = delete;

        /**
         * Builds the parse tree for the given query string.
         * @param query_string query string to be parsed
         * @return unique pointer to parse tree
         */
        static std::unique_ptr<parser::ParseResult> BuildParseTree(const std::string &query_string);
    };
}
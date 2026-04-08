#include "parser.hpp"

u32 Parse()
{

    PgQueryParseResult result;

    result = pg_query_parse("SELECT * FROM customers");

    printf("%s\n", result.parse_tree);

    pg_query_free_parse_result(result);

    return 0;
}
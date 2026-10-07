#include "catalog/objects/create_index_info.hpp"

namespace db7::catalog {
CreateIndexInfo::CreateIndexInfo()
    : CreateInfo(CatalogType::INDEX_ENTRY, Identifier::InvalidSchema()) {}
} // namespace db7::catalog
#include "access/table.hpp"

#include "storage/fsm/fsm.hpp"
#include "shared/align_util.hpp"

namespace db7::access
{
    /**
     * Inserts multiple rows into a table.
     * Note: this is still a column store but in code its much easier to work in row format for catalog.
     */
    void Table::Insert(const ProjectedRows &rows)
    {
        u32 page_id = storage::FreeSpaceManager::Get(rows.total_size); // TODO table oid
        storage::Page *insert_page = buffer_->Pin(page_id);

        const auto &map = schema_.GetOffsetMap();
        auto curr = rows.data;

        for (const auto &column : schema_.GetColumns())
        {
            catalog::col_oid_t oid = column.GetOid();
            u32 offset = map.at(oid);

            u32 type_size = column.GetTypeSize();
            u32 size = rows.row_count * type_size;
            curr = shared::AlignUp(curr, type_size);
            insert_page->WriteOffset(offset, curr, size);
            curr += size;
        }

        buffer_->Unpin(insert_page, true);
    }
}
#pragma once

#include "storage/storage_common.hpp"
#include "storage/disk_manager/fd_cache.hpp"

#include <memory>

/**
 * this is a hack
 */
#define MAX_PATH_LEN 256u
#define MAX_PATH_LEN_2 128u

namespace db7::storage
{
    class DiskManager
    {
    private:
        char base_dir_[MAX_PATH_LEN_2]; /* directory that holds table files */
        std::unique_ptr<FdCache> cache_;

        void BuildPath(table_id tid, char *buf, u32 len);
        void LoadExistingTables();

    public:
        DiskManager(const char *base_dir);
        ~DiskManager();

        bool CreateTable(table_id tbl_id);
        bool DropTable(table_id tbl_id);
        bool ExistsTable(table_id tbl_id);
        bool ReadPage(table_id tbl_id, page_id pid, void *dest);
        bool WritePage(table_id tbl_id, page_id pid, const void *src);
        bool TruncateFile(table_id tbl_id, u32 pages_num);
        u32 PageCount(table_id tbl_id);
    };
}
#include "storage/disk_manager/disk_manager.hpp"
#include "shared/macro_helper.hpp"

#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#define DIRECT_ALIGN 4096
#define INIT_FREE_PAGES 3

namespace db7::storage
{
    void DiskManager::BuildPath(table_id tid, char *buf, u32 len)
    {
        int n = snprintf(buf, len, "%s/table_%u.db", base_dir_, (unsigned)tid);
        (void)n;
        DB7_ASSERT(n > 0 && (u32)n < len, "Path buffer too small");
    }

    void DiskManager::LoadExistingTables()
    {
        DIR *dir = opendir(base_dir_);
        if (!dir)
            return;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type != DT_REG)
                continue;

            table_id tbl_id;
            if (sscanf(entry->d_name, "table_%u", &tbl_id) != 1)
                continue;

            char path[MAX_PATH_LEN];
            BuildPath(tbl_id, path, sizeof(path));

            int fd = open(path, O_RDWR | O_DIRECT);
            if (fd < 0)
                continue;

            struct stat st;
            fstat(fd, &st);

            FdCacheEntry hdr;
            hdr.fd = fd;
            hdr.page_count = st.st_size / DB7_PAGE_SIZE;

            cache_->Set(tbl_id, hdr);
        }

        closedir(dir);
    }

    DiskManager::DiskManager(const char *base_dir)
        : cache_(std::make_unique<FdCache>())
    {
        strncpy(base_dir_, base_dir, MAX_PATH_LEN_2 - 1);

        for (u32 i = 0; i < MAX_OPEN_FILES; i++)
        {
            cache_->Invalidate(i);
        }

        mkdir(base_dir_, 0755);

        LoadExistingTables();
    }

    DiskManager::~DiskManager()
    {
        for (u32 i = 0; i < MAX_OPEN_FILES; i++)
        {
            int fd = cache_->Get(i).fd;
            if (fd >= 0)
            {
                close(fd);
            }
        }
    }

    bool DiskManager::CreateTable(table_id tbl_id)
    {
        if (cache_->Get(tbl_id).fd != -1)
        {
            DB7_ASSERT(false, "File already exists");
            return false;
        }

        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));

        int fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_DIRECT, 0644);
        if (fd < 0)
        {
            DB7_ASSERT(false, "File not found");
            return false;
        }

        FdCacheEntry entry(fd, INIT_FREE_PAGES);
        cache_->Set(tbl_id, entry);

        TruncateFile(tbl_id, INIT_FREE_PAGES);

        return true;
    }

    bool DiskManager::DropTable(table_id tbl_id)
    {
        auto hdr = cache_->Invalidate(tbl_id);
        DB7_ASSERT(hdr.fd >= 0, "Invalid file");

        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));

        close(hdr.fd);
        unlink(path);

        return true;
    }

    bool DiskManager::ExistsTable(table_id tbl_id)
    {
        char path[MAX_PATH_LEN];
        BuildPath(tbl_id, path, sizeof(path));
        return access(path, F_OK) == 0;
    }

    bool DiskManager::ReadPage(table_id tbl_id, page_id pid, void *dest)
    {
        DB7_ASSERT(((uintptr_t)dest & (DIRECT_ALIGN - 1)) == 0, "Dest not aligned to 4096");

        FdCacheEntry hdr = cache_->Get(tbl_id);
        DB7_ASSERT(hdr.fd >= 0, "File not found");
        DB7_ASSERT(pid < hdr.page_count, "File outside of bounds");

        off_t offset = (off_t)pid * DB7_PAGE_SIZE;
        ssize_t n = pread(hdr.fd, dest, DB7_PAGE_SIZE, offset);
        (void)n;
        DB7_ASSERT(n == DB7_PAGE_SIZE, "Short read");

        return true;
    }

    bool DiskManager::WritePage(table_id tbl_id, page_id pid, const void *src)
    {
        DB7_ASSERT(((uintptr_t)src & (DIRECT_ALIGN - 1)) == 0, "Dest not aligned to 4096");

        int fd = cache_->Get(tbl_id).fd;
        DB7_ASSERT(fd >= 0, "File not found");

        off_t offset = (off_t)pid * DB7_PAGE_SIZE;
        ssize_t n = pwrite(fd, src, DB7_PAGE_SIZE, offset);
        (void)n;
        DB7_ASSERT(n == DB7_PAGE_SIZE, "Short read");

        return true;
    }

    bool DiskManager::TruncateFile(table_id tbl_id, u32 pages_num)
    {
        FdCacheEntry hdr = cache_->Get(tbl_id);
        DB7_ASSERT(hdr.fd >= 0, "File not found");

        int n = ftruncate(hdr.fd, (pages_num)*DB7_PAGE_SIZE);
        (void)n;
        DB7_ASSERT(n == 0, "Truncate failed");
        hdr.page_count = pages_num;

        cache_->Set(tbl_id, hdr);

        return true;
    }

    u32 DiskManager::PageCount(table_id tbl_id)
    {
        FdCacheEntry hdr = cache_->Get(tbl_id);
        return hdr.page_count;
    }
}
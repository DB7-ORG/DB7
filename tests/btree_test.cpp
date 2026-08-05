#include <gtest/gtest.h>

#include "access/index/btree.hpp"
#include "storage/buffer_pool/buffer_pool.hpp"

#include <cstring>
#include <memory>

using namespace db7;

namespace
{

    /* Builds the same [normalized][original] key shape as prep_keys. */
    access::Key MakeKey(u64 i)
    {
        byte *buf = new byte[sizeof(u64) * 16];
        u32 len = access::KeyNormEncoder::Encode(buf, i, false, false, false);
        std::memcpy(buf + len, &i, sizeof(u64));
        return {u16(len + sizeof(u64)), u16(len), buf};
    }

    class BTreeTest : public ::testing::Test
    {
    protected:
        /* Each test gets its own table id so files never collide. */
        static inline u32 next_tbl_ = 200;

        std::unique_ptr<storage::DiskManagerAsync> dm_;
        std::unique_ptr<storage::DiskScheduler> sched_;
        std::unique_ptr<storage::PageVersionManager> vm_;
        std::unique_ptr<storage::BufferPool> bp_;
        std::unique_ptr<access::BTreeIndex<u64>> tree_;

        void SetUp() override
        {
            dm_ = std::make_unique<storage::DiskManagerAsync>(".data_test");
            sched_ = std::make_unique<storage::DiskScheduler>(dm_.get());
            sched_->Start();
            vm_ = std::make_unique<storage::PageVersionManager>();
            bp_ = std::make_unique<storage::BufferPool>(sched_.get(), vm_.get());
            tree_ = std::make_unique<access::BTreeIndex<u64>>(bp_.get(), dm_.get(), next_tbl_++);
            // TODO clean  ".data_test" directory
        }
    };

    TEST_F(BTreeTest, InsertThenGetSingleKey)
    {
        access::Key k = MakeKey(42);

        ASSERT_TRUE(tree_->Insert(k, 1000).success);

        access::VectorValues<u64> res;
        ASSERT_TRUE(tree_->Get(k, res).success);

        ASSERT_EQ(res.vec.size(), 1u);
        EXPECT_EQ(res.vec[0], 1000u);
    }

} // namespace
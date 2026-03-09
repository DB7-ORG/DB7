#include <gtest/gtest.h>
#include "slab_arena.hpp"

TEST(SlabArena, AllocMultipleReturnsAligned)
{
    SlabArena arena(1024);
    u8 *a = arena.Alloc<u8>(1);
    u64 *b = arena.Alloc<u64>(1);
    u16 *c = arena.Alloc<u16>(1);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % alignof(u8), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(b) % alignof(u64), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(c) % alignof(u16), 0);
}

TEST(SlabArena, AllocPointersDoNotOverlap)
{
    SlabArena arena(1024);
    u32 *a = arena.Alloc<u32>(4);
    u32 *b = arena.Alloc<u32>(4);
    // b must start at or after end of a
    EXPECT_GE(reinterpret_cast<uintptr_t>(b),
              reinterpret_cast<uintptr_t>(a) + sizeof(u32) * 4);
}

TEST(SlabArena, AllocWriteAndRead)
{
    SlabArena arena(1024);
    u32 *p = arena.Alloc<u32>(4);
    for (u32 i = 0; i < 4; i++)
        p[i] = i * 10;
    for (u32 i = 0; i < 4; i++)
        EXPECT_EQ(p[i], i * 10);
}

TEST(SlabArena, AllocSpillsIntoNewBlock)
{
    // Arena fits exactly 4 bytes, force a second block
    SlabArena arena(4);
    u8 *a = arena.Alloc<u8>(4);
    u8 *b = arena.Alloc<u8>(4); // must spill
    ASSERT_NE(b, nullptr);
    // pointers should not overlap
    EXPECT_TRUE(b >= a + 4 || b + 4 <= a);
}

TEST(SlabArena, OversizedSingleAlloc)
{
    // Single alloc larger than initial block size
    SlabArena arena(8);
    u8 *p = arena.Alloc<u8>(256);
    ASSERT_NE(p, nullptr);
    // should be writable
    memset(p, 0xAB, 256);
    EXPECT_EQ(p[255], 0xAB);
}

// ── Reset ────────────────────────────────────────────────────────────────────

TEST(SlabArena, ResetAllowsReuse)
{
    SlabArena arena(1024);
    u32 *a = arena.Alloc<u32>(8);
    (void)a;
    arena.Reset();
    u32 *b = arena.Alloc<u32>(8);
    ASSERT_NE(b, nullptr);
    ASSERT_EQ(a, b);
    // after reset, pointer should be back at the start of the first block
    b[0] = 42;
    EXPECT_EQ(b[0], 42);
}

TEST(SlabArena, ResetFreesExtraBlocks)
{
    SlabArena arena(16);
    // Force several extra blocks
    u8 *old = arena.Alloc<u8>(7);
    for (u32 i = 0; i < 10; i++)
        arena.Alloc<u8>(7);
    arena.Reset();
    // After reset only the base block remains; alloc should still work
    u8 *p = arena.Alloc<u8>(16);
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(p, old);
}

// ── New (placement) ──────────────────────────────────────────────────────────

struct Point
{
    u32 x, y;
    Point(u32 x, u32 y) : x(x), y(y) {}
};

TEST(SlabArena, NewConstructsObject)
{
    SlabArena arena(1024);
    Point *p = arena.New<Point>(3u, 7u);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->x, 3u);
    EXPECT_EQ(p->y, 7u);
}

TEST(SlabArena, NewReturnsAligned)
{
    SlabArena arena(1024);
    Point *p = arena.New<Point>(1u, 2u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(Point), 0);
}

TEST(SlabArena, NewMultipleObjectsDoNotOverlap)
{
    SlabArena arena(1024);
    Point *a = arena.New<Point>(1u, 2u);
    Point *b = arena.New<Point>(3u, 4u);
    EXPECT_NE(a, b);
    EXPECT_EQ(a->x, 1u); // a not clobbered by b
    EXPECT_EQ(b->x, 3u);
}

// ── Mixed type stress ─────────────────────────────────────────────────────────

TEST(SlabArena, MixedTypeAllocationsAllAligned)
{
    SlabArena arena(4096);
    u8 *a = arena.Alloc<u8>(1);
    u64 *b = arena.Alloc<u64>(1);
    u8 *c = arena.Alloc<u8>(1);
    u32 *d = arena.Alloc<u32>(1);
    u64 *e = arena.Alloc<u64>(1);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % alignof(u8), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(b) % alignof(u64), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(c) % alignof(u8), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(d) % alignof(u32), 0);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(e) % alignof(u64), 0);
}
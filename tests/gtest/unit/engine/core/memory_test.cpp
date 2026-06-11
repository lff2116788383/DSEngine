/**
 * @file memory_test.cpp
 * @brief 内存子系统阶段1单测：MemoryTag 注册、SystemAllocator 块头/对齐/统计、Memory 门面与 New/Delete。
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "engine/core/memory/allocator.h"
#include "engine/core/memory/system_allocator.h"
#include "engine/core/memory/memory.h"

using namespace dse::core;

// ============================================================
// MemoryTag 注册表
// ============================================================

TEST(MemoryTagTest, BuiltinNamesResolve) {
    EXPECT_STREQ(MemoryTagName(MemoryTag::Default), "Default");
    EXPECT_STREQ(MemoryTagName(MemoryTag::Render), "Render");
    EXPECT_STREQ(MemoryTagName(MemoryTag::FrameTemp), "FrameTemp");
}

TEST(MemoryTagTest, RuntimeRegistrationGetsUniqueId) {
    const uint16_t builtin = static_cast<uint16_t>(MemoryTag::BuiltinCount);
    const uint16_t id = RegisterMemoryTag("UnitTestTagA");
    EXPECT_GE(id, builtin);
    EXPECT_STREQ(MemoryTagName(id), "UnitTestTagA");

    const uint16_t id2 = RegisterMemoryTag("UnitTestTagB");
    EXPECT_GT(id2, id);
    EXPECT_STREQ(MemoryTagName(id2), "UnitTestTagB");
    EXPECT_GE(MemoryTagCount(), static_cast<uint16_t>(builtin + 2));
}

TEST(MemoryTagTest, UnknownIdIsSafe) {
    EXPECT_STREQ(MemoryTagName(static_cast<uint16_t>(60000)), "Unknown");
}

// ============================================================
// 对齐辅助
// ============================================================

TEST(MemoryAlignTest, AlignUpAndPowerOfTwo) {
    EXPECT_EQ(AlignUp(0, 16), 0u);
    EXPECT_EQ(AlignUp(1, 16), 16u);
    EXPECT_EQ(AlignUp(16, 16), 16u);
    EXPECT_EQ(AlignUp(17, 16), 32u);
    EXPECT_TRUE(IsPowerOfTwo(1));
    EXPECT_TRUE(IsPowerOfTwo(64));
    EXPECT_FALSE(IsPowerOfTwo(0));
    EXPECT_FALSE(IsPowerOfTwo(48));
}

// ============================================================
// SystemAllocator
// ============================================================

TEST(SystemAllocatorTest, AllocateReturnsAlignedAndTracksSize) {
    SystemAllocator alloc;
    for (size_t alignment : {size_t(8), size_t(16), size_t(32), size_t(64), size_t(256)}) {
        void* p = alloc.Allocate(100, alignment, TagId(MemoryTag::Default));
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignment, 0u);
        EXPECT_EQ(SystemAllocator::AllocatedSize(p), 100u);
        EXPECT_EQ(SystemAllocator::AllocatedTag(p), TagId(MemoryTag::Default));
        alloc.Deallocate(p);
    }
}

TEST(SystemAllocatorTest, StatsReflectLiveBytes) {
    SystemAllocator alloc;
    const MemoryStats before = alloc.TotalStats();
    void* a = alloc.Allocate(1000, 16, TagId(MemoryTag::Texture));
    void* b = alloc.Allocate(2000, 16, TagId(MemoryTag::Mesh));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    const MemoryStats mid = alloc.TotalStats();
    EXPECT_EQ(mid.current - before.current, 3000u);
    EXPECT_EQ(mid.alloc_count - before.alloc_count, 2u);
    EXPECT_GE(mid.peak, 3000u);

    alloc.Deallocate(a);
    alloc.Deallocate(b);
    const MemoryStats after = alloc.TotalStats();
    EXPECT_EQ(after.current, before.current);
    EXPECT_EQ(after.free_count - before.free_count, 2u);
}

TEST(SystemAllocatorTest, ReallocPreservesData) {
    SystemAllocator alloc;
    auto* p = static_cast<uint8_t*>(alloc.Allocate(16, 16, TagId(MemoryTag::Default)));
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 16; ++i) p[i] = static_cast<uint8_t>(i + 1);

    auto* q = static_cast<uint8_t*>(alloc.Reallocate(p, 64, 16, TagId(MemoryTag::Default)));
    ASSERT_NE(q, nullptr);
    for (int i = 0; i < 16; ++i) EXPECT_EQ(q[i], static_cast<uint8_t>(i + 1));
    EXPECT_EQ(SystemAllocator::AllocatedSize(q), 64u);
    alloc.Deallocate(q);
}

TEST(SystemAllocatorTest, NullAndZeroAreSafe) {
    SystemAllocator alloc;
    EXPECT_NO_THROW(alloc.Deallocate(nullptr));
    void* p = alloc.Reallocate(nullptr, 32, 16, TagId(MemoryTag::Default)); // 等价 Allocate
    EXPECT_NE(p, nullptr);
    void* r = alloc.Reallocate(p, 0, 16, TagId(MemoryTag::Default)); // 等价 Free
    EXPECT_EQ(r, nullptr);
}

// ============================================================
// Memory 门面 + New/Delete
// ============================================================

namespace {
struct Tracked {
    static int live;
    int value;
    explicit Tracked(int v) : value(v) { ++live; }
    ~Tracked() { --live; }
};
int Tracked::live = 0;
} // namespace

TEST(MemoryFacadeTest, AllocFreeAndAlignment) {
    Memory::Init();
    void* p = Memory::AllocAligned(128, 64, MemoryTag::Render);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64u, 0u);
    Memory::Free(p);
    EXPECT_NO_THROW(Memory::Free(nullptr));
}

TEST(MemoryFacadeTest, NewDeleteRunsConstructorAndDestructor) {
    Memory::Init();
    const int before = Tracked::live;
    Tracked* t = New<Tracked>(MemoryTag::Default, 42);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->value, 42);
    EXPECT_EQ(Tracked::live, before + 1);
    Delete(t);
    EXPECT_EQ(Tracked::live, before);
}

TEST(MemoryFacadeTest, TotalStatsIncreaseOnAlloc) {
    Memory::Init();
    const MemoryStats before = Memory::TotalStats();
    void* p = Memory::Alloc(4096, MemoryTag::Audio);
    ASSERT_NE(p, nullptr);
    const MemoryStats mid = Memory::TotalStats();
    EXPECT_GE(mid.current, before.current + 4096u);
    Memory::Free(p);
}

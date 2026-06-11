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
#include "engine/core/memory/memory_tracker.h"
#include "engine/core/memory/tracking_allocator.h"

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

// ============================================================
// MemoryTracker（独立实例，避免与全局单例相互干扰）
// ============================================================

TEST(MemoryTrackerTest, PerTagCountsAndPeak) {
    MemoryTracker tracker;
    EXPECT_FALSE(tracker.HasLiveAllocations());

    tracker.OnAlloc(TagId(MemoryTag::Texture), 1000, reinterpret_cast<void*>(0x1));
    tracker.OnAlloc(TagId(MemoryTag::Texture), 500, reinterpret_cast<void*>(0x2));
    tracker.OnAlloc(TagId(MemoryTag::Mesh), 200, reinterpret_cast<void*>(0x3));

    MemoryStats tex = tracker.Stats(TagId(MemoryTag::Texture));
    EXPECT_EQ(tex.current, 1500u);
    EXPECT_EQ(tex.peak, 1500u);
    EXPECT_EQ(tex.alloc_count, 2u);

    tracker.OnFree(TagId(MemoryTag::Texture), 1000, reinterpret_cast<void*>(0x1));
    tex = tracker.Stats(TagId(MemoryTag::Texture));
    EXPECT_EQ(tex.current, 500u);
    EXPECT_EQ(tex.peak, 1500u); // 峰值不回落
    EXPECT_EQ(tex.free_count, 1u);

    MemoryStats total = tracker.TotalStats();
    EXPECT_EQ(total.current, 700u); // 500 (tex) + 200 (mesh)
    EXPECT_TRUE(tracker.HasLiveAllocations());

    // 全部释放后无泄漏
    tracker.OnFree(TagId(MemoryTag::Texture), 500, reinterpret_cast<void*>(0x2));
    tracker.OnFree(TagId(MemoryTag::Mesh), 200, reinterpret_cast<void*>(0x3));
    EXPECT_FALSE(tracker.HasLiveAllocations());
}

// ============================================================
// TrackingAllocator 装饰器（后端无关）
// ============================================================

TEST(TrackingAllocatorTest, WrapsBackendAndReportsToTracker) {
    SystemAllocator backend;
    MemoryTracker tracker;
    TrackingAllocator tracking(&backend, &tracker);

    void* p = tracking.Allocate(300, 32, TagId(MemoryTag::Physics));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 32u, 0u); // 用户对齐保持

    MemoryStats s = tracker.Stats(TagId(MemoryTag::Physics));
    EXPECT_EQ(s.current, 300u);
    EXPECT_EQ(s.alloc_count, 1u);

    tracking.Deallocate(p);
    s = tracker.Stats(TagId(MemoryTag::Physics));
    EXPECT_EQ(s.current, 0u);
    EXPECT_EQ(s.free_count, 1u);
    EXPECT_FALSE(tracker.HasLiveAllocations());
}

TEST(TrackingAllocatorTest, ReallocPreservesDataAndStats) {
    SystemAllocator backend;
    MemoryTracker tracker;
    TrackingAllocator tracking(&backend, &tracker);

    auto* p = static_cast<uint8_t*>(tracking.Allocate(16, 16, TagId(MemoryTag::Scene)));
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 16; ++i) p[i] = static_cast<uint8_t>(i + 100);

    auto* q = static_cast<uint8_t*>(tracking.Reallocate(p, 48, 16, TagId(MemoryTag::Scene)));
    ASSERT_NE(q, nullptr);
    for (int i = 0; i < 16; ++i) EXPECT_EQ(q[i], static_cast<uint8_t>(i + 100));

    MemoryStats s = tracker.Stats(TagId(MemoryTag::Scene));
    EXPECT_EQ(s.current, 48u);
    tracking.Deallocate(q);
    EXPECT_FALSE(tracker.HasLiveAllocations());
}

#if defined(DSE_ENABLE_MEM_TRACKING)
TEST(MemoryFacadeTest, TrackingEnabledStatsPerTag) {
    EXPECT_TRUE(Memory::TrackingEnabled());
    Memory::Init();
    const MemoryStats before = Memory::Stats(MemoryTag::Net);
    void* p = Memory::Alloc(777, MemoryTag::Net);
    ASSERT_NE(p, nullptr);
    const MemoryStats mid = Memory::Stats(MemoryTag::Net);
    EXPECT_EQ(mid.current - before.current, 777u);
    Memory::Free(p);
    const MemoryStats after = Memory::Stats(MemoryTag::Net);
    EXPECT_EQ(after.current, before.current);
}
#else
TEST(MemoryFacadeTest, TrackingDisabledReturnsZeroStats) {
    EXPECT_FALSE(Memory::TrackingEnabled());
    EXPECT_EQ(Memory::Stats(MemoryTag::Net).current, 0u);
}
#endif

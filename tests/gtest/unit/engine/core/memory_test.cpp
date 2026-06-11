/**
 * @file memory_test.cpp
 * @brief 内存子系统阶段1单测：MemoryTag 注册、SystemAllocator 块头/对齐/统计、Memory 门面与 New/Delete。
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include <atomic>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "engine/core/memory/allocator.h"
#include "engine/core/memory/system_allocator.h"
#include "engine/core/memory/memory.h"
#include "engine/core/memory/memory_tracker.h"
#include "engine/core/memory/tracking_allocator.h"
#include "engine/core/memory/linear_allocator.h"
#include "engine/core/memory/frame_allocator.h"
#include "engine/core/memory/pool_allocator.h"
#include "engine/core/object_pool.h"

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

// ============================================================
// LinearAllocator（bump-pointer）
// ============================================================

TEST(LinearAllocatorTest, BumpAllocAlignmentAndUsage) {
    LinearAllocator la;
    la.Init(1024, MemoryTag::FrameTemp);
    ASSERT_TRUE(la.IsInitialized());
    EXPECT_EQ(la.Capacity(), 1024u);
    EXPECT_EQ(la.Used(), 0u);

    void* a = la.Allocate(10, 16);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % 16u, 0u);

    void* b = la.Allocate(1, 64);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(b) % 64u, 0u);
    EXPECT_GT(reinterpret_cast<uintptr_t>(b), reinterpret_cast<uintptr_t>(a));
    EXPECT_GT(la.Used(), 0u);
}

TEST(LinearAllocatorTest, OverflowReturnsNullNoCrash) {
    LinearAllocator la;
    la.Init(64, MemoryTag::FrameTemp);
    void* a = la.Allocate(32, 8);
    ASSERT_NE(a, nullptr);
    void* big = la.Allocate(1000, 8); // 超容量
    EXPECT_EQ(big, nullptr);
    EXPECT_EQ(la.OverflowCount(), 1u);
    // 溢出后仍可继续小额分配
    void* c = la.Allocate(8, 8);
    EXPECT_NE(c, nullptr);
}

TEST(LinearAllocatorTest, ResetAndMarkRewind) {
    LinearAllocator la;
    la.Init(256, MemoryTag::FrameTemp);
    void* first = la.Allocate(32, 16);
    const size_t mark = la.Mark();
    la.Allocate(64, 16);
    EXPECT_GT(la.Used(), mark);
    la.Rewind(mark);
    EXPECT_EQ(la.Used(), mark);

    const size_t hw_before = la.HighWater();
    la.Reset();
    EXPECT_EQ(la.Used(), 0u);
    EXPECT_GE(la.HighWater(), hw_before); // 峰值不回落
    void* again = la.Allocate(32, 16);
    EXPECT_EQ(again, first); // Reset 后复用起始区
}

// ============================================================
// FrameAllocator（多缓冲轮转 + fence 复位）
// ============================================================

TEST(FrameAllocatorTest, RotatesBuffersAndResets) {
    FrameAllocator fa;
    fa.Init(512, 2, MemoryTag::FrameTemp);
    ASSERT_TRUE(fa.IsInitialized());
    EXPECT_EQ(fa.BufferCount(), 2u);

    void* p0 = fa.Alloc(64, 16); // buffer 0
    ASSERT_NE(p0, nullptr);

    fa.BeginFrame();             // -> buffer 1
    void* p1 = fa.Alloc(64, 16);
    ASSERT_NE(p1, nullptr);
    EXPECT_NE(p0, p1);           // 不同缓冲，地址不同（上一帧仍可被消费）

    fa.BeginFrame();             // -> buffer 0（复位）
    void* p2 = fa.Alloc(64, 16);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2, p0);           // 回到 buffer 0 起始
    EXPECT_EQ(fa.FrameNumber(), 2u);
}

TEST(FrameAllocatorTest, ClampsBufferCount) {
    FrameAllocator fa;
    fa.Init(128, 1, MemoryTag::FrameTemp); // <2 应被夹到 2
    EXPECT_EQ(fa.BufferCount(), 2u);
    fa.Shutdown();
    fa.Init(128, 99, MemoryTag::FrameTemp); // >max 应被夹到上限
    EXPECT_LE(fa.BufferCount(), kMaxFrameBuffers);
}

// ============================================================
// ThreadScratch（每线程私有、零争用）
// ============================================================

TEST(ThreadScratchTest, PerThreadDistinctBuffersNoRace) {
    constexpr int kThreads = 8;
    std::vector<std::thread> threads;
    std::mutex mtx;
    std::set<void*> bases;
    std::atomic<int> failures{0};
    // 屏障：确保捕获基址时所有线程都还存活（否则退出线程的缓冲会被复用，地址冲突）。
    std::atomic<int> arrived{0};

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            LinearAllocator& scratch = Memory::ThreadScratch();
            void* base = scratch.Allocate(16, 16);
            {
                std::lock_guard<std::mutex> lock(mtx);
                bases.insert(base);
            }
            arrived.fetch_add(1, std::memory_order_acq_rel);
            while (arrived.load(std::memory_order_acquire) < kThreads) {
                std::this_thread::yield();
            }
            // 反复在私有 scratch 上分配并写入，验证无串扰
            for (int iter = 0; iter < 1000; ++iter) {
                auto* buf = static_cast<uint8_t*>(scratch.Allocate(64, 16));
                if (buf == nullptr) {
                    scratch.Reset();
                    buf = static_cast<uint8_t*>(scratch.Allocate(64, 16));
                }
                if (buf == nullptr) { ++failures; break; }
                const uint8_t marker = static_cast<uint8_t>(t * 7 + iter);
                std::memset(buf, marker, 64);
                for (int k = 0; k < 64; ++k) {
                    if (buf[k] != marker) { ++failures; break; }
                }
            }
        });
    }
    for (auto& th : threads) th.join();

    EXPECT_EQ(failures.load(), 0);
    // 每个线程应得到独立 scratch 缓冲（基址互不相同）。
    EXPECT_EQ(bases.size(), static_cast<size_t>(kThreads));
}

// ============================================================
// PoolAllocator（固定大小块池）
// ============================================================

TEST(PoolAllocatorTest, AllocateFreeReuseNoFragmentation) {
    PoolAllocator pool;
    pool.Init(sizeof(int), alignof(int), 4, MemoryTag::Default);
    ASSERT_TRUE(pool.IsInitialized());
    EXPECT_GE(pool.Capacity(), 4u);
    EXPECT_EQ(pool.UsedCount(), 0u);

    void* a = pool.Allocate();
    void* b = pool.Allocate();
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
    EXPECT_EQ(pool.UsedCount(), 2u);

    // 归还后再取应复用同一块（无碎片）。
    pool.Free(b);
    void* c = pool.Allocate();
    EXPECT_EQ(c, b);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % alignof(int), 0u);
}

TEST(PoolAllocatorTest, AutoGrowsBeyondInitialCapacity) {
    PoolAllocator pool;
    pool.Init(64, 16, 2, MemoryTag::Default);
    const size_t initial = pool.Capacity();
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; ++i) {
        void* p = pool.Allocate();
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 16u, 0u); // 对齐保持
        ptrs.push_back(p);
    }
    EXPECT_GT(pool.Capacity(), initial); // 触发了扩容
    EXPECT_GT(pool.ChunkCount(), 1u);
    for (void* p : ptrs) pool.Free(p);
    EXPECT_EQ(pool.UsedCount(), 0u);
}

TEST(PoolAllocatorTest, BlockStrideHonorsMinAndAlignment) {
    PoolAllocator pool;
    pool.Init(1, 8, 4, MemoryTag::Default); // 1 字节请求 → 抬升到 >= sizeof(void*) 且 8 对齐
    EXPECT_GE(pool.BlockSize(), sizeof(void*));
    EXPECT_EQ(pool.BlockSize() % 8u, 0u);
}

// ============================================================
// ObjectPool（原地构造 + 析构）
// ============================================================

TEST(ObjectPoolTest, AcquireConstructsReleaseDestructsInPlace) {
    static int live = 0;
    struct Tracked {
        int v;
        explicit Tracked(int x) : v(x) { ++live; }
        ~Tracked() { --live; }
    };
    {
        ObjectPool<Tracked> pool(4);
        EXPECT_EQ(pool.AvailableCount(), 4u);
        Tracked* a = pool.Acquire(7);
        ASSERT_NE(a, nullptr);
        EXPECT_EQ(a->v, 7);       // 构造参数透传
        EXPECT_EQ(live, 1);       // 构造函数已执行
        EXPECT_EQ(pool.UsedCount(), 1u);
        pool.Release(a);
        EXPECT_EQ(live, 0);       // 析构函数已执行
        EXPECT_EQ(pool.UsedCount(), 0u);
    }
}

TEST(ObjectPoolTest, CycleReuseNoLeak) {
    ObjectPool<int> pool(8);
    const size_t cap = pool.Capacity();
    for (int i = 0; i < 1000; ++i) {
        int* p = pool.Acquire(i);
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, i);
        pool.Release(p);
    }
    EXPECT_EQ(pool.UsedCount(), 0u);
    EXPECT_EQ(pool.Capacity(), cap); // 反复借还不增长容量
}

TEST(ObjectPoolTest, SupportsNonCopyableType) {
    struct NonCopyable {
        int id;
        explicit NonCopyable(int i) : id(i) {}
        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };
    ObjectPool<NonCopyable> pool(2);
    NonCopyable* a = pool.Acquire(1);
    NonCopyable* b = pool.Acquire(2);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->id, 1);
    EXPECT_EQ(b->id, 2);
    pool.Release(a);
    pool.Release(b);
}

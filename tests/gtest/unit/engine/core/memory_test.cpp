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
#include "engine/core/memory/stl_allocator.h"
#include "engine/core/memory/handle.h"
#include "engine/core/memory/mimalloc_allocator.h"
#include "engine/core/object_pool.h"

using namespace dse::core;

// ============================================================
// MemoryTag 注册表
// ============================================================

// 测试 内存标签：内置名称解析
TEST(MemoryTagTest, BuiltinNamesResolve) {
    EXPECT_STREQ(MemoryTagName(MemoryTag::Default), "Default");
    EXPECT_STREQ(MemoryTagName(MemoryTag::Render), "Render");
    EXPECT_STREQ(MemoryTagName(MemoryTag::FrameTemp), "FrameTemp");
}

// 测试 内存标签：运行时注册获取Unique ID
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

// 测试 内存标签：未知ID为安全
TEST(MemoryTagTest, UnknownIdIsSafe) {
    EXPECT_STREQ(MemoryTagName(static_cast<uint16_t>(60000)), "Unknown");
}

// ============================================================
// 对齐辅助
// ============================================================

// 测试 内存Align：Align上且Power的两个
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

// 测试 系统分配器：分配返回Aligned且Tracks尺寸
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

// 测试 系统分配器：统计反映Live Bytes
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

// 测试 系统分配器：Realloc Preserves数据
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

// 测试 系统分配器：空且零为安全
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

// 测试 内存门面：分配释放且对齐
TEST(MemoryFacadeTest, AllocFreeAndAlignment) {
    Memory::Init();
    void* p = Memory::AllocAligned(128, 64, MemoryTag::Render);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 64u, 0u);
    Memory::Free(p);
    EXPECT_NO_THROW(Memory::Free(nullptr));
}

// 测试 内存门面：新建删除Runs Constructor且Destructor
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

// 测试 内存门面：总计统计Increase开启分配
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

// 测试 内存Tracker：每标签Counts且峰值
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

// 测试 追踪分配器：Wraps后端且Reports到Tracker
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

// 测试 追踪分配器：Realloc Preserves数据且统计
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
// 测试 内存门面：追踪启用统计每标签
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
// 测试 内存门面：追踪禁用返回零统计
TEST(MemoryFacadeTest, TrackingDisabledReturnsZeroStats) {
    EXPECT_FALSE(Memory::TrackingEnabled());
    EXPECT_EQ(Memory::Stats(MemoryTag::Net).current, 0u);
}
#endif

// ============================================================
// LinearAllocator（bump-pointer）
// ============================================================

// 测试 线性分配器：Bump分配对齐且用量
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

// 测试 线性分配器：溢出返回空无崩溃
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

// 测试 线性分配器：重置且标记Rewind
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

// 测试 帧分配器：Rotates缓冲区且Resets
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

// 测试 帧分配器：Clamps缓冲区计数
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

// 测试 线程Scratch：每线程Distinct缓冲区无Race
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

// 测试 对象池分配器：分配释放复用无Fragmentation
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

// 测试 对象池分配器：自动增长Beyond初始容量
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

// 测试 对象池分配器：Block Stride Honors Min且对齐
TEST(PoolAllocatorTest, BlockStrideHonorsMinAndAlignment) {
    PoolAllocator pool;
    pool.Init(1, 8, 4, MemoryTag::Default); // 1 字节请求 → 抬升到 >= sizeof(void*) 且 8 对齐
    EXPECT_GE(pool.BlockSize(), sizeof(void*));
    EXPECT_EQ(pool.BlockSize() % 8u, 0u);
}

// ============================================================
// ObjectPool（原地构造 + 析构）
// ============================================================

// 测试 对象对象池：获取Constructs释放Destructs于Place
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

// 测试 对象对象池：周期复用无泄漏
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

// 测试 对象对象池：支持非Copyable类型
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

// ============================================================
// MemoryBudget（独立实例：预算登记 + 越限沿回调）
// ============================================================

namespace {
struct BudgetProbe {
    static std::atomic<int> calls;
    static std::atomic<uint16_t> last_tag;
    static std::atomic<size_t> last_usage;
    static std::atomic<size_t> last_budget;
    static void Reset() {
        calls.store(0);
        last_tag.store(0);
        last_usage.store(0);
        last_budget.store(0);
    }
};
std::atomic<int> BudgetProbe::calls{0};
std::atomic<uint16_t> BudgetProbe::last_tag{0};
std::atomic<size_t> BudgetProbe::last_usage{0};
std::atomic<size_t> BudgetProbe::last_budget{0};
} // namespace

// 测试 内存预算：设置获取预算且外部用量
TEST(MemoryBudgetTest, SetGetBudgetAndExternalUsage) {
    MemoryBudget budget;
    EXPECT_EQ(budget.GetBudget(TagId(MemoryTag::Texture)), 0u); // 默认不限
    budget.SetBudget(TagId(MemoryTag::Texture), 4096);
    EXPECT_EQ(budget.GetBudget(TagId(MemoryTag::Texture)), 4096u);

    EXPECT_EQ(budget.ExternalUsage(TagId(MemoryTag::Texture)), 0u);
    budget.SetExternalUsage(TagId(MemoryTag::Texture), 1234);
    EXPECT_EQ(budget.ExternalUsage(TagId(MemoryTag::Texture)), 1234u);
}

// 测试 内存预算：回调Fires Once开启Rising Edge
TEST(MemoryBudgetTest, CallbackFiresOnceOnRisingEdge) {
    BudgetProbe::Reset();
    MemoryBudget budget;
    budget.SetExceededCallback([](uint16_t tag, size_t usage, size_t b) {
        BudgetProbe::calls.fetch_add(1);
        BudgetProbe::last_tag.store(tag);
        BudgetProbe::last_usage.store(usage);
        BudgetProbe::last_budget.store(b);
    });
    budget.SetBudget(TagId(MemoryTag::Mesh), 1000);

    budget.CheckBudget(TagId(MemoryTag::Mesh), 500); // 未超
    EXPECT_EQ(BudgetProbe::calls.load(), 0);

    budget.CheckBudget(TagId(MemoryTag::Mesh), 1500); // 越限 -> 触发一次
    EXPECT_EQ(BudgetProbe::calls.load(), 1);
    EXPECT_EQ(BudgetProbe::last_tag.load(), TagId(MemoryTag::Mesh));
    EXPECT_EQ(BudgetProbe::last_usage.load(), 1500u);
    EXPECT_EQ(BudgetProbe::last_budget.load(), 1000u);

    budget.CheckBudget(TagId(MemoryTag::Mesh), 1600); // 仍超 -> 不重复
    EXPECT_EQ(BudgetProbe::calls.load(), 1);

    budget.CheckBudget(TagId(MemoryTag::Mesh), 800);  // 回落到预算内
    budget.CheckBudget(TagId(MemoryTag::Mesh), 1200); // 再次越限 -> 再触发
    EXPECT_EQ(BudgetProbe::calls.load(), 2);
}

// 测试 内存预算：零预算为Unlimited
TEST(MemoryBudgetTest, ZeroBudgetIsUnlimited) {
    BudgetProbe::Reset();
    MemoryBudget budget;
    budget.SetExceededCallback([](uint16_t, size_t, size_t) { BudgetProbe::calls.fetch_add(1); });
    budget.CheckBudget(TagId(MemoryTag::Audio), size_t(1) << 30); // 预算 0：任意用量都不触发
    EXPECT_EQ(BudgetProbe::calls.load(), 0);
}

// 测试 内存预算：默认路径无回调为安全
TEST(MemoryBudgetTest, DefaultPathWithoutCallbackIsSafe) {
    MemoryBudget budget;
    budget.SetBudget(TagId(MemoryTag::Physics), 100);
    EXPECT_NO_THROW(budget.CheckBudget(TagId(MemoryTag::Physics), 200)); // 无回调走默认告警日志
}

// ============================================================
// Memory 门面预算（全局单例：用专用 tag 并在用例末复位）
// ============================================================

// 测试 内存预算门面：设置预算报告用量且溢出
TEST(MemoryBudgetFacadeTest, SetBudgetReportUsageAndOverflow) {
    const MemoryTag tag = MemoryTag::Navigation;
    Memory::SetBudgetExceededCallback(nullptr);
    Memory::ReportExternalUsage(tag, 0);
    Memory::SetBudget(tag, 0);

    constexpr size_t kBudget = 1u << 20; // 1 MB
    Memory::SetBudget(tag, kBudget);
    EXPECT_EQ(Memory::GetBudget(tag), kBudget);
    EXPECT_FALSE(Memory::IsOverBudget(tag));

    Memory::ReportExternalUsage(tag, 256u * 1024);
    EXPECT_GE(Memory::BudgetUsage(tag), 256u * 1024);
    EXPECT_FALSE(Memory::IsOverBudget(tag));

    Memory::ReportExternalUsage(tag, 4u * 1024 * 1024);
    EXPECT_TRUE(Memory::IsOverBudget(tag));
    EXPECT_GE(Memory::BudgetUsage(tag), 4u * 1024 * 1024);

    EXPECT_NO_THROW(Memory::ReportBudgets());

    Memory::ReportExternalUsage(tag, 0);
    Memory::SetBudget(tag, 0);
}

// 测试 内存预算门面：用量Combines Tracked且外部
TEST(MemoryBudgetFacadeTest, UsageCombinesTrackedAndExternal) {
    const MemoryTag tag = MemoryTag::Editor;
    Memory::SetBudgetExceededCallback(nullptr);
    Memory::SetBudget(tag, 0);
    Memory::ReportExternalUsage(tag, 0);
    Memory::Init();

    const size_t base = Memory::BudgetUsage(tag);
    Memory::ReportExternalUsage(tag, 5000);
    EXPECT_EQ(Memory::BudgetUsage(tag) - base, 5000u); // 仅外部用量变化

    void* p = Memory::Alloc(3000, tag);
    ASSERT_NE(p, nullptr);
    if (Memory::TrackingEnabled()) {
        EXPECT_GE(Memory::BudgetUsage(tag), 5000u + 3000u); // 含门面追踪当前量
    } else {
        EXPECT_EQ(Memory::BudgetUsage(tag), 5000u); // 无追踪时仅外部用量
    }
    Memory::Free(p);

    Memory::ReportExternalUsage(tag, 0);
    Memory::SetBudget(tag, 0);
}

// ============================================================
// StlAllocator + Dse* 容器别名（无状态、转发门面、标签化）
// ============================================================

// 测试 STL分配器：Stateless且相等
TEST(StlAllocatorTest, StatelessAndEquality) {
    static_assert(std::is_empty_v<StlAllocator<int, MemoryTag::Mesh>>, "must be stateless");
    static_assert(StlAllocator<int, MemoryTag::Mesh>::is_always_equal::value, "always equal");
    static_assert(StlAllocator<int, MemoryTag::Mesh>::kTag == MemoryTag::Mesh, "tag exposed");

    StlAllocator<int, MemoryTag::Mesh> a;
    StlAllocator<int, MemoryTag::Mesh> b;
    StlAllocator<int, MemoryTag::Texture> c;
    EXPECT_TRUE(a == b);  // 同 tag 恒等价
    EXPECT_TRUE(a != c);  // 不同 tag 视为不等

    using Rebound = StlAllocator<int, MemoryTag::Mesh>::rebind<double>::other;
    static_assert(std::is_same_v<Rebound, StlAllocator<double, MemoryTag::Mesh>>, "rebind keeps tag");
}

// 仅在启用按标签内存追踪（Debug + DSE_ENABLE_MEM_TRACKING）时注册：
// 关闭时这些标签计费断言无意义，故编译期排除而非运行时跳过。
#if defined(DSE_ENABLE_MEM_TRACKING)
// 测试 STL分配器：向量Allocations为Tagged且Routed到门面
TEST(StlAllocatorTest, VectorAllocationsAreTaggedAndRoutedToFacade) {
    Memory::Init();
    const MemoryTag tag = MemoryTag::Scripting;
    const MemoryStats before = Memory::Stats(tag);
    {
        DseVector<std::uint64_t, MemoryTag::Scripting> v;
        v.reserve(4096); // 预留 -> 单次堆分配，避免扩容
        for (std::uint64_t i = 0; i < 4096; ++i) {
            v.push_back(i);
        }
        const MemoryStats mid = Memory::Stats(tag);
        EXPECT_GE(mid.current - before.current, 4096u * sizeof(std::uint64_t));
        EXPECT_GT(mid.alloc_count, before.alloc_count);
        EXPECT_EQ(v.front(), 0u);
        EXPECT_EQ(v.back(), 4095u);
    }
    EXPECT_EQ(Memory::Stats(tag).current, before.current); // 析构后增量归零
}

// 测试 STL分配器：字符串Heap分配为Tagged
TEST(StlAllocatorTest, StringHeapAllocationIsTagged) {
    Memory::Init();
    const MemoryTag tag = MemoryTag::Scripting;
    const MemoryStats before = Memory::Stats(tag);
    {
        DseString<MemoryTag::Scripting> s(512, 'x'); // 远超 SSO，强制堆分配
        const MemoryStats mid = Memory::Stats(tag);
        EXPECT_GE(mid.current - before.current, 512u);
        EXPECT_EQ(s.size(), 512u);
        EXPECT_EQ(s.front(), 'x');
    }
    EXPECT_EQ(Memory::Stats(tag).current, before.current);
}
#endif // DSE_ENABLE_MEM_TRACKING

// 测试 STL分配器：节点Containers Work Via重新绑定
TEST(StlAllocatorTest, NodeContainersWorkViaRebind) {
    DseUnorderedMap<int, std::string, MemoryTag::Scripting> m;
    m.emplace(1, "one");
    m.emplace(2, "two");
    EXPECT_EQ(m.size(), 2u);
    EXPECT_EQ(m.at(1), "one");

    DseMap<int, int, MemoryTag::Scripting> om;
    om[3] = 30;
    om[1] = 10;
    om[2] = 20;
    std::vector<int> keys;
    for (const auto& kv : om) {
        keys.push_back(kv.first);
    }
    EXPECT_EQ(keys, (std::vector<int>{1, 2, 3})); // map 有序

    DseList<int, MemoryTag::Scripting> lst{1, 2, 3};
    EXPECT_EQ(lst.size(), 3u);

    DseSet<int, MemoryTag::Scripting> st{5, 5, 7};
    EXPECT_EQ(st.size(), 2u); // 去重
}

// ============================================================
// Handle<T> + HandleTable<T, Tag>（index+generation 句柄化资源表）
// ============================================================

namespace {
/// 统计构造/析构次数，验证 HandleTable 正确管理对象生命周期。
struct LifeTracked {
    static int alive;
    int value;
    explicit LifeTracked(int v) : value(v) { ++alive; }
    ~LifeTracked() { --alive; }
};
int LifeTracked::alive = 0;
} // namespace

// 测试 句柄表：创建获取销毁
TEST(HandleTableTest, CreateGetDestroy) {
    HandleTable<int, MemoryTag::Default> table;
    EXPECT_TRUE(table.Empty());

    Handle<int> h = table.Create(42);
    EXPECT_TRUE(h.IsValid());
    EXPECT_TRUE(table.IsValid(h));
    ASSERT_NE(table.Get(h), nullptr);
    EXPECT_EQ(*table.Get(h), 42);
    EXPECT_EQ(table.Size(), 1u);

    EXPECT_TRUE(table.Destroy(h));
    EXPECT_FALSE(table.IsValid(h));
    EXPECT_EQ(table.Get(h), nullptr);
    EXPECT_EQ(table.Size(), 0u);
    EXPECT_FALSE(table.Destroy(h)); // 重复销毁失败
}

// 测试 句柄表：代次Invalidates Stale句柄
TEST(HandleTableTest, GenerationInvalidatesStaleHandle) {
    HandleTable<int> table;
    Handle<int> h1 = table.Create(1);
    EXPECT_TRUE(table.Destroy(h1));

    // 复用同一槽位，generation 自增 -> 旧句柄失效、新句柄有效。
    Handle<int> h2 = table.Create(2);
    EXPECT_EQ(h1.index, h2.index);      // 槽位被复用
    EXPECT_NE(h1, h2);                   // 但 generation 不同
    EXPECT_FALSE(table.IsValid(h1));     // 旧句柄失效（悬垂安全）
    EXPECT_EQ(table.Get(h1), nullptr);
    ASSERT_NE(table.Get(h2), nullptr);
    EXPECT_EQ(*table.Get(h2), 2);
}

// 测试 句柄表：Constructs且Destructs Objects
TEST(HandleTableTest, ConstructsAndDestructsObjects) {
    LifeTracked::alive = 0;
    {
        HandleTable<LifeTracked, MemoryTag::Default> table;
        Handle<LifeTracked> a = table.Create(10);
        Handle<LifeTracked> b = table.Create(20);
        EXPECT_EQ(LifeTracked::alive, 2);
        EXPECT_EQ(table.Get(a)->value, 10);

        table.Destroy(a);
        EXPECT_EQ(LifeTracked::alive, 1); // 析构被调用
        EXPECT_EQ(table.Get(b)->value, 20);
        // table 出作用域析构剩余对象
    }
    EXPECT_EQ(LifeTracked::alive, 0); // 表析构清理所有存活对象
}

// 测试 句柄表：尺寸容量且复用
TEST(HandleTableTest, SizeCapacityAndReuse) {
    HandleTable<int> table;
    Handle<int> a = table.Create(1);
    Handle<int> b = table.Create(2);
    Handle<int> c = table.Create(3);
    EXPECT_EQ(table.Size(), 3u);
    EXPECT_EQ(table.Capacity(), 3u);

    table.Destroy(b);
    EXPECT_EQ(table.Size(), 2u);
    EXPECT_EQ(table.Capacity(), 3u); // 槽位保留

    Handle<int> d = table.Create(4); // 复用空闲槽，不新增容量
    EXPECT_EQ(table.Size(), 3u);
    EXPECT_EQ(table.Capacity(), 3u);
    EXPECT_EQ(*table.Get(a), 1);
    EXPECT_EQ(*table.Get(c), 3);
    EXPECT_EQ(*table.Get(d), 4);
}

// 测试 句柄表：默认句柄为无效
TEST(HandleTableTest, DefaultHandleIsInvalid) {
    HandleTable<int> table;
    Handle<int> def;
    EXPECT_FALSE(def.IsValid());
    EXPECT_FALSE(table.IsValid(def));
    EXPECT_EQ(table.Get(def), nullptr);
}

// ============================================================
// MimallocAllocator（仅 DSE_MEM_BACKEND=mimalloc 时编译/运行）
// ============================================================

#if defined(DSE_MEM_USE_MIMALLOC)
// 测试 Mimalloc分配器：分配释放且统计
TEST(MimallocAllocatorTest, AllocateFreeAndStats) {
    MimallocAllocator a;
    void* p = a.Allocate(1000, 32, TagId(MemoryTag::Default));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % 32u, 0u); // 对齐
    const MemoryStats s = a.TotalStats();
    EXPECT_GE(s.current, 1000u);
    EXPECT_EQ(s.alloc_count, 1u);

    void* q = a.Reallocate(p, 4000, 32, TagId(MemoryTag::Default));
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(q) % 32u, 0u);

    a.Deallocate(q);
    EXPECT_EQ(a.TotalStats().current, 0u);
}
#endif

/**
 * @file memory.cpp
 * @brief Memory 门面实现（阶段1：转发到 SystemAllocator）。
 */

#include "engine/core/memory/memory.h"
#include "engine/core/memory/system_allocator.h"
#include "engine/core/memory/mimalloc_allocator.h"
#include "engine/core/memory/frame_allocator.h"
#include "engine/core/memory/linear_allocator.h"
#include "engine/core/memory/memory_tracker.h"  // MemoryBudget（与追踪开关无关，始终可用）
#include "engine/base/debug.h"

#if defined(DSE_ENABLE_MEM_TRACKING)
#include "engine/core/memory/tracking_allocator.h"
#endif

namespace dse {
namespace core {

namespace {

// 仅编译当前生效的后端单例（按 CMake DSE_MEM_BACKEND 编译期选择），避免未用函数告警。
#if defined(DSE_MEM_USE_MIMALLOC)
/// mimalloc 后端单例。Meyers 单例规避静态初始化顺序问题。
MimallocAllocator& BackendInstance() {
    static MimallocAllocator instance;
    return instance;
}
#else
/// system 后端单例。Meyers 单例规避静态初始化顺序问题。
SystemAllocator& BackendInstance() {
    static SystemAllocator instance;
    return instance;
}
#endif

/// 当前生效的通用堆后端（IAllocator 视图）。
IAllocator& BackendHeap() {
    return BackendInstance();
}

/// 当前后端的总量统计（具体类型，旁路 IAllocator 的非虚 TotalStats）。
[[maybe_unused]] MemoryStats BackendTotalStats() {
    return BackendInstance().TotalStats();
}

#if defined(DSE_ENABLE_MEM_TRACKING)
/// 追踪装饰器，包裹生效后端并上报 MemoryTracker。
TrackingAllocator& TrackingHeap() {
    static TrackingAllocator instance(&BackendHeap(), &MemoryTracker::Instance());
    return instance;
}
#endif

} // namespace

IAllocator* Memory::heap_ = nullptr;
MemoryConfig Memory::config_;

void Memory::Init(const MemoryConfig& config) {
    if (heap_ == nullptr) {
        config_ = config;
#if defined(DSE_ENABLE_MEM_TRACKING)
        heap_ = &TrackingHeap();
#else
        heap_ = &BackendHeap();
#endif
    }
}

void Memory::Shutdown() {
    ReportLeaks();
    // 后端为进程级，刻意不在此销毁：进程退出期仍可能有晚期释放。
}

IAllocator& Memory::Heap() {
    if (heap_ == nullptr) {
        Init();
    }
    return *heap_;
}

bool Memory::TrackingEnabled() {
#if defined(DSE_ENABLE_MEM_TRACKING)
    return true;
#else
    return false;
#endif
}

void* Memory::Alloc(size_t size, MemoryTag tag) {
    return Heap().Allocate(size, kDefaultAlignment, TagId(tag));
}

void* Memory::AllocAligned(size_t size, size_t alignment, MemoryTag tag) {
    return Heap().Allocate(size, alignment, TagId(tag));
}

void* Memory::Realloc(void* ptr, size_t new_size, MemoryTag tag) {
    return Heap().Reallocate(ptr, new_size, kDefaultAlignment, TagId(tag));
}

void Memory::Free(void* ptr) {
    Heap().Deallocate(ptr);
}

MemoryStats Memory::TotalStats() {
#if defined(DSE_ENABLE_MEM_TRACKING)
    return MemoryTracker::Instance().TotalStats();
#else
    return BackendTotalStats();
#endif
}

MemoryStats Memory::Stats(MemoryTag tag) {
#if defined(DSE_ENABLE_MEM_TRACKING)
    return MemoryTracker::Instance().Stats(TagId(tag));
#else
    (void)tag;
    return MemoryStats{};
#endif
}

void Memory::ReportLeaks() {
#if defined(DSE_ENABLE_MEM_TRACKING)
    MemoryTracker::Instance().Report("Memory::ReportLeaks");
#else
    const MemoryStats s = BackendTotalStats();
    DEBUG_LOG_INFO("[Memory] heap={} current={} bytes peak={} bytes allocs={} frees={}",
                   BackendHeap().Name(), s.current, s.peak, s.alloc_count, s.free_count);
    if (s.current != 0) {
        DEBUG_LOG_WARN("[Memory] {} bytes still live at report (live allocations={})",
                       s.current, s.alloc_count - s.free_count);
    }
#endif
}

void Memory::SetBudget(MemoryTag tag, size_t bytes) {
    MemoryBudget& b = MemoryBudget::Instance();
    b.SetBudget(TagId(tag), bytes);
    b.CheckBudget(TagId(tag), BudgetUsage(tag));
}

size_t Memory::GetBudget(MemoryTag tag) {
    return MemoryBudget::Instance().GetBudget(TagId(tag));
}

void Memory::ReportExternalUsage(MemoryTag tag, size_t current_bytes) {
    MemoryBudget& b = MemoryBudget::Instance();
    b.SetExternalUsage(TagId(tag), current_bytes);
    b.CheckBudget(TagId(tag), BudgetUsage(tag));
}

size_t Memory::BudgetUsage(MemoryTag tag) {
    size_t usage = MemoryBudget::Instance().ExternalUsage(TagId(tag));
#if defined(DSE_ENABLE_MEM_TRACKING)
    usage += MemoryTracker::Instance().Stats(TagId(tag)).current;
#endif
    return usage;
}

bool Memory::IsOverBudget(MemoryTag tag) {
    const size_t budget = MemoryBudget::Instance().GetBudget(TagId(tag));
    return budget != 0 && BudgetUsage(tag) > budget;
}

void Memory::SetBudgetExceededCallback(BudgetExceededCallback cb) {
    MemoryBudget::Instance().SetExceededCallback(cb);
}

void Memory::ReportBudgets() {
    MemoryBudget& b = MemoryBudget::Instance();
    DEBUG_LOG_INFO("[Memory] budget report (usage = facade-tracked current + reported external)");
    bool any = false;
    const uint16_t count = MemoryTagCount();
    for (uint16_t i = 0; i < count; ++i) {
        const size_t budget = b.GetBudget(i);
        size_t usage = b.ExternalUsage(i);
#if defined(DSE_ENABLE_MEM_TRACKING)
        usage += MemoryTracker::Instance().Stats(i).current;
#endif
        if (budget == 0 && usage == 0) {
            continue;
        }
        any = true;
        const bool over = budget != 0 && usage > budget;
        DEBUG_LOG_INFO("  tag={} usage={} bytes budget={} bytes{}",
                       MemoryTagName(i), usage, budget, over ? " [OVER]" : "");
        if (over) {
            DEBUG_LOG_WARN("  [BUDGET] tag={} over by {} bytes",
                           MemoryTagName(i), usage - budget);
        }
    }
    if (!any) {
        DEBUG_LOG_INFO("  (no budgets set)");
    }
}

FrameAllocator& Memory::Frame() {
    // 进程级单一主线程帧分配器；首次访问惰性按 config_ 初始化。
    static FrameAllocator instance;
    if (!instance.IsInitialized()) {
        if (heap_ == nullptr) {
            Init();
        }
        instance.Init(config_.frame_buffer_bytes, config_.frame_buffer_count, MemoryTag::FrameTemp);
    }
    return instance;
}

void* Memory::FrameAlloc(size_t size, size_t alignment) {
    return Frame().Alloc(size, alignment);
}

LinearAllocator& Memory::ThreadScratch() {
    // 线程私有：每线程一份，零争用。
    thread_local LinearAllocator scratch;
    if (!scratch.IsInitialized()) {
        if (heap_ == nullptr) {
            Init();
        }
        scratch.Init(config_.scratch_bytes, MemoryTag::FrameTemp);
    }
    return scratch;
}

} // namespace core
} // namespace dse

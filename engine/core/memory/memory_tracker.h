/**
 * @file memory_tracker.h
 * @brief 按标签聚合的内存统计与泄漏追踪（Debug 默认开，Release 默认关）。
 *
 * 口径：默认只统计经 Memory 门面（TrackingAllocator）的分配——即引擎自管部分。
 * 第三方库与未改造代码不在内，报告时显式标注口径（见设计文档 §3.7）。
 *
 * 计数采用按标签原子计数（标签独立，争用低），可选逐指针登记用于定位泄漏。
 */

#ifndef DSE_CORE_MEMORY_MEMORY_TRACKER_H
#define DSE_CORE_MEMORY_MEMORY_TRACKER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#if defined(DSE_MEM_TRACK_POINTERS)
#include <mutex>
#include <unordered_map>
#endif
#include "engine/core/memory/allocator.h"

namespace dse {
namespace core {

/// 追踪器支持的最大标签数（内置 + 运行期注册）。超出部分归并到 Default 桶。
constexpr uint16_t kMaxTrackedTags = 256;

/**
 * @class MemoryTracker
 * @brief 进程级单例，记录每标签的当前/峰值占用与分配/释放次数。
 */
class DSE_EXPORT MemoryTracker {
public:
    /// 进程级单例（门面使用）。也可直接构造独立实例（便于测试隔离）。
    MemoryTracker() = default;

    static MemoryTracker& Instance();

    /// 记录一次分配。
    void OnAlloc(uint16_t tag, size_t size, void* ptr);
    /// 记录一次释放。
    void OnFree(uint16_t tag, size_t size, void* ptr);

    /// 某标签统计快照。
    MemoryStats Stats(uint16_t tag) const;
    /// 所有标签聚合快照。
    MemoryStats TotalStats() const;

    /// 是否存在未释放的活跃分配（current>0 或 alloc!=free）。
    bool HasLiveAllocations() const;

    /// 输出按标签的占用/泄漏报告到日志，标注统计口径。
    void Report(const char* scope_label) const;

#if defined(DSE_MEM_TRACK_POINTERS)
    /// 逐指针登记下，返回当前活跃指针数（用于测试/诊断）。
    size_t LivePointerCount() const;
#endif

private:
    struct TagCounters {
        std::atomic<size_t> current{0};
        std::atomic<size_t> peak{0};
        std::atomic<size_t> alloc_count{0};
        std::atomic<size_t> free_count{0};
    };

    uint16_t Bucket(uint16_t tag) const { return tag < kMaxTrackedTags ? tag : 0; }

    TagCounters tags_[kMaxTrackedTags];

#if defined(DSE_MEM_TRACK_POINTERS)
    struct PointerInfo {
        uint16_t tag;
        size_t size;
    };
    mutable std::mutex pointers_mutex_;
    std::unordered_map<void*, PointerInfo> pointers_;
#endif
};

/**
 * @class MemoryBudget
 * @brief 按标签的内存预算登记表（与追踪开关无关，始终可用）。
 *
 * 仅存储「每标签预算」与「子系统上报的外部用量」，本身不感知门面追踪量；
 * 用量越限判定由调用方（Memory 门面）传入合并后的 usage 完成（见 §3.8）。
 * 对接 AssetManager 等自管内存（shared_ptr/LRU）的子系统：它们经
 * Memory::ReportExternalUsage 把估算用量纳入统一视图，而行为不变。
 */
class DSE_EXPORT MemoryBudget {
public:
    /// 进程级单例（门面使用）。也可直接构造独立实例（便于测试隔离）。
    MemoryBudget() = default;

    static MemoryBudget& Instance();

    /// 设定某标签预算（字节）；0 表示不限。
    void SetBudget(uint16_t tag, size_t bytes);
    /// 读取某标签预算（字节）；0 表示不限。
    size_t GetBudget(uint16_t tag) const;

    /// 设定某标签的外部用量（绝对值，字节）——供自管内存的子系统上报。
    void SetExternalUsage(uint16_t tag, size_t current_bytes);
    /// 读取某标签的外部用量（字节）。
    size_t ExternalUsage(uint16_t tag) const;

    /// 注册超限回调（替代默认告警日志）；传 nullptr 恢复默认。
    void SetExceededCallback(BudgetExceededCallback cb);

    /**
     * @brief 用合并后的 usage 评估某标签是否越限，按需触发回调/告警。
     *
     * 越限沿（从未超到超）只触发一次，回落到预算内后重置，避免日志刷屏。
     * usage 由调用方提供（通常 = 门面追踪当前量 + 外部上报量）。
     */
    void CheckBudget(uint16_t tag, size_t usage);

private:
    uint16_t Bucket(uint16_t tag) const { return tag < kMaxTrackedTags ? tag : 0; }

    struct TagBudget {
        std::atomic<size_t> budget{0};    ///< 预算字节（0=不限）
        std::atomic<size_t> external{0};  ///< 外部上报用量字节
        std::atomic<bool> warned{false};  ///< 越限沿去重标志
    };

    TagBudget tags_[kMaxTrackedTags];
    std::atomic<BudgetExceededCallback> callback_{nullptr};
};

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_MEMORY_TRACKER_H

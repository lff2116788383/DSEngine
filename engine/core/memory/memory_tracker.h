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

} // namespace core
} // namespace dse

#endif // DSE_CORE_MEMORY_MEMORY_TRACKER_H

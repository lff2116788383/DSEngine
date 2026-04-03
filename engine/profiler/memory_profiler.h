/**
 * @file memory_profiler.h
 * @brief 内存性能分析器，追踪内存分配、释放和使用统计
 */

#ifndef DSE_MEMORY_PROFILER_H
#define DSE_MEMORY_PROFILER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstddef>

namespace dse {
namespace profiler {

/**
 * @struct MemoryAllocation
 * @brief 单次内存分配记录
 */
struct MemoryAllocation {
    std::string tag;            ///< 分配标签/类别
    size_t size_bytes = 0;      ///< 分配大小（字节）
    void* address = nullptr;    ///< 分配地址
};

/**
 * @struct MemoryCategoryStats
 * @brief 某个类别的内存统计
 */
struct MemoryCategoryStats {
    std::string tag;                ///< 类别名称
    size_t current_bytes = 0;       ///< 当前使用量
    size_t peak_bytes = 0;          ///< 峰值使用量
    size_t total_allocated = 0;     ///< 累计分配量
    size_t total_freed = 0;         ///< 累计释放量
    int alloc_count = 0;            ///< 分配次数
    int free_count = 0;             ///< 释放次数
};

/**
 * @struct MemorySnapshot
 * @brief 内存快照
 */
struct MemorySnapshot {
    size_t total_allocated = 0;     ///< 总分配量
    size_t total_freed = 0;         ///< 总释放量
    size_t current_usage = 0;       ///< 当前使用量
    size_t peak_usage = 0;          ///< 峰值使用量
    int active_allocations = 0;     ///< 活跃分配数
};

/**
 * @class MemoryProfiler
 * @brief 内存分析器，追踪分类内存使用、检测泄漏
 */
class MemoryProfiler {
public:
    MemoryProfiler() = default;
    ~MemoryProfiler() = default;

    /**
     * @brief 记录一次内存分配
     * @param tag 分配类别标签
     * @param size_bytes 分配大小
     * @param address 分配地址（可选，用于泄漏检测）
     */
    void RecordAlloc(const std::string& tag, size_t size_bytes, void* address = nullptr);

    /**
     * @brief 记录一次内存释放
     * @param tag 释放类别标签
     * @param size_bytes 释放大小
     * @param address 释放地址（可选）
     */
    void RecordFree(const std::string& tag, size_t size_bytes, void* address = nullptr);

    /**
     * @brief 获取当前内存快照
     * @return 内存快照
     */
    MemorySnapshot GetSnapshot() const;

    /**
     * @brief 获取所有类别的内存统计
     * @return 类别统计映射表
     */
    const std::unordered_map<std::string, MemoryCategoryStats>& GetCategoryStats() const { return category_stats_; }

    /**
     * @brief 检测可能的内存泄漏（活跃分配数 > 释放数的类别）
     * @return 可能泄漏的类别列表
     */
    std::vector<std::string> DetectLeaks() const;

    /**
     * @brief 重置所有统计
     */
    void Reset();

    /**
     * @brief 导出为 CSV
     */
    std::string ExportCSV() const;

private:
    std::unordered_map<std::string, MemoryCategoryStats> category_stats_;
    size_t total_allocated_ = 0;
    size_t total_freed_ = 0;
    size_t current_usage_ = 0;
    size_t peak_usage_ = 0;
    int active_alloc_count_ = 0;
    mutable std::mutex mutex_;
};

} // namespace profiler
} // namespace dse

#endif

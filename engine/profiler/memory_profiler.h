#ifndef DSE_MEMORY_PROFILER_H
#define DSE_MEMORY_PROFILER_H

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace profiler {

struct MemoryAllocation {
    std::string tag;
    size_t size_bytes = 0;
    void* address = nullptr;
};

struct MemoryCategoryStats {
    std::string tag;
    size_t current_bytes = 0;
    size_t peak_bytes = 0;
    size_t total_allocated = 0;
    size_t total_freed = 0;
    int alloc_count = 0;
    int free_count = 0;
};

struct MemoryTraceEvent {
    double timestamp_us = 0.0;
    std::string tag;
    size_t size_bytes = 0;
    bool is_alloc = true;
    size_t running_total = 0;
};

struct MemorySnapshot {
    size_t total_allocated = 0;
    size_t total_freed = 0;
    size_t current_usage = 0;
    size_t peak_usage = 0;
    int active_allocations = 0;
};

class MemoryProfiler {
public:
    MemoryProfiler() = default;
    ~MemoryProfiler() = default;

    void RecordAlloc(const std::string& tag, size_t size_bytes, void* address = nullptr);
    void RecordFree(const std::string& tag, size_t size_bytes, void* address = nullptr);
    MemorySnapshot GetSnapshot() const;
    const std::unordered_map<std::string, MemoryCategoryStats>& GetCategoryStats() const { return category_stats_; }
    std::vector<std::string> DetectLeaks() const;
    void Reset();
    std::string ExportCSV() const;
    std::string ExportChromeTrace() const;

private:
    std::unordered_map<std::string, MemoryCategoryStats> category_stats_;
    size_t total_allocated_ = 0;
    size_t total_freed_ = 0;
    size_t current_usage_ = 0;
    size_t peak_usage_ = 0;
    int active_alloc_count_ = 0;
    std::vector<MemoryTraceEvent> trace_events_;
    std::chrono::high_resolution_clock::time_point origin_time_ = std::chrono::high_resolution_clock::now();
    mutable std::mutex mutex_;
};

} // namespace profiler
} // namespace dse

#endif // DSE_MEMORY_PROFILER_H

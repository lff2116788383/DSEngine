/**
 * @file memory_profiler.cpp
 * @brief 内存性能分析器实现
 */

#include "engine/profiler/memory_profiler.h"
#include <sstream>
#include <algorithm>

namespace dse {
namespace profiler {

void MemoryProfiler::RecordAlloc(const std::string& tag, size_t size_bytes, void* /*address*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& cat = category_stats_[tag];
    cat.tag = tag;
    cat.current_bytes += size_bytes;
    cat.total_allocated += size_bytes;
    cat.alloc_count++;
    cat.peak_bytes = std::max(cat.peak_bytes, cat.current_bytes);
    
    total_allocated_ += size_bytes;
    current_usage_ += size_bytes;
    active_alloc_count_++;
    peak_usage_ = std::max(peak_usage_, current_usage_);
}

void MemoryProfiler::RecordFree(const std::string& tag, size_t size_bytes, void* /*address*/) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& cat = category_stats_[tag];
    cat.tag = tag;
    cat.current_bytes = (size_bytes <= cat.current_bytes) ? (cat.current_bytes - size_bytes) : 0;
    cat.total_freed += size_bytes;
    cat.free_count++;
    
    total_freed_ += size_bytes;
    current_usage_ = (size_bytes <= current_usage_) ? (current_usage_ - size_bytes) : 0;
    active_alloc_count_ = std::max(0, active_alloc_count_ - 1);
}

MemorySnapshot MemoryProfiler::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    MemorySnapshot snap;
    snap.total_allocated = total_allocated_;
    snap.total_freed = total_freed_;
    snap.current_usage = current_usage_;
    snap.peak_usage = peak_usage_;
    snap.active_allocations = active_alloc_count_;
    return snap;
}

std::vector<std::string> MemoryProfiler::DetectLeaks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> leaks;
    for (const auto& [tag, cat] : category_stats_) {
        if (cat.alloc_count > cat.free_count && cat.current_bytes > 0) {
            leaks.push_back(tag);
        }
    }
    return leaks;
}

void MemoryProfiler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    category_stats_.clear();
    total_allocated_ = 0;
    total_freed_ = 0;
    current_usage_ = 0;
    peak_usage_ = 0;
    active_alloc_count_ = 0;
}

std::string MemoryProfiler::ExportCSV() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "Tag,CurrentBytes,PeakBytes,TotalAllocated,TotalFreed,AllocCount,FreeCount\n";
    for (const auto& [tag, cat] : category_stats_) {
        oss << cat.tag << ","
            << cat.current_bytes << ","
            << cat.peak_bytes << ","
            << cat.total_allocated << ","
            << cat.total_freed << ","
            << cat.alloc_count << ","
            << cat.free_count << "\n";
    }
    return oss.str();
}

} // namespace profiler
} // namespace dse

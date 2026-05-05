/**
 * @file memory_profiler.cpp
 * @brief 内存性能分析器实现
 */

#include "engine/profiler/memory_profiler.h"
#include <sstream>
#include <iomanip>
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

    MemoryTraceEvent evt;
    evt.timestamp_us = std::chrono::duration<double, std::micro>(
        std::chrono::high_resolution_clock::now() - origin_time_
    ).count();
    evt.tag = tag;
    evt.size_bytes = size_bytes;
    evt.is_alloc = true;
    evt.running_total = current_usage_;
    trace_events_.push_back(evt);
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

    MemoryTraceEvent evt;
    evt.timestamp_us = std::chrono::duration<double, std::micro>(
        std::chrono::high_resolution_clock::now() - origin_time_
    ).count();
    evt.tag = tag;
    evt.size_bytes = size_bytes;
    evt.is_alloc = false;
    evt.running_total = current_usage_;
    trace_events_.push_back(evt);
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
    trace_events_.clear();
    origin_time_ = std::chrono::high_resolution_clock::now();
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

std::string MemoryProfiler::ExportChromeTrace() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "[\n";
    bool first = true;
    for (const auto& evt : trace_events_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "{\"name\":\"" << (evt.is_alloc ? "alloc" : "free") << ":" << evt.tag << "\""
            << ",\"cat\":\"memory\""
            << ",\"ph\":\"i\""
            << ",\"ts\":" << std::fixed << std::setprecision(1) << evt.timestamp_us
            << ",\"pid\":1"
            << ",\"tid\":1"
            << ",\"s\":\"g\""
            << ",\"args\":{\"size\":" << evt.size_bytes << "}}";
        oss << ",\n{\"name\":\"memory_usage\""
            << ",\"cat\":\"memory\""
            << ",\"ph\":\"C\""
            << ",\"ts\":" << std::fixed << std::setprecision(1) << evt.timestamp_us
            << ",\"pid\":1"
            << ",\"tid\":1"
            << ",\"args\":{\"bytes\":" << evt.running_total << "}}";
    }
    oss << "\n]";
    return oss.str();
}

} // namespace profiler
} // namespace dse

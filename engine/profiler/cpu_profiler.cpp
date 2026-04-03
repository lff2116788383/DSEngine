/**
 * @file cpu_profiler.cpp
 * @brief CPU 性能分析器实现
 */

#include "engine/profiler/cpu_profiler.h"
#include <sstream>
#include <iomanip>

namespace dse {
namespace profiler {

void CPUProfiler::BeginSample(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    ActiveSample sample;
    sample.name = name;
    sample.start_time = std::chrono::high_resolution_clock::now();
    sample.depth = static_cast<int>(sample_stack_.size());
    sample_stack_.push_back(sample);
}

void CPUProfiler::EndSample() {
    auto end_time = std::chrono::high_resolution_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (sample_stack_.empty()) return;
    
    auto& active = sample_stack_.back();
    double duration_ms = std::chrono::duration<double, std::milli>(
        end_time - active.start_time
    ).count();
    
    // Record sample
    ProfileSample sample;
    sample.name = active.name;
    sample.duration_ms = duration_ms;
    sample.depth = active.depth;
    current_frame_samples_.push_back(sample);
    
    // Update stats
    auto& stat = stats_[active.name];
    stat.name = active.name;
    stat.total_ms += duration_ms;
    stat.call_count++;
    stat.min_ms = std::min(stat.min_ms, duration_ms);
    stat.max_ms = std::max(stat.max_ms, duration_ms);
    stat.avg_ms = stat.total_ms / stat.call_count;
    
    sample_stack_.pop_back();
}

void CPUProfiler::BeginFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    frame_start_ = std::chrono::high_resolution_clock::now();
    current_frame_samples_.clear();
}

void CPUProfiler::EndFrame() {
    auto end_time = std::chrono::high_resolution_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    
    double frame_ms = std::chrono::duration<double, std::milli>(
        end_time - frame_start_
    ).count();
    
    frame_stats_.frame_time_ms = frame_ms;
    frame_stats_.frame_count++;
    total_frame_time_ms_ += frame_ms;
    frame_stats_.avg_frame_time_ms = total_frame_time_ms_ / frame_stats_.frame_count;
    frame_stats_.min_frame_time_ms = std::min(frame_stats_.min_frame_time_ms, frame_ms);
    frame_stats_.max_frame_time_ms = std::max(frame_stats_.max_frame_time_ms, frame_ms);
    frame_stats_.fps = (frame_ms > 0.0) ? (1000.0 / frame_ms) : 0.0;
    frame_stats_.avg_fps = (frame_stats_.avg_frame_time_ms > 0.0) 
        ? (1000.0 / frame_stats_.avg_frame_time_ms) : 0.0;
}

void CPUProfiler::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    sample_stack_.clear();
    current_frame_samples_.clear();
    stats_.clear();
    frame_stats_ = FrameStats{};
    total_frame_time_ms_ = 0.0;
}

std::string CPUProfiler::ExportCSV() const {
    std::ostringstream oss;
    oss << "Name,TotalMs,AvgMs,MinMs,MaxMs,CallCount\n";
    for (const auto& [name, stat] : stats_) {
        oss << stat.name << ","
            << std::fixed << std::setprecision(3)
            << stat.total_ms << ","
            << stat.avg_ms << ","
            << stat.min_ms << ","
            << stat.max_ms << ","
            << stat.call_count << "\n";
    }
    return oss.str();
}

std::string CPUProfiler::ExportJSON() const {
    std::ostringstream oss;
    oss << "{\n  \"frame_stats\": {\n"
        << "    \"frame_count\": " << frame_stats_.frame_count << ",\n"
        << "    \"avg_frame_time_ms\": " << std::fixed << std::setprecision(3) << frame_stats_.avg_frame_time_ms << ",\n"
        << "    \"min_frame_time_ms\": " << frame_stats_.min_frame_time_ms << ",\n"
        << "    \"max_frame_time_ms\": " << frame_stats_.max_frame_time_ms << ",\n"
        << "    \"avg_fps\": " << frame_stats_.avg_fps << "\n"
        << "  },\n  \"samples\": [\n";
    
    bool first = true;
    for (const auto& [name, stat] : stats_) {
        if (!first) oss << ",\n";
        first = false;
        oss << "    {\"name\": \"" << stat.name << "\""
            << ", \"total_ms\": " << stat.total_ms
            << ", \"avg_ms\": " << stat.avg_ms
            << ", \"min_ms\": " << stat.min_ms
            << ", \"max_ms\": " << stat.max_ms
            << ", \"call_count\": " << stat.call_count
            << "}";
    }
    oss << "\n  ]\n}";
    return oss.str();
}

} // namespace profiler
} // namespace dse

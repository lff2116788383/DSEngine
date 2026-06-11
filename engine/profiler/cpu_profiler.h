#ifndef DSE_CPU_PROFILER_H
#define DSE_CPU_PROFILER_H

#include "engine/core/dse_export.h"
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace profiler {

struct ProfileSample {
    std::string name;
    double duration_ms = 0.0;
    double timestamp_us = 0.0;
    int depth = 0;
};

struct ProfileStats {
    std::string name;
    double total_ms = 0.0;
    double min_ms = 1e9;
    double max_ms = 0.0;
    double avg_ms = 0.0;
    int call_count = 0;
};

struct FrameStats {
    double frame_time_ms = 0.0;
    double avg_frame_time_ms = 0.0;
    double min_frame_time_ms = 1e9;
    double max_frame_time_ms = 0.0;
    double fps = 0.0;
    double avg_fps = 0.0;
    int frame_count = 0;
};

class DSE_EXPORT CPUProfiler {
public:
    CPUProfiler() = default;
    ~CPUProfiler() = default;

    void BeginSample(const std::string& name);
    void EndSample();
    void BeginFrame();
    void EndFrame();
    void Reset();

    const std::unordered_map<std::string, ProfileStats>& GetStats() const { return stats_; }
    const FrameStats& GetFrameStats() const { return frame_stats_; }
    const std::vector<ProfileSample>& GetCurrentFrameSamples() const { return current_frame_samples_; }

    std::string ExportCSV() const;
    std::string ExportJSON() const;
    std::string ExportChromeTrace() const;
    const std::vector<ProfileSample>& GetAllSamples() const { return trace_samples_; }

private:
    struct ActiveSample {
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        int depth = 0;
    };

    std::vector<ActiveSample> sample_stack_;
    std::vector<ProfileSample> current_frame_samples_;
    std::unordered_map<std::string, ProfileStats> stats_;
    FrameStats frame_stats_;
    std::chrono::high_resolution_clock::time_point frame_start_;
    double total_frame_time_ms_ = 0.0;
    std::vector<ProfileSample> trace_samples_;
    std::chrono::high_resolution_clock::time_point origin_time_ = std::chrono::high_resolution_clock::now();
    std::mutex mutex_;
};

class ScopedCPUProfile {
public:
    ScopedCPUProfile(CPUProfiler& profiler, const std::string& name)
        : profiler_(profiler) {
        profiler_.BeginSample(name);
    }

    ~ScopedCPUProfile() {
        profiler_.EndSample();
    }

private:
    CPUProfiler& profiler_;
};

#define DSE_PROFILE_SCOPE(profiler_instance) \
    dse::profiler::ScopedCPUProfile _dse_profile_##__LINE__(profiler_instance, __FUNCTION__)

} // namespace profiler
} // namespace dse

#endif // DSE_CPU_PROFILER_H

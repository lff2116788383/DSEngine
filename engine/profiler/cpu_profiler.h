/**
 * @file cpu_profiler.h
 * @brief CPU 性能分析器，提供函数级别的耗时追踪和帧时间统计
 */

#ifndef DSE_CPU_PROFILER_H
#define DSE_CPU_PROFILER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace dse {
namespace profiler {

/**
 * @struct ProfileSample
 * @brief 单次性能采样记录
 */
struct ProfileSample {
    std::string name;           ///< 采样名称
    double duration_ms = 0.0;   ///< 持续时间（毫秒）
    int depth = 0;              ///< 调用深度
};

/**
 * @struct ProfileStats
 * @brief 某个采样点的累计统计数据
 */
struct ProfileStats {
    std::string name;           ///< 采样名称
    double total_ms = 0.0;      ///< 累计总时间（毫秒）
    double min_ms = 1e9;        ///< 最小耗时
    double max_ms = 0.0;        ///< 最大耗时
    double avg_ms = 0.0;        ///< 平均耗时
    int call_count = 0;         ///< 调用次数
};

/**
 * @struct FrameStats
 * @brief 帧时间统计
 */
struct FrameStats {
    double frame_time_ms = 0.0;     ///< 当前帧耗时
    double avg_frame_time_ms = 0.0; ///< 平均帧耗时
    double min_frame_time_ms = 1e9; ///< 最小帧耗时
    double max_frame_time_ms = 0.0; ///< 最大帧耗时
    double fps = 0.0;               ///< 当前 FPS
    double avg_fps = 0.0;           ///< 平均 FPS
    int frame_count = 0;            ///< 总帧数
};

/**
 * @class CPUProfiler
 * @brief CPU 性能分析器，支持嵌套采样、帧时间追踪和统计汇总
 */
class CPUProfiler {
public:
    CPUProfiler() = default;
    ~CPUProfiler() = default;

    /**
     * @brief 开始一个命名采样区间
     * @param name 采样名称
     */
    void BeginSample(const std::string& name);

    /**
     * @brief 结束当前采样区间
     */
    void EndSample();

    /**
     * @brief 标记帧开始
     */
    void BeginFrame();

    /**
     * @brief 标记帧结束，更新帧统计
     */
    void EndFrame();

    /**
     * @brief 获取所有采样点的累计统计
     * @return 统计数据映射表
     */
    const std::unordered_map<std::string, ProfileStats>& GetStats() const { return stats_; }

    /**
     * @brief 获取帧时间统计
     * @return 帧统计数据
     */
    const FrameStats& GetFrameStats() const { return frame_stats_; }

    /**
     * @brief 获取当前帧的所有采样记录
     * @return 采样记录列表
     */
    const std::vector<ProfileSample>& GetCurrentFrameSamples() const { return current_frame_samples_; }

    /**
     * @brief 重置所有统计数据
     */
    void Reset();

    /**
     * @brief 导出统计数据为 CSV 格式字符串
     * @return CSV 格式的统计数据
     */
    std::string ExportCSV() const;

    /**
     * @brief 导出统计数据为 JSON 格式字符串
     * @return JSON 格式的统计数据
     */
    std::string ExportJSON() const;

private:
    struct ActiveSample {
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        int depth;
    };

    std::vector<ActiveSample> sample_stack_;                        ///< 活跃采样栈
    std::vector<ProfileSample> current_frame_samples_;              ///< 当前帧采样
    std::unordered_map<std::string, ProfileStats> stats_;           ///< 累计统计
    FrameStats frame_stats_;                                        ///< 帧统计
    std::chrono::high_resolution_clock::time_point frame_start_;    ///< 帧开始时间
    double total_frame_time_ms_ = 0.0;                              ///< 累计帧时间
    std::mutex mutex_;                                              ///< 线程安全锁
};

/**
 * @class ScopedCPUProfile
 * @brief RAII 风格的 CPU 采样辅助类
 * @example
 * {
 *     ScopedCPUProfile scope(profiler, "Physics::Update");
 *     // ... physics code ...
 * }
 */
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

/// 便捷宏：自动使用函数名作为采样名
#define DSE_PROFILE_SCOPE(profiler) \
    dse::profiler::ScopedCPUProfile _dse_profile_##__LINE__(profiler, __FUNCTION__)

} // namespace profiler
} // namespace dse

#endif

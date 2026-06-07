/**
 * @file gl_gpu_timer.h
 * @brief OpenGL GPU Timestamp Query 实现
 *
 * 使用 GL_TIMESTAMP + glQueryCounter 双缓冲策略：
 * - 每帧写入 query pair (begin/end)
 * - 读取上一帧 query 结果
 */

#ifndef DSE_RENDER_GL_GPU_TIMER_H
#define DSE_RENDER_GL_GPU_TIMER_H

#include "engine/render/rhi/rhi_gpu_timer.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace dse {
namespace render {

class GLGpuTimer {
public:
    GLGpuTimer() = default;
    ~GLGpuTimer();

    /// 初始化（需要 GL 上下文）
    void Init();
    void Shutdown();

    bool SupportsGpuTimer() const { return initialized_; }
    GpuTimerId GetOrCreateGpuTimer(const std::string& name);
    void BeginGpuTimer(GpuTimerId id);
    void EndGpuTimer(GpuTimerId id);
    float GetGpuTimerResultMs(GpuTimerId id) const;
    void ResetGpuTimers();
    void ResolveGpuTimers();
    std::vector<IRhiGpuTimer::GpuTimerEntry> GetAllGpuTimerResults() const;

private:
    static constexpr int kFrameCount = 2;

    struct TimerSlot {
        std::string name;
        unsigned int queries[kFrameCount][2] = {};  // [frame][begin/end]
        float last_result_ms = -1.0f;
    };

    std::vector<TimerSlot> slots_;
    std::unordered_map<std::string, GpuTimerId> name_to_id_;

    int write_frame_ = 0;   // 当前写入帧 index
    int read_frame_ = 1;    // 当前读取帧 index（上一帧）
    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_GPU_TIMER_H

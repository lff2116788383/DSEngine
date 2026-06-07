/**
 * @file rhi_gpu_timer.h
 * @brief GPU Timestamp Query 抽象接口
 *
 * 提供跨后端统一的 GPU 耗时测量能力：
 * - BeginGpuTimer / EndGpuTimer 配对使用
 * - GetGpuTimerResultMs 获取上一帧的查询结果（避免 pipeline stall）
 *
 * 使用模式：
 *   auto id = device->BeginGpuTimer("ShadowPass");
 *   // ... 渲染命令 ...
 *   device->EndGpuTimer(id);
 *   // 下一帧：
 *   float ms = device->GetGpuTimerResultMs(id);
 */

#ifndef DSE_RHI_GPU_TIMER_H
#define DSE_RHI_GPU_TIMER_H

#include <string>
#include <cstdint>
#include <vector>

namespace dse {
namespace render {

/// GPU Timer 句柄（不透明 ID）
using GpuTimerId = uint32_t;

/// 无效的 GPU Timer 句柄
static constexpr GpuTimerId kInvalidGpuTimerId = 0;

/**
 * @class IRhiGpuTimer
 * @brief GPU 时间戳查询接口
 *
 * 后端实现要点：
 * - OpenGL: glQueryCounter + GL_TIMESTAMP
 * - Vulkan: vkCmdWriteTimestamp + VkQueryPool
 * - D3D11:  ID3D11Query(D3D11_QUERY_TIMESTAMP) + DISJOINT
 *
 * 双缓冲策略：本帧写入 query，读取上一帧结果，避免 GPU stall。
 */
class IRhiGpuTimer {
public:
    virtual ~IRhiGpuTimer() = default;

    /// 是否支持 GPU 时间戳查询
    virtual bool SupportsGpuTimer() const { return false; }

    /// 创建或获取命名 timer 的 ID（首次调用时分配资源）
    virtual GpuTimerId GetOrCreateGpuTimer(const std::string& name) { (void)name; return kInvalidGpuTimerId; }

    /// 标记 GPU 时间戳起点
    virtual void BeginGpuTimer(GpuTimerId id) { (void)id; }

    /// 标记 GPU 时间戳终点
    virtual void EndGpuTimer(GpuTimerId id) { (void)id; }

    /// 获取上一帧 timer 的耗时（毫秒），如尚未就绪返回 -1.0f
    virtual float GetGpuTimerResultMs(GpuTimerId id) const { (void)id; return -1.0f; }

    /// 帧开始时重置本帧 query 状态（由 BeginFrame 调用）
    virtual void ResetGpuTimers() {}

    /// 帧结束时收集本帧 query 结果（由 EndFrame 调用）
    virtual void ResolveGpuTimers() {}

    /// 获取所有 timer 的名称和上一帧耗时（供 Profiler UI 使用）
    struct GpuTimerEntry {
        std::string name;
        float ms = -1.0f;
    };
    virtual std::vector<GpuTimerEntry> GetAllGpuTimerResults() const { return {}; }
};

} // namespace render
} // namespace dse

#endif // DSE_RHI_GPU_TIMER_H

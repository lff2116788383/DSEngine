/**
 * @file vulkan_gpu_timer.h
 * @brief Vulkan GPU Timestamp Query 实现
 *
 * 使用 VkQueryPool + vkCmdWriteTimestamp 双缓冲策略：
 * - 每帧在命令缓冲中写入 timestamp
 * - 帧结束后从上一帧的 query pool 读取结果
 */

#ifndef DSE_RENDER_VULKAN_GPU_TIMER_H
#define DSE_RENDER_VULKAN_GPU_TIMER_H

#include "engine/render/rhi/rhi_gpu_timer.h"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace dse {
namespace render {

class VulkanContext;

class VulkanGpuTimer {
public:
    VulkanGpuTimer() = default;
    ~VulkanGpuTimer();

    /// 初始化（需要 Vulkan device）
    void Init(VulkanContext* context);
    void Shutdown();

    bool SupportsGpuTimer() const { return initialized_; }
    GpuTimerId GetOrCreateGpuTimer(const std::string& name);
    void BeginGpuTimer(GpuTimerId id, VkCommandBuffer cmd);
    void EndGpuTimer(GpuTimerId id, VkCommandBuffer cmd);
    float GetGpuTimerResultMs(GpuTimerId id) const;
    void ResetGpuTimers();
    void FlushPendingQueryPoolReset(VkCommandBuffer cmd);
    void ResolveGpuTimers();
    std::vector<IRhiGpuTimer::GpuTimerEntry> GetAllGpuTimerResults() const;

private:
    static constexpr int kFrameCount = 2;
    static constexpr uint32_t kMaxTimers = 64;  // 最多 64 个 timer slot

    struct TimerSlot {
        std::string name;
        float last_result_ms = -1.0f;
    };

    VulkanContext* context_ = nullptr;
    VkQueryPool query_pools_[kFrameCount] = {};  // 每帧一个 pool
    std::vector<TimerSlot> slots_;
    std::unordered_map<std::string, GpuTimerId> name_to_id_;

    int write_frame_ = 0;
    int read_frame_ = 1;
    float timestamp_period_ = 0.0f;  // 纳秒/tick
    bool initialized_ = false;
    bool pending_pool_reset_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_GPU_TIMER_H

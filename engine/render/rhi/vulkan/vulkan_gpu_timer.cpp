/**
 * @file vulkan_gpu_timer.cpp
 * @brief Vulkan GPU Timestamp Query 实现
 */

#include "engine/render/rhi/vulkan/vulkan_gpu_timer.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

VulkanGpuTimer::~VulkanGpuTimer() {
    Shutdown();
}

void VulkanGpuTimer::Init(VulkanContext* context) {
    if (initialized_ || !context) return;
    context_ = context;

    VkDevice device = context_->device();
    if (device == VK_NULL_HANDLE) return;

    // 获取 timestamp 精度
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(context_->physical_device(), &props);
    timestamp_period_ = props.limits.timestampPeriod;  // 纳秒/tick

    if (timestamp_period_ == 0.0f) {
        DEBUG_LOG_WARN("[VulkanGpuTimer] timestampPeriod is 0, GPU timing not supported");
        return;
    }

    // 检查 graphics queue 是否支持 timestamp
    VkQueueFamilyProperties queue_props;
    uint32_t count = 1;
    vkGetPhysicalDeviceQueueFamilyProperties(context_->physical_device(), &count, &queue_props);
    if (queue_props.timestampValidBits == 0) {
        DEBUG_LOG_WARN("[VulkanGpuTimer] Graphics queue does not support timestamps");
        return;
    }

    // 创建双缓冲 query pool（每个 timer 使用 2 个 query: begin + end）
    for (int f = 0; f < kFrameCount; ++f) {
        VkQueryPoolCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        ci.queryType = VK_QUERY_TYPE_TIMESTAMP;
        ci.queryCount = kMaxTimers * 2;  // begin + end per timer
        if (vkCreateQueryPool(device, &ci, nullptr, &query_pools_[f]) != VK_SUCCESS) {
            DEBUG_LOG_WARN("[VulkanGpuTimer] Failed to create query pool");
            return;
        }
    }

    initialized_ = true;
    write_frame_ = 0;
    read_frame_ = 1;
    DEBUG_LOG_INFO("[VulkanGpuTimer] Initialized (period={:.2f} ns/tick)", timestamp_period_);
}

void VulkanGpuTimer::Shutdown() {
    if (!initialized_) return;
    VkDevice device = context_ ? context_->device() : VK_NULL_HANDLE;
    if (device != VK_NULL_HANDLE) {
        for (int f = 0; f < kFrameCount; ++f) {
            if (query_pools_[f] != VK_NULL_HANDLE) {
                vkDestroyQueryPool(device, query_pools_[f], nullptr);
                query_pools_[f] = VK_NULL_HANDLE;
            }
        }
    }
    slots_.clear();
    name_to_id_.clear();
    initialized_ = false;
}

GpuTimerId VulkanGpuTimer::GetOrCreateGpuTimer(const std::string& name) {
    if (!initialized_) return kInvalidGpuTimerId;

    auto it = name_to_id_.find(name);
    if (it != name_to_id_.end()) return it->second;

    if (slots_.size() >= kMaxTimers) {
        DEBUG_LOG_WARN("[VulkanGpuTimer] Max timer count reached ({})", kMaxTimers);
        return kInvalidGpuTimerId;
    }

    GpuTimerId id = static_cast<GpuTimerId>(slots_.size() + 1);
    TimerSlot slot;
    slot.name = name;
    slots_.push_back(std::move(slot));
    name_to_id_[name] = id;
    return id;
}

void VulkanGpuTimer::BeginGpuTimer(GpuTimerId id, VkCommandBuffer cmd) {
    if (!initialized_ || id == kInvalidGpuTimerId || cmd == VK_NULL_HANDLE) return;
    uint32_t idx = id - 1;
    if (idx >= slots_.size()) return;
    uint32_t query_index = idx * 2;  // begin query
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        query_pools_[write_frame_], query_index);
}

void VulkanGpuTimer::EndGpuTimer(GpuTimerId id, VkCommandBuffer cmd) {
    if (!initialized_ || id == kInvalidGpuTimerId || cmd == VK_NULL_HANDLE) return;
    uint32_t idx = id - 1;
    if (idx >= slots_.size()) return;
    uint32_t query_index = idx * 2 + 1;  // end query
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        query_pools_[write_frame_], query_index);
}

float VulkanGpuTimer::GetGpuTimerResultMs(GpuTimerId id) const {
    if (!initialized_ || id == kInvalidGpuTimerId) return -1.0f;
    uint32_t idx = id - 1;
    if (idx >= slots_.size()) return -1.0f;
    return slots_[idx].last_result_ms;
}

void VulkanGpuTimer::ResetGpuTimers() {
    if (!initialized_) return;
    write_frame_ = (write_frame_ + 1) % kFrameCount;
    read_frame_ = (read_frame_ + 1) % kFrameCount;
    pending_pool_reset_ = true;
}

void VulkanGpuTimer::FlushPendingQueryPoolReset(VkCommandBuffer cmd) {
    if (!pending_pool_reset_ || cmd == VK_NULL_HANDLE) return;
    uint32_t query_count = static_cast<uint32_t>(slots_.size()) * 2;
    if (query_count > 0) {
        vkCmdResetQueryPool(cmd, query_pools_[write_frame_], 0, query_count);
    }
    pending_pool_reset_ = false;
}

void VulkanGpuTimer::ResolveGpuTimers() {
    if (!initialized_ || slots_.empty()) return;

    VkDevice device = context_->device();
    uint32_t query_count = static_cast<uint32_t>(slots_.size()) * 2;

    std::vector<uint64_t> results(query_count);
    VkResult vr = vkGetQueryPoolResults(
        device, query_pools_[read_frame_], 0, query_count,
        query_count * sizeof(uint64_t), results.data(),
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

    if (vr != VK_SUCCESS && vr != VK_NOT_READY) return;

    for (size_t i = 0; i < slots_.size(); ++i) {
        uint64_t ts_begin = results[i * 2];
        uint64_t ts_end = results[i * 2 + 1];
        if (ts_end >= ts_begin && ts_end != 0) {
            // ticks → ns → ms
            float ns = static_cast<float>(ts_end - ts_begin) * timestamp_period_;
            slots_[i].last_result_ms = ns / 1'000'000.0f;
        } else {
            slots_[i].last_result_ms = -1.0f;
        }
    }
}

std::vector<IRhiGpuTimer::GpuTimerEntry> VulkanGpuTimer::GetAllGpuTimerResults() const {
    std::vector<IRhiGpuTimer::GpuTimerEntry> results;
    results.reserve(slots_.size());
    for (const auto& slot : slots_) {
        results.push_back({slot.name, slot.last_result_ms});
    }
    return results;
}

} // namespace render
} // namespace dse

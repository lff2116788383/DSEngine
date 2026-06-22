/**
 * @file per_in_flight_buffer.h
 * @brief 「每在飞帧缓冲」helper — 为每帧覆写的动态缓冲按引擎在飞帧数 N 缓冲。
 *
 * 背景（RHI_ABSTRACTION_BOUNDARY §8.2 D9）：引擎 Vulkan 后端 2 帧在飞
 * （MAX_FRAMES_IN_FLIGHT=2），高层渲染器若每帧 UpdateGpuBuffer 覆写同一动态
 * VBO/UBO，则帧 N+1 可能在帧 N 仍被 GPU 读取时覆写 → 竞争。mesh 执行器内部已用
 * MAX_FRAMES 双缓冲规避，本 helper 把该模式抽成后端无关小工具（基于 BufferHandle +
 * RhiDevice），供 SpriteBatchRenderer 等高层渲染器复用。
 *
 * 语义：持有 N=device.FramesInFlight() 个物理缓冲槽位；每帧仅访问
 * device.CurrentFrameSlot() 指向的当前槽位——其 fence 已在 AcquireNextImage 等待，
 * 故安全覆写/重建；其余槽位属其它在飞帧，绝不触碰。GL/DX11（N=1）退化为单缓冲，
 * 行为与改造前一致。
 */

#ifndef DSE_RENDER_RHI_PER_IN_FLIGHT_BUFFER_H
#define DSE_RENDER_RHI_PER_IN_FLIGHT_BUFFER_H

#include <cstddef>
#include <vector>

#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class RhiDevice;

/**
 * @class PerInFlightBuffer
 * @brief 单一逻辑动态缓冲的 N 槽位（每在飞帧）封装。
 *
 * 用法：每帧绘制前调用 Acquire 取当前槽位句柄，再对其 UpdateGpuBuffer / Bind*。
 * 容量按需增长（仅(重)建当前槽位，不动其它在飞槽位）。析构/重置时 Shutdown 释放全部槽位。
 */
class PerInFlightBuffer {
public:
    /// 取当前在飞槽位的缓冲，确保其容量 >= byte_size、用途 == usage（恒 is_dynamic）。
    /// 仅按需(重)建当前槽位；其余槽位（属其它在飞帧）不动。返回句柄供 UpdateGpuBuffer/Bind* 用。
    BufferHandle Acquire(RhiDevice& device, size_t byte_size, GpuBufferUsage usage);

    /// 释放全部槽位（须在 device 仍有效、且 GPU 不再使用这些缓冲时调用）。
    void Shutdown(RhiDevice& device);

    /// 是否已分配过任一槽位。
    bool valid() const { return !slots_.empty(); }

private:
    struct Slot {
        BufferHandle handle;
        size_t capacity = 0;  ///< 当前槽位缓冲的字节容量（只增不减，避免无谓重建）
    };
    std::vector<Slot> slots_;  ///< 一槽位对应一在飞帧
};

}  // namespace render
}  // namespace dse

#endif  // DSE_RENDER_RHI_PER_IN_FLIGHT_BUFFER_H

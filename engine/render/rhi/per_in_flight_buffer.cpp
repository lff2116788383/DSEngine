/**
 * @file per_in_flight_buffer.cpp
 * @brief PerInFlightBuffer 实现 — 见头文件说明。
 */

#include "engine/render/rhi/per_in_flight_buffer.h"

#include "engine/render/rhi/rhi_device.h"

namespace dse {
namespace render {

BufferHandle PerInFlightBuffer::Acquire(RhiDevice& device, size_t byte_size, GpuBufferUsage usage) {
    if (byte_size == 0) byte_size = 1;  // 零字节缓冲非法

    uint32_t n = device.FramesInFlight();
    if (n == 0) n = 1;
    if (slots_.size() < n) slots_.resize(n);

    uint32_t idx = device.CurrentFrameSlot();
    if (idx >= slots_.size()) idx = 0;  // 防御：槽位号越界时退化到 0

    Slot& s = slots_[idx];
    if (!s.handle || byte_size > s.capacity) {
        // 仅(重)建当前槽位——其 fence 已等待，安全；其余在飞槽位不动。
        if (s.handle) device.DeleteGpuBuffer(s.handle);
        GpuBufferDesc desc;
        desc.size = byte_size;
        desc.usage = usage;
        desc.is_dynamic = true;
        s.handle = device.CreateGpuBuffer(desc, nullptr);
        s.capacity = byte_size;
    }
    return s.handle;
}

void PerInFlightBuffer::Shutdown(RhiDevice& device) {
    for (Slot& s : slots_) {
        if (s.handle) device.DeleteGpuBuffer(s.handle);
    }
    slots_.clear();
}

}  // namespace render
}  // namespace dse

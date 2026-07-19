/**
 * @file rhi_gpu_buffer.h
 * @brief 统一 GPU Buffer 描述符和 Usage Flags
 *
 * Phase 2: 替代分散的 CreateBuffer/CreateSSBO/CreateIndirectBuffer，
 * 通过 GpuBufferUsage 标志位区分用途。
 */

#ifndef DSE_RHI_GPU_BUFFER_H
#define DSE_RHI_GPU_BUFFER_H

#include <cstdint>
#include <cstddef>
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

// ============================================================
// GpuBufferUsage — 按位组合的 buffer 用途标志
// ============================================================

enum class GpuBufferUsage : uint32_t {
    kNone           = 0,
    kVertex         = 1 << 0,
    kIndex          = 1 << 1,
    kStorage        = 1 << 2,  // SSBO / StructuredBuffer
    kIndirect       = 1 << 3,
    kTransferSrc    = 1 << 4,
    kTransferDst    = 1 << 5,
    kUniform        = 1 << 6,  // UBO / Constant Buffer（预留）
};

inline GpuBufferUsage operator|(GpuBufferUsage a, GpuBufferUsage b) {
    return static_cast<GpuBufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline GpuBufferUsage operator&(GpuBufferUsage a, GpuBufferUsage b) {
    return static_cast<GpuBufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline GpuBufferUsage& operator|=(GpuBufferUsage& a, GpuBufferUsage b) {
    a = a | b; return a;
}
inline bool has(GpuBufferUsage flags, GpuBufferUsage bit) {
    return (flags & bit) != GpuBufferUsage::kNone;
}

// ============================================================
// GpuBufferDesc — 创建 GPU Buffer 的描述符
// ============================================================

struct GpuBufferDesc {
    size_t size = 0;
    GpuBufferUsage usage = GpuBufferUsage::kVertex;
    bool is_dynamic = false;       // HOST_VISIBLE，允许 CPU 每帧写入
    const char* debug_name = nullptr;
    /// 该缓冲由 per-in-flight ring 管理（PerInFlightBuffer）：每帧只写当前在飞槽位、
    /// 其 fence 已在 AcquireNextImage 等待，故 host 写入无需跨帧总闸同步
    /// （Vulkan SyncHostWriteWithGpu）。仅 Vulkan 消费；GL/DX11（N=1）忽略。
    bool per_in_flight = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RHI_GPU_BUFFER_H

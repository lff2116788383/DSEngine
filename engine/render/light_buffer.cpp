/**
 * @file light_buffer.cpp
 * @brief LightBuffer 实现 - 每帧光源收集 + SSBO 上传
 */

#include "engine/render/light_buffer.h"
#include "engine/render/render_scene_view.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/ubo_types.h"
#include "engine/base/debug.h"
#include <algorithm>

namespace dse {
namespace render {

void LightBuffer::Init(RhiDevice* device) {
    device_ = device;
    if (!device_) return;

    // SSBO 现由 per-in-flight ring 惰性分配（首帧 Upload 时按需 Acquire 当前槽位），
    // 不再在 Init 预建单份持久缓冲。仅预留 CPU 容器容量。
    point_lights_.reserve(64);
    spot_lights_.reserve(64);
}

void LightBuffer::CollectLightsFromView(const RenderSceneView& view, const glm::vec3& camera_offset) {
    point_lights_.clear();
    spot_lights_.clear();

    const bool ubo_mode = device_ && !device_->SupportsSSBO();
    const int max_point = ubo_mode ? kMaxUBOLights : kMaxClusteredPointLights;
    const int max_spot  = ubo_mode ? kMaxUBOLights : kMaxClusteredSpotLights;

    int next_point_shadow = 0;
    for (const auto& pl : view.point_lights) {
        if (static_cast<int>(point_lights_.size()) >= max_point) break;
        GPUPointLight gpu{};
        gpu.color     = pl.color;
        gpu.intensity = pl.intensity;
        gpu.position  = pl.position - camera_offset;
        gpu.radius    = pl.radius * std::max(0.1f, pl.falloff);
        if (pl.cast_shadow && next_point_shadow < 4) {
            gpu.cast_shadow  = 1;
            gpu.shadow_index = next_point_shadow++;
        } else {
            gpu.cast_shadow  = 0;
            gpu.shadow_index = -1;
        }
        point_lights_.push_back(gpu);
    }

    int next_spot_shadow = 0;
    for (const auto& sl : view.spot_lights) {
        if (static_cast<int>(spot_lights_.size()) >= max_spot) break;
        GPUSpotLight gpu{};
        gpu.color      = sl.color;
        gpu.intensity  = sl.intensity;
        gpu.position   = sl.position - camera_offset;
        gpu.radius     = sl.range * std::max(0.1f, sl.falloff);
        gpu.direction  = sl.direction;  // already world-space
        gpu.inner_cone = sl.inner_cone;
        gpu.outer_cone = sl.outer_cone;
        if (sl.cast_shadow && next_spot_shadow < 4) {
            gpu.cast_shadow  = 1;
            gpu.shadow_index = next_spot_shadow++;
        } else {
            gpu.cast_shadow  = 0;
            gpu.shadow_index = -1;
        }
        spot_lights_.push_back(gpu);
    }
}

void LightBuffer::Upload() {
    if (!device_) return;

    const int pc = static_cast<int>(point_lights_.size());
    const int sc = static_cast<int>(spot_lights_.size());

    // per-in-flight ring：写当前槽位（其 fence 已在 BeginFrame/AcquireNextImage 等待），
    // 仅(重)建当前槽位、不触发跨帧总闸。与原实现一致：每帧都写 header（count 可为 0），
    // 使绑定点始终有效缓冲。
    {
        const size_t size = sizeof(LightBufferHeader) +
                            sizeof(GPUPointLight) * static_cast<size_t>(std::max(pc, 1));
        point_light_ssbo_ = point_light_ring_.Acquire(*device_, size, GpuBufferUsage::kStorage);
        LightBufferHeader header{};
        header.count = pc;
        device_->UpdateGpuBuffer(point_light_ssbo_, 0, sizeof(header), &header);
        if (pc > 0) {
            device_->UpdateGpuBuffer(point_light_ssbo_, sizeof(header),
                                sizeof(GPUPointLight) * pc, point_lights_.data());
        }
    }

    {
        const size_t size = sizeof(LightBufferHeader) +
                            sizeof(GPUSpotLight) * static_cast<size_t>(std::max(sc, 1));
        spot_light_ssbo_ = spot_light_ring_.Acquire(*device_, size, GpuBufferUsage::kStorage);
        LightBufferHeader header{};
        header.count = sc;
        device_->UpdateGpuBuffer(spot_light_ssbo_, 0, sizeof(header), &header);
        if (sc > 0) {
            device_->UpdateGpuBuffer(spot_light_ssbo_, sizeof(header),
                                sizeof(GPUSpotLight) * sc, spot_lights_.data());
        }
    }
}

void LightBuffer::Bind() {
    if (!device_) return;
    device_->BindGpuBuffer(point_light_ssbo_, kSSBOBindingPointLights);
    device_->BindGpuBuffer(spot_light_ssbo_,  kSSBOBindingSpotLights);
}

void LightBuffer::Shutdown() {
    if (!device_) return;
    point_light_ring_.Shutdown(*device_);
    spot_light_ring_.Shutdown(*device_);
    point_light_ssbo_ = {};
    spot_light_ssbo_ = {};
    point_lights_.clear();
    spot_lights_.clear();
    device_ = nullptr;
}

} // namespace render
} // namespace dse

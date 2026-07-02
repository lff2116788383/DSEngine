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

    // 预分配初始容量（64 个光源）
    const int initial_point = 64;
    const int initial_spot  = 64;

    const size_t point_ssbo_size = sizeof(LightBufferHeader) + sizeof(GPUPointLight) * initial_point;
    const size_t spot_ssbo_size  = sizeof(LightBufferHeader) + sizeof(GPUSpotLight)  * initial_spot;

    GpuBufferDesc desc;
    desc.usage = GpuBufferUsage::kStorage;
    desc.is_dynamic = true;

    desc.size = point_ssbo_size;
    desc.debug_name = "point_light_ssbo";
    point_light_ssbo_ = device_->CreateGpuBuffer(desc, nullptr);

    desc.size = spot_ssbo_size;
    desc.debug_name = "spot_light_ssbo";
    spot_light_ssbo_  = device_->CreateGpuBuffer(desc, nullptr);

    point_light_capacity_ = initial_point;
    spot_light_capacity_  = initial_spot;

    point_lights_.reserve(initial_point);
    spot_lights_.reserve(initial_spot);
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

    // 如果当前容量不足，重新分配 SSBO
    if (pc > point_light_capacity_) {
        if (point_light_ssbo_) {
            device_->DeleteGpuBuffer(point_light_ssbo_);
        }
        point_light_capacity_ = std::max(pc, point_light_capacity_ * 2);
        const size_t new_size = sizeof(LightBufferHeader) + sizeof(GPUPointLight) * point_light_capacity_;
        GpuBufferDesc desc{new_size, GpuBufferUsage::kStorage, true, "point_light_ssbo"};
        point_light_ssbo_ = device_->CreateGpuBuffer(desc, nullptr);
    }
    if (sc > spot_light_capacity_) {
        if (spot_light_ssbo_) {
            device_->DeleteGpuBuffer(spot_light_ssbo_);
        }
        spot_light_capacity_ = std::max(sc, spot_light_capacity_ * 2);
        const size_t new_size = sizeof(LightBufferHeader) + sizeof(GPUSpotLight) * spot_light_capacity_;
        GpuBufferDesc desc{new_size, GpuBufferUsage::kStorage, true, "spot_light_ssbo"};
        spot_light_ssbo_ = device_->CreateGpuBuffer(desc, nullptr);
    }

    // 上传点光源 SSBO
    if (point_light_ssbo_) {
        LightBufferHeader header{};
        header.count = pc;
        device_->UpdateGpuBuffer(point_light_ssbo_, 0, sizeof(header), &header);
        if (pc > 0) {
            device_->UpdateGpuBuffer(point_light_ssbo_, sizeof(header),
                                sizeof(GPUPointLight) * pc, point_lights_.data());
        }
    }

    // 上传聚光灯 SSBO
    if (spot_light_ssbo_) {
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
    if (point_light_ssbo_) {
        device_->DeleteGpuBuffer(point_light_ssbo_);
        point_light_ssbo_ = {};
    }
    if (spot_light_ssbo_) {
        device_->DeleteGpuBuffer(spot_light_ssbo_);
        spot_light_ssbo_ = {};
    }
    point_lights_.clear();
    spot_lights_.clear();
    point_light_capacity_ = 0;
    spot_light_capacity_  = 0;
    device_ = nullptr;
}

} // namespace render
} // namespace dse

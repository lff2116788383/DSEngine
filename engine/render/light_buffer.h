/**
 * @file light_buffer.h
 * @brief Clustered Forward+ 光源缓冲 - 每帧收集全场景光源并上传 SSBO
 *
 * 设计：
 *   - 每帧从 ECS 收集所有点光源 / 聚光灯，存入 CPU 端 vector
 *   - 通过 RhiDevice::CreateSSBO / UpdateSSBO 上传到 GPU SSBO
 *   - Phase 1.3 (Cluster Build) 和 Phase 1.4 (PBR Shader) 将消费这些 SSBO
 *
 * SSBO 绑定点分配：
 *   binding 0: PointLightSSBO （header + GPUPointLight[]）
 *   binding 1: SpotLightSSBO  （header + GPUSpotLight[]）
 */

#ifndef DSE_RENDER_LIGHT_BUFFER_H
#define DSE_RENDER_LIGHT_BUFFER_H

#include <vector>
#include <glm/glm.hpp>
#include <cstring>
#include "engine/render/rhi/rhi_handle.h"

class World;

namespace dse {
namespace render {

class RhiDevice;

/// 最大支持光源数（SSBO 容量上限）
static constexpr int kMaxClusteredPointLights = 256;
static constexpr int kMaxClusteredSpotLights  = 256;

/// SSBO 绑定点 — 匹配 pbr.frag 中 layout(binding = N) 声明
static constexpr unsigned int kSSBOBindingPointLights = 1;
static constexpr unsigned int kSSBOBindingSpotLights  = 2;

// ============================================================
// GPU 光源结构体（std430 布局，与 SSBO 对齐）
// ============================================================

/// 点光源 SSBO 条目 — 48 bytes（3 × 16B）
struct GPUPointLight {
    glm::vec3 color;       float intensity;     // 16B
    glm::vec3 position;    float radius;         // 16B
    int       cast_shadow; int shadow_index;
    int       _pad0;       int _pad1;            // 16B
};
static_assert(sizeof(GPUPointLight) == 48, "GPUPointLight must be 48 bytes");

/// 聚光灯 SSBO 条目 — 64 bytes（4 × 16B）
struct GPUSpotLight {
    glm::vec3 color;       float intensity;     // 16B
    glm::vec3 position;    float radius;         // 16B
    glm::vec3 direction;   float inner_cone;     // 16B
    float     outer_cone;  int cast_shadow;
    int       shadow_index; float _pad0;          // 16B
};
static_assert(sizeof(GPUSpotLight) == 64, "GPUSpotLight must be 64 bytes");

/// SSBO 头部 — 16 bytes
struct LightBufferHeader {
    int count;
    int _pad0, _pad1, _pad2;
};
static_assert(sizeof(LightBufferHeader) == 16, "LightBufferHeader must be 16 bytes");

// ============================================================
// LightBuffer — 每帧光源收集 + SSBO 管理
// ============================================================

class LightBuffer {
public:
    LightBuffer() = default;
    ~LightBuffer() = default;

    /// 初始化 SSBO（在 RHI 设备就绪后调用）
    void Init(RhiDevice* device);

    /// 从 ECS World 收集所有光源到 CPU 端缓冲
    void CollectLights(World& world);

    /// 将 CPU 端数据上传到 SSBO
    void Upload();

    /// 绑定 SSBO 到着色器绑定点
    void Bind();

    /// 释放 SSBO 资源
    void Shutdown();

    // --- 只读访问 ---
    int point_light_count() const { return static_cast<int>(point_lights_.size()); }
    int spot_light_count()  const { return static_cast<int>(spot_lights_.size()); }
    const std::vector<GPUPointLight>& point_lights() const { return point_lights_; }
    const std::vector<GPUSpotLight>&  spot_lights()  const { return spot_lights_; }

private:
    RhiDevice* device_ = nullptr;

    // GPU SSBO 句柄
    BufferHandle point_light_ssbo_;
    BufferHandle spot_light_ssbo_;

    // SSBO 当前分配容量（元素数）
    int point_light_capacity_ = 0;
    int spot_light_capacity_  = 0;

    // CPU 端光源数据
    std::vector<GPUPointLight> point_lights_;
    std::vector<GPUSpotLight>  spot_lights_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_LIGHT_BUFFER_H

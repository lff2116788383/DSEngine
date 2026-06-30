/**
 * @file gpu_particle_system.h
 * @brief GPU Compute Shader 驱动的粒子系统
 *
 * 粒子生命周期（发射/物理模拟/老化/死亡）完全在 GPU 执行：
 * - 双缓冲 SSBO 存储粒子状态（位置/速度/生命/颜色/大小）
 * - Compute shader 每帧更新 + 发射新粒子
 * - Atomic counter 维护活跃粒子数
 * - Indirect draw buffer 避免 GPU→CPU 回读
 *
 * 支持：
 * - 重力 / 风力 / 涡旋 / 噪声力场
 * - Color-over-life / Size-over-life 曲线
 * - 发射形状（点/球/锥/环）
 * - 碰撞平面
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class RhiDevice;

/// 发射形状
enum class EmitterShape : uint8_t {
    Point = 0,
    Sphere,
    Cone,
    Ring,
    Box
};

/// GPU 粒子系统配置（ECS 组件数据）
struct GpuParticleEmitterConfig {
    bool enabled = true;

    // 发射
    uint32_t max_particles = 10000;
    float emission_rate = 500.0f;          ///< 粒子/秒
    EmitterShape shape = EmitterShape::Point;
    float shape_radius = 1.0f;
    float cone_angle = 45.0f;              ///< 锥形发射半角

    // 初始值范围
    float life_min = 1.0f;
    float life_max = 3.0f;
    float speed_min = 2.0f;
    float speed_max = 8.0f;
    float size_start = 0.3f;
    float size_end = 0.01f;

    // 颜色演化
    glm::vec4 color_start = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f);
    glm::vec4 color_end = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

    // 力场
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    glm::vec3 wind = glm::vec3(0.0f);
    float turbulence = 0.0f;               ///< 噪声力场强度
    float vortex_strength = 0.0f;          ///< 涡旋力

    // 碰撞
    bool collision_enabled = false;
    float collision_plane_y = 0.0f;        ///< Y 平面碰撞
    float collision_bounce = 0.3f;         ///< 反弹系数
    float collision_friction = 0.8f;       ///< 摩擦

    // 渲染
    std::string texture_path;
    bool additive_blend = true;            ///< true=加性，false=alpha
    bool sort_particles = false;           ///< 是否按深度排序（开销较高）
};

/// GPU 粒子组件（ECS 中每个粒子发射器实体持有）
struct GpuParticleComponent {
    GpuParticleEmitterConfig config;

    // GPU 资源（运行时由 GpuParticleManager 管理）
    unsigned int particle_buffer_a = 0;    ///< SSBO: 粒子状态 buffer A (ping)
    unsigned int particle_buffer_b = 0;    ///< SSBO: 粒子状态 buffer B (pong)
    unsigned int counter_buffer = 0;       ///< SSBO: atomic counters (alive_count, dead_count, emit_count)
    unsigned int indirect_buffer = 0;      ///< Indirect draw args buffer
    unsigned int texture_handle = 0;       ///< 粒子纹理

    bool ping = true;                      ///< 当前读 A 写 B (true) 还是读 B 写 A (false)
    float emit_accumulator = 0.0f;
    bool initialized = false;
};

/**
 * @class GpuParticleManager
 * @brief 管理所有 GPU 粒子系统的 compute dispatch 和渲染
 *
 * 每帧流程：
 * 1. BeginComputePass
 * 2. 对每个 GpuParticleComponent：
 *    a. 绑定 SSBO (ping/pong)
 *    b. 设置 uniforms (dt, gravity, emission params...)
 *    c. Dispatch particle_update.comp (模拟 + 死亡回收)
 *    d. Dispatch particle_emit.comp (发射新粒子)
 * 3. EndComputePass + MemoryBarrier
 * 4. 渲染：使用已有 ParticleRenderer (SSBO→billboard)
 */
class GpuParticleManager {
public:
    GpuParticleManager() = default;
    ~GpuParticleManager() = default;

    /// 初始化 compute shader（在 RHI 可用后调用一次）
    bool Init(RhiDevice* rhi);

    /// 初始化单个粒子组件的 GPU 资源
    void InitComponent(GpuParticleComponent& comp, RhiDevice* rhi);

    /// 每帧更新所有粒子（compute dispatch）
    void Update(GpuParticleComponent& comp, RhiDevice* rhi,
                const glm::vec3& emitter_pos, float delta_time);

    /// 释放 compute shader
    void Shutdown(RhiDevice* rhi);

    /// 释放单个组件的 GPU 资源
    void ShutdownComponent(GpuParticleComponent& comp, RhiDevice* rhi);

private:
    unsigned int update_shader_ = 0;   ///< particle_update compute shader
    unsigned int emit_shader_ = 0;     ///< particle_emit compute shader
    bool inited_ = false;
};

} // namespace render
} // namespace dse

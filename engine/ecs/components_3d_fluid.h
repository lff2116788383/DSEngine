#ifndef DSE_COMPONENTS_3D_FLUID_H
#define DSE_COMPONENTS_3D_FLUID_H

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace dse {

/// 单个流体粒子（运行时状态）
struct FluidParticle {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 velocity = glm::vec3(0.0f);
    float density = 0.0f;        ///< SPH 密度估计
    float pressure = 0.0f;       ///< 由密度推导的压力
    float life = 0.0f;           ///< 剩余生命时间（秒）
};

/// 流体发射器形状
enum class FluidEmitterShape {
    Point = 0,     ///< 点发射
    Sphere = 1,    ///< 球形区域发射
    Box = 2        ///< 箱形区域发射
};

/// 流体模拟与渲染的 ECS 组件
struct FluidEmitterComponent {
    bool enabled = true;

    // --- 发射配置 ---
    FluidEmitterShape shape = FluidEmitterShape::Point;
    float emission_rate = 500.0f;       ///< 每秒发射粒子数
    float particle_lifetime = 3.0f;     ///< 每个粒子的生命时间（秒）
    float particle_radius = 0.05f;      ///< 视觉 + 物理半径
    glm::vec3 emit_direction = {0.0f, -1.0f, 0.0f}; ///< 初始速度方向
    float emit_speed = 2.0f;            ///< 沿发射方向的初始速度
    float emit_spread = 0.3f;           ///< 锥形扩散角（弧度）

    // 发射器形状参数
    float sphere_radius = 0.5f;         ///< 球形发射器半径
    glm::vec3 box_half_extents = {0.5f, 0.1f, 0.5f}; ///< 箱形发射器半尺寸

    // --- 物理参数 ---
    glm::vec3 gravity = {0.0f, -9.81f, 0.0f};
    float viscosity = 0.01f;            ///< 粒子间速度平滑（粘性）
    float surface_tension = 0.05f;      ///< 表面张力系数
    float rest_density = 1000.0f;       ///< 目标密度 (kg/m³)
    float gas_stiffness = 50.0f;        ///< 压力 = stiffness * (density/rest_density - 1)
    float damping = 0.01f;              ///< 速度阻尼

    // --- 碰撞 ---
    float collision_restitution = 0.3f; ///< 碰撞反弹系数
    float floor_y = 0.0f;              ///< 简单地面平面高度

    // --- 渲染（Screen-Space Fluid）---
    glm::vec4 color = {0.2f, 0.5f, 0.9f, 0.8f};
    float refraction_strength = 0.3f;   ///< 折射强度
    float fresnel_power = 2.0f;         ///< 菲涅尔指数
    float specular_intensity = 0.8f;    ///< 高光强度
    float depth_smoothing_radius = 5.0f; ///< 双边滤波半径（像素）

    // --- 运行时状态 ---
    std::vector<FluidParticle> particles;
    float emit_accumulator = 0.0f;       ///< 小数粒子累加器
    uint32_t active_count = 0;           ///< 存活粒子数量

    // GPU 实例缓冲区（用于渲染）
    unsigned int instance_vbo = 0;
    bool gpu_dirty = false;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_FLUID_H

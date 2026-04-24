/**
 * @file particle_2d.h
 * @brief 2D 粒子发射器组件与生命周期曲线
 */

#ifndef DSE_ECS_COMPONENTS_2D_PARTICLE_2D_H
#define DSE_ECS_COMPONENTS_2D_PARTICLE_2D_H

#include <glm/glm.hpp>
#include <algorithm>
#include <memory>
#include <vector>

class TextureAsset;

/**
 * @struct Particle2D
 * @brief 单个粒子的运行时数据结构
 */
struct Particle2D {
    glm::vec3 position;         ///< 粒子当前的世界坐标
    glm::vec3 velocity;         ///< 粒子的运动速度
    glm::vec4 color;            ///< 粒子的当前颜色
    float life_time;            ///< 粒子的总生命周期(秒)
    float life_remaining;       ///< 粒子的剩余生命周期(秒)
    float size;                 ///< 粒子的当前尺寸
    float rotation = 0.0f;      ///< 粒子旋转角度(弧度)
    float angular_velocity = 0.0f; ///< 角速度(弧度/秒)
};

/**
 * @enum ParticleCurveType
 * @brief 粒子曲线类型，用于驱动生命周期参数插值
 */
enum class ParticleCurveType {
    Linear,     ///< 线性插值
    EaseIn,     ///< 缓入
    EaseOut,    ///< 缓出
    EaseInOut   ///< 缓入缓出
};

/**
 * @struct ParticleCurve
 * @brief 标量粒子曲线，适用于尺寸、透明度、速度缩放等生命周期参数
 */
struct ParticleCurve {
    bool enabled = false;                               ///< 是否启用曲线
    ParticleCurveType type = ParticleCurveType::Linear; ///< 曲线类型
    float start_value = 1.0f;                           ///< 生命周期起始值
    float end_value = 0.0f;                             ///< 生命周期结束值

    float Evaluate(float t) const {
        t = std::clamp(t, 0.0f, 1.0f);
        float shaped_t = t;
        switch (type) {
        case ParticleCurveType::Linear:
            shaped_t = t;
            break;
        case ParticleCurveType::EaseIn:
            shaped_t = t * t;
            break;
        case ParticleCurveType::EaseOut:
            shaped_t = 1.0f - (1.0f - t) * (1.0f - t);
            break;
        case ParticleCurveType::EaseInOut:
            if (t < 0.5f) {
                shaped_t = 2.0f * t * t;
            } else {
                const float k = -2.0f * t + 2.0f;
                shaped_t = 1.0f - (k * k) * 0.5f;
            }
            break;
        }
        return glm::mix(start_value, end_value, shaped_t);
    }
};

/**
 * @enum ParticleCollisionMode
 * @brief 粒子碰撞模式，便于从简易地面碰撞逐步扩展到更正式的碰撞系统
 */
enum class ParticleCollisionMode {
    None,        ///< 不启用碰撞
    GroundPlane, ///< 与简易地面平面碰撞
    Box2D        ///< 预留给 Box2D 集成
};

/**
 * @struct ParticleEmitterComponent
 * @brief 粒子发射器组件，控制粒子的生成规则和渲染材质
 */
struct ParticleEmitterComponent {
    std::vector<Particle2D> particles;                   ///< 活跃的粒子池
    std::shared_ptr<TextureAsset> texture;               ///< 粒子的贴图资产
    unsigned int texture_handle = 0;                     ///< 粒子的渲染纹理句柄
    int max_particles = 100;                             ///< 允许的最大粒子数量
    float emit_rate = 10.0f;                             ///< 每秒发射的粒子数
    float emit_rate_scale = 1.0f;                        ///< 发射率缩放倍数
    float emit_accumulator = 0.0f;                       ///< 发射计时累加器
    bool emitting = true;                                ///< 是否持续发射中
    int pending_burst = 0;                               ///< 等待爆发(一次性生成)的粒子数
    
    // Emission parameters
    float start_life_time = 2.0f;                        ///< 新粒子的初始生命周期
    float start_size = 1.0f;                             ///< 新粒子的初始尺寸
    glm::vec4 start_color = glm::vec4(1.0f);             ///< 新粒子的初始颜色

    // --- Advanced: Randomization ---
    glm::vec3 velocity_min = glm::vec3(-1.0f, 0.5f, 0.0f);  ///< 随机速度下限
    glm::vec3 velocity_max = glm::vec3(1.0f, 2.0f, 0.0f);   ///< 随机速度上限
    float life_time_min = 1.0f;                              ///< 随机生命周期下限
    float life_time_max = 3.0f;                              ///< 随机生命周期上限
    float size_min = 0.5f;                                   ///< 随机尺寸下限
    float size_max = 1.5f;                                   ///< 随机尺寸上限
    float rotation_min = 0.0f;                               ///< 随机初始旋转下限(弧度)
    float rotation_max = 6.2832f;                            ///< 随机初始旋转上限(弧度)
    float angular_velocity_min = -1.0f;                      ///< 随机角速度下限
    float angular_velocity_max = 1.0f;                       ///< 随机角速度上限
    bool use_random_params = false;                          ///< 是否启用随机参数

    // --- Advanced: Lifetime Curves ---
    bool use_size_curve = false;                             ///< 是否启用尺寸曲线（兼容旧字段）
    float size_curve_end = 0.0f;                             ///< 生命末期尺寸（兼容旧字段）
    bool use_alpha_curve = false;                            ///< 是否启用透明度曲线（兼容旧字段）
    float alpha_curve_end = 0.0f;                            ///< 生命末期透明度（兼容旧字段）
    bool use_color_curve = false;                            ///< 是否启用颜色曲线
    glm::vec4 color_curve_end = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f); ///< 生命末期颜色
    bool use_speed_curve = false;                            ///< 是否启用速度曲线（兼容旧字段）
    float speed_curve_end_scale = 0.0f;                      ///< 生命末期速度缩放（兼容旧字段）
    ParticleCurve size_curve = {false, ParticleCurveType::Linear, 1.0f, 0.0f};   ///< 正式尺寸曲线
    ParticleCurve alpha_curve = {false, ParticleCurveType::Linear, 1.0f, 0.0f};  ///< 正式透明度曲线
    ParticleCurve speed_curve = {false, ParticleCurveType::Linear, 1.0f, 0.0f};  ///< 正式速度曲线

    // --- Advanced: Gravity & Collision ---
    glm::vec3 gravity = glm::vec3(0.0f, 0.0f, 0.0f);        ///< 粒子重力加速度
    bool enable_collision = false;                           ///< 是否启用碰撞检测
    ParticleCollisionMode collision_mode = ParticleCollisionMode::None; ///< 碰撞模式
    float collision_bounce = 0.5f;                           ///< 碰撞反弹系数
    float collision_friction = 0.1f;                         ///< 碰撞摩擦系数
    float collision_life_loss = 0.0f;                        ///< 碰撞时生命损失
    float ground_y = 0.0f;                                   ///< 简易地面碰撞Y坐标
    bool use_ground_collision = false;                       ///< 是否启用简易地面碰撞
};

#endif // DSE_ECS_COMPONENTS_2D_PARTICLE_2D_H

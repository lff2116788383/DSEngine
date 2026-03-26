/**
 * @file particle_system.h
 * @brief 粒子系统，管理粒子发射器、生命周期和渲染批处理
 */

#ifndef DSE_PARTICLE_SYSTEM_H
#define DSE_PARTICLE_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

/**
 * @class ParticleSystem
 * @brief 2D粒子系统，负责粒子发射器的状态更新、粒子的生命周期管理及提交渲染批次
 */
class ParticleSystem {
public:
    /**
     * @brief 更新所有粒子发射器和活动粒子的状态（位置、生命周期、颜色等）
     * @param world 包含粒子组件的实体世界
     * @param delta_time 距离上一帧的时间间隔（秒）
     * @example
     * // particle_system.Update(world, dt);
     */
    void Update(World& world, float delta_time);
    
    /**
     * @brief 收集并提交所有存活粒子的渲染命令到渲染管线
     * @param world 包含粒子组件的实体世界
     * @param cmd_buffer 目标渲染命令缓冲
     */
    void Render(World& world, CommandBuffer& cmd_buffer);
};

#endif

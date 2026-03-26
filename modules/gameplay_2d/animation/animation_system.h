/**
 * @file animation_system.h
 * @brief 动画系统，处理精灵帧动画和骨骼动画的播放与状态机控制
 */

#ifndef DSE_ANIMATION_SYSTEM_H
#define DSE_ANIMATION_SYSTEM_H

#include "engine/ecs/world.h"

/**
 * @class AnimationSystem
 * @brief 帧动画系统，遍历所有带动画组件的实体并更新其当前播放帧
 */
class AnimationSystem {
public:
    /**
     * @brief 执行每帧动画更新操作
     * @param world 包含动画组件的世界对象
     * @param delta_time 距离上一帧的时间间隔（秒）
     * @example
     * // AnimationSystem::Update(world, dt);
     */
    void Update(World& world, float delta_time);
};

#endif

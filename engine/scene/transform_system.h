/**
 * @file transform_system.h
 * @brief 场景层级与变换系统，负责更新实体的本地/世界变换矩阵
 */

#ifndef DSE_TRANSFORM_SYSTEM_H
#define DSE_TRANSFORM_SYSTEM_H

#include "engine/ecs/world.h"

/**
 * @class TransformSystem
 * @brief 变换系统，遍历世界中的所有变换组件，计算并更新模型矩阵
 */
class TransformSystem {
public:
    /**
     * @brief 执行每帧变换组件的更新操作
     * @param world 包含变换组件的实体世界引用
     */
    void Update(World& world);
};

#endif

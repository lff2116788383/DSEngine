/**
 * @file components_2d.h
 * @brief 2D 游戏引擎的所有 ECS 数据组件定义 (聚合头文件)
 *
 * 此文件为向后兼容的聚合入口，包含了所有 2D 组件。
 * 新代码建议按需包含具体子文件，以减少编译依赖：
 *   #include "engine/ecs/transform.h"    // 仅变换组件
 *   #include "engine/ecs/sprite.h"       // 仅精灵组件
 *   #include "engine/ecs/ui.h"           // 仅 UI 组件
 *   等等
 */

#ifndef DSE_COMPONENTS_2D_H
#define DSE_COMPONENTS_2D_H

#include "engine/ecs/transform.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/physics_2d.h"
#include "engine/ecs/animation.h"
#include "engine/ecs/particle_2d.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/tilemap.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/script.h"
#include "engine/ecs/gameplay.h"

#endif // DSE_COMPONENTS_2D_H

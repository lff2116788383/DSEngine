/**
 * @file sprite_render_system.h
 * @brief 2D 渲染系统，处理精灵图(Sprite)和 UI 元素的渲染指令提交
 */

#ifndef DSE_SPRITE_RENDER_SYSTEM_H
#define DSE_SPRITE_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

/**
 * @class SpriteRenderSystem
 * @brief 精灵图渲染系统，遍历所有的 SpriteRenderer 组件并提交渲染批次
 */
class SpriteRenderSystem {
public:
    /**
     * @brief 执行精灵图的渲染命令收集
     * @param world 包含精灵和变换组件的实体世界
     * @param cmd_buffer 目标渲染命令缓冲
     */
    void Render(World& world, CommandBuffer& cmd_buffer);
};

/**
 * @class UIRenderSystem
 * @brief UI渲染系统，基于正交投影在屏幕空间绘制 UI 元素
 */
class UIRenderSystem {
public:
    /**
     * @brief 执行 UI 元素的渲染命令收集
     * @param world 包含 UI 组件的实体世界
     * @param cmd_buffer 目标渲染命令缓冲
     * @param screen_width 当前屏幕宽度
     * @param screen_height 当前屏幕高度
     */
    void Render(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height, const glm::mat4& clip_correction = glm::mat4(1.0f));
};

#endif

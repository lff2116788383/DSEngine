/**
 * @file sprite_render_system.h
 * @brief 2D 渲染系统，处理精灵图(Sprite)和 UI 元素的渲染指令提交
 */

#ifndef DSE_SPRITE_RENDER_SYSTEM_H
#define DSE_SPRITE_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/sprite_batch_renderer.h"
#include "engine/render/frame_context.h"
#include <vector>
#include <glm/glm.hpp>

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
    void Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame);

    /// 注入 RhiDevice（由所属模块在初始化时调用）。新 SpriteBatchRenderer 路径需要。
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }
    /// 释放内部批渲染器 GPU 资源（模块关闭、device 仍有效时调用）。
    void Shutdown() { if (rhi_device_) sprite_batch_.Shutdown(*rhi_device_); }

private:
    RhiDevice* rhi_device_ = nullptr;
    dse::render::SpriteBatchRenderer sprite_batch_;
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

    /// 注入 RhiDevice（由所属模块在初始化时调用）。新 SpriteBatchRenderer 路径需要。
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }
    /// 释放内部批渲染器 GPU 资源（模块关闭、device 仍有效时调用）。
    void Shutdown() { if (rhi_device_) sprite_batch_.Shutdown(*rhi_device_); }

private:
    RhiDevice* rhi_device_ = nullptr;
    dse::render::SpriteBatchRenderer sprite_batch_;
};

/**
 * @brief 将 9 宫格参数展开为最多 9 个 SpriteDrawItem 并追加到输出容器
 *
 * 9 宫格划分规则：以 border (left, bottom, right, top) UV 分量将 sprite 区域分成
 * 3×3 共 9 个子矩形。角块固定采样对应边角 UV，中心与边块在另一方向上拉伸。
 * 尺寸退化（宽或高 <= 0）的格子将被跳过。
 *
 * @param base_item   已填充 texture_handle / color / sorting_layer / order_in_layer 的基础绘制项
 * @param final_pos   控件中心的屏幕坐标（像素，y 轴朝上）
 * @param size        控件总尺寸（像素）
 * @param uv          精灵 UV 区域 (offset_u, offset_v, extent_u, extent_v)
 * @param border      9 宫格边框 UV 分量 (left, bottom, right, top)，有效范围 [0, 0.5]
 * @param src_size    源精灵像素尺寸；> 0 时角块屏幕尺寸固定为 border * src_size（弹性面板模式）
 *                    = (0,0) 时角块随控件等比缩放（均匀缩放模式）
 * @param out_items   展开结果追加目标
 */
void Expand9SliceItems(const SpriteDrawItem& base_item,
                       const glm::vec2& final_pos,
                       const glm::vec2& size,
                       const glm::vec4& uv,
                       const glm::vec4& border,
                       const glm::vec2& src_size,
                       std::vector<SpriteDrawItem>& out_items);

#endif

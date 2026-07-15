/**
 * @file trail_system.h
 * @brief 2D 拖尾渲染系统
 */

#ifndef DSE_TRAIL_SYSTEM_H
#define DSE_TRAIL_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/sprite_batch_renderer.h"
#include "engine/render/frame_context.h"

class TrailSystem {
public:
    void Update(World& world, float delta_time);
    /// Phase 1：主线程（Prepare）从 ECS 提取拖尾绘制项，渲染线程 Render 消费。
    void ExtractFrameRenderData(World& world);
    void Render(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame);
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }
    void Shutdown() { if (rhi_device_) sprite_batch_.Shutdown(*rhi_device_); }

private:
    RhiDevice* rhi_device_ = nullptr;
    dse::render::SpriteBatchRenderer sprite_batch_;
    std::vector<SpriteDrawItem> frame_items_;
};

#endif // DSE_TRAIL_SYSTEM_H

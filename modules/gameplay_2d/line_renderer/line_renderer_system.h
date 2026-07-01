/**
 * @file line_renderer_system.h
 * @brief 2D 折线渲染系统
 */

#ifndef DSE_LINE_RENDERER_SYSTEM_H
#define DSE_LINE_RENDERER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/sprite_batch_renderer.h"
#include "engine/render/frame_context.h"

class LineRendererSystem {
public:
    void Update(World& world, float delta_time);
    void Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame);
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }
    void Shutdown() { if (rhi_device_) sprite_batch_.Shutdown(*rhi_device_); }

private:
    RhiDevice* rhi_device_ = nullptr;
    dse::render::SpriteBatchRenderer sprite_batch_;
};

#endif // DSE_LINE_RENDERER_SYSTEM_H

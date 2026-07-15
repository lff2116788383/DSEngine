/**
 * @file light_2d_system.h
 * @brief 2D 灯光系统
 */

#ifndef DSE_LIGHT_2D_SYSTEM_H
#define DSE_LIGHT_2D_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/frame_context.h"

class Light2DSystem {
public:
    void Update(World& world, float delta_time);
    /// Phase 1：主线程（Prepare）提取灯光/环境光数据（当前仅计算、无 GPU 输出）。
    void ExtractFrameRenderData(World& world);
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }
    void Shutdown();

private:
    RhiDevice* rhi_device_ = nullptr;
    unsigned int light_accumulation_fbo_ = 0;
    unsigned int light_accumulation_tex_ = 0;
    bool resources_initialized_ = false;
};

#endif // DSE_LIGHT_2D_SYSTEM_H

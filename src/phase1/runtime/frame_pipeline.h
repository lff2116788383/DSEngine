#ifndef DSE_PHASE1_FRAME_PIPELINE_H
#define DSE_PHASE1_FRAME_PIPELINE_H

#include <memory>
#include "phase1/ecs/world.h"
#include "phase1/rhi/rhi_device.h"
#include "phase1/systems/transform_system.h"
#include "phase1/systems/physics2d_system.h"
#include "phase1/systems/sprite_render_system.h"
#include "phase1/systems/camera_system.h"
#include "phase1/systems/lua_script_system.h"

class Phase1FramePipeline {
public:
    static Phase1FramePipeline& Instance();

    void Init();
    void Update(float delta_time);
    void FixedUpdate(float fixed_delta_time);
    void Render();

    Phase1World& world();

private:
    Phase1FramePipeline() = default;
    ~Phase1FramePipeline() = default;

    Phase1World* world_ = nullptr;
    std::unique_ptr<RhiDevice> rhi_device_;
    
    TransformSystem transform_system_;
    CameraSystem camera_system_;
    SpriteRenderSystem sprite_render_system_;
    UIRenderSystem ui_render_system_;
    Physics2DSystem physics2d_system_;
    LuaScriptSystem lua_script_system_;
    
    bool initialized_ = false;
};  float stats_accumulator_ = 0.0f;
};

#endif

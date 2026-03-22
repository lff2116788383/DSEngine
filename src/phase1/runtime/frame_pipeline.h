#ifndef DSE_PHASE1_FRAME_PIPELINE_H
#define DSE_PHASE1_FRAME_PIPELINE_H

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <cstddef>
#include "phase1/ecs/world.h"
#include "phase1/rhi/rhi_device.h"
#include "phase1/systems/transform_system.h"
#include "phase1/systems/physics2d_system.h"
#include "phase1/systems/sprite_render_system.h"
#include "phase1/systems/camera_system.h"
class Phase1AssetManager;

enum class Phase1BusinessMode {
    Lua = 0,
    Cpp = 1
};

class Phase1FramePipeline {
public:
    static Phase1FramePipeline& Instance();

    bool Init();
    void Shutdown();
    void Update(float delta_time);
    void FixedUpdate(float fixed_delta_time);
    void Render();

    void SetWorld(Phase1World* world);
    Phase1World& world();
    int LastDrawCalls() const;
    int LastMaxBatchSprites() const;
    int LastSpriteCount() const;
    void SetWindowTitleSetter(std::function<void(const std::string&)> setter);
    void SetWindowTitle(const std::string& title);
    void SetBusinessMode(Phase1BusinessMode mode);
    void SetAssetManager(Phase1AssetManager* asset_manager);

private:
    struct RenderGraphPass {
        std::string name;
        std::function<void(CommandBuffer&)> execute;
    };

    void BuildRenderGraph();
    void ExecuteRenderGraph(CommandBuffer& cmd_buffer);

    Phase1FramePipeline() = default;
    ~Phase1FramePipeline() = default;

    Phase1World* world_ = nullptr;
    std::unique_ptr<RhiDevice> rhi_device_;
    
    TransformSystem transform_system_;
    CameraSystem camera_system_;
    SpriteRenderSystem sprite_render_system_;
    UIRenderSystem ui_render_system_;
    Physics2DSystem physics2d_system_;
    
    bool initialized_ = false;
    float stats_accumulator_ = 0.0f;
    int last_draw_calls_ = 0;
    int last_max_batch_sprites_ = 0;
    int last_sprite_count_ = 0;
    std::size_t callback_budget_per_frame_ = 16;
    float update_time_accumulator_ms_ = 0.0f;
    float fixed_time_accumulator_ms_ = 0.0f;
    float render_time_accumulator_ms_ = 0.0f;
    int update_samples_ = 0;
    int fixed_samples_ = 0;
    int render_samples_ = 0;
    std::function<void(const std::string&)> window_title_setter_;
    Phase1BusinessMode business_mode_ = Phase1BusinessMode::Lua;
    unsigned int main_render_target_ = 0;
    unsigned int scene_render_target_ = 0;
    unsigned int ui_render_target_ = 0;
    unsigned int sprite_pipeline_state_ = 0;
    unsigned int composite_pipeline_state_ = 0;
    std::vector<RenderGraphPass> render_graph_passes_;
    Phase1AssetManager* asset_manager_ = nullptr;
};

#endif

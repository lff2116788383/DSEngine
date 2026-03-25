#ifndef DSE_FRAME_PIPELINE_H
#define DSE_FRAME_PIPELINE_H

#include <memory>
#include <functional>
#include <vector>
#include <string>
#include <cstddef>
#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/scene/transform_system.h"
#include "engine/physics/physics2d/physics2d_system.h"
#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "modules/gameplay_2d/camera/camera_system.h"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "engine/audio/audio_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
class AssetManager;

enum class BusinessMode {
    Lua = 0,
    Cpp = 1
};

class FramePipeline {
public:
    static FramePipeline& Instance();

    bool Init();
    void Shutdown();
    void Update(float delta_time);
    void FixedUpdate(float fixed_delta_time);
    void Render();

    void SetWorld(World* world);
    World& world();
    int LastDrawCalls() const;
    int LastMaxBatchSprites() const;
    int LastSpriteCount() const;
    void SetWindowTitleSetter(std::function<void(const std::string&)> setter);
    void SetWindowTitle(const std::string& title);
    void SetBusinessMode(BusinessMode mode);
    void SetAssetManager(AssetManager* asset_manager);

private:
    struct RenderGraphPass {
        std::string name;
        std::function<void(CommandBuffer&)> execute;
    };

    void BuildRenderGraph();
    void ExecuteRenderGraph(CommandBuffer& cmd_buffer);

    FramePipeline() = default;
    ~FramePipeline() = default;

    World* world_ = nullptr;
    std::unique_ptr<RhiDevice> rhi_device_;
    
    TransformSystem transform_system_;
    CameraSystem camera_system_;
    SpriteRenderSystem sprite_render_system_;
    UIRenderSystem ui_render_system_;
    Physics2DSystem physics2d_system_;
    AnimationSystem animation_system_;
    ParticleSystem particle_system_;
    dse::gameplay2d::UISystem ui_logic_system_;
    dse::gameplay2d::AudioSystem audio_system_;
    dse::gameplay2d::TilemapSystem tilemap_system_;
    
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
    BusinessMode business_mode_ = BusinessMode::Lua;
    unsigned int main_render_target_ = 0;
    unsigned int scene_render_target_ = 0;
    unsigned int ui_render_target_ = 0;
    unsigned int sprite_pipeline_state_ = 0;
    unsigned int composite_pipeline_state_ = 0;
    std::vector<RenderGraphPass> render_graph_passes_;
    AssetManager* asset_manager_ = nullptr;
};

#endif

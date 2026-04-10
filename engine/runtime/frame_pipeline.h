/**
 * @file frame_pipeline.h
 * @brief 帧流水线，负责协调引擎的各个子系统（渲染、物理、脚本等）按顺序执行。
 */

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
#include "modules/gameplay_2d/spine/spine_system.h"
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "engine/core/module.h"
#include "engine/core/dynamic_library.h"
#include "engine/runtime/runtime_frame_ops.h"
#include "engine/runtime/render_pipeline_resources.h"

class AssetManager;

enum class BusinessMode {
    Lua = 0,
    Cpp = 1
};

/**
 * @class FramePipeline
 * @brief 引擎的主循环流水线，单例模式，控制各个 System 的调用顺序和生命周期。
 */
class FramePipeline {
public:
    static FramePipeline& Instance();

    FramePipeline() = default;
    ~FramePipeline() = default;

    /**
     * @brief 初始化流水线及内部子系统
     * @return 初始化成功返回 true
     */
    bool Init();
    
    void Shutdown();
    
    /**
     * @brief 每帧逻辑更新
     * @param delta_time 帧间隔时间
     */
    void Update(float delta_time);
    
    /**
     * @brief 固定步长物理更新
     * @param fixed_delta_time 固定的时间步长
     */
    void FixedUpdate(float fixed_delta_time);
    
    /**
     * @brief 执行渲染图和渲染管线
     */
    void Render();

    /**
     * @brief 设置是否启用编辑器模式 (在Init前调用)
     */
    void EnableEditorMode(bool enable);

    /**
     * @brief 注入当前激活的实体世界
     * @param world 世界对象指针
     */
    void SetWorld(World* world);
    
    /**
     * @brief 获取当前绑定的世界对象
     * @return World 引用
     * @warning 如果 world 未注入将触发 assert 宕机
     */
    World& world();

    /**
     * @brief 获取上一帧的 DrawCall 数量
     * @return DrawCall 总数
     */
    int LastDrawCalls() const;

    /**
     * @brief 获取上一帧中最大的精灵图批处理数量
     * @return 单个批次内包含的最大精灵数
     */
    int LastMaxBatchSprites() const;

    /**
     * @brief 获取上一帧提交渲染的精灵总数
     * @return 精灵数量
     */
    int LastSpriteCount() const;

    /**
     * @brief 注入用于修改底层平台窗口标题的回调函数
     * @param setter 回调函数
     */
    void SetWindowTitleSetter(std::function<void(const std::string&)> setter);

    /**
     * @brief 设置应用窗口标题（通常用于显示帧率等调试信息）
     * @param title 标题字符串
     */
    void SetWindowTitle(const std::string& title);

    /**
     * @brief 设置业务逻辑驱动模式
     * @param mode 驱动模式（Lua 或原生 C++）
     */
    void SetBusinessMode(BusinessMode mode);

    /**
     * @brief 注入全局资产管理器实例
     * @param asset_manager 资产管理器指针
     */
    void SetAssetManager(AssetManager* asset_manager);

    /**
     * @brief 获取渲染管线中场景纹理的句柄 (用于编辑器集成)
     * @return 纹理 ID
     */
    unsigned int GetSceneTextureId() const;

    /**
     * @brief 获取渲染管线中游戏视图（最终合成）的纹理句柄 (用于编辑器集成)
     * @return 纹理 ID
     */
    unsigned int GetMainTextureId() const;

private:
    struct RenderGraphPass {
        std::string name;
        std::function<void(CommandBuffer&)> execute;
    };

    friend void dse::runtime::RunFrameUpdate(FramePipeline& pipeline, float delta_time);
    friend void dse::runtime::RunFrameFixedUpdate(FramePipeline& pipeline, float fixed_delta_time);
    friend void dse::runtime::RunFrameRender(FramePipeline& pipeline);
    friend void dse::runtime::BuildFrameRenderGraph(FramePipeline& pipeline);
    friend void dse::runtime::ExecuteFrameRenderGraph(FramePipeline& pipeline, CommandBuffer& cmd_buffer);

private:
    void RunUpdateInternal(float delta_time);
    void RunFixedUpdateInternal(float fixed_delta_time);
    void RunRenderInternal();
    void BuildRenderGraphInternal();
    void ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer);

    void BuildRenderGraph();
    void ExecuteRenderGraph(CommandBuffer& cmd_buffer);


    World* world_ = nullptr;
    std::unique_ptr<RhiDevice> rhi_device_;
    
    TransformSystem transform_system_;
    CameraSystem camera_system_;
    SpriteRenderSystem sprite_render_system_;
    UIRenderSystem ui_render_system_;
    Physics2DSystem physics2d_system_;
    AnimationSystem animation_system_;
    ParticleSystem particle_system_;
    dse::gameplay2d::SpineSystem spine_system_;
    dse::gameplay2d::UISystem ui_logic_system_;
    dse::gameplay2d::AudioSystem audio_system_;
    dse::gameplay2d::TilemapSystem tilemap_system_;
    dse::gameplay3d::MeshRenderSystem mesh_render_system_;
    
    // 动态模块化架构：不再直接实例化 3D 相关系统
    struct LoadedModule {
        std::unique_ptr<dse::core::DynamicLibrary> lib;
        dse::core::IModule* instance = nullptr;
        using DestroyModuleFunc = void(*)(dse::core::IModule*);
        DestroyModuleFunc destroy = nullptr;
    };
    std::vector<LoadedModule> modules_;
    
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
    dse::runtime::RenderPipelineResources render_resources_;
    std::vector<RenderGraphPass> render_graph_passes_;
    AssetManager* asset_manager_ = nullptr;
    bool editor_mode_ = false;
};

#endif

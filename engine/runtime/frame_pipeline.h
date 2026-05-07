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
#if defined(DSE_ENABLE_3D) && defined(DSE_ENABLE_PHYSX)
#include "engine/physics/physics3d/physics3d_system.h"
#endif
#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "modules/gameplay_2d/camera/camera_system.h"
#include "modules/gameplay_2d/ui/ui_system.h"
#include "engine/audio/audio_system.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "modules/gameplay_2d/animation/animation_system.h"
#include "modules/gameplay_2d/particle/particle_system.h"
#include "modules/gameplay_2d/spine/spine_system.h"
#include "modules/gameplay_2d/gameplay_2d_module.h"
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#ifdef DSE_ENABLE_3D
#include "modules/gameplay_3d/gameplay_3d_module.h"
#else
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "modules/gameplay_3d/ai/steering_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#endif
#include "engine/core/module.h"
#include "engine/core/dynamic_library.h"
#include "engine/runtime/runtime_frame_ops.h"
#include "engine/runtime/runtime_update_graph.h"
#include "engine/runtime/runtime_render_shell.h"
#include "engine/runtime/render_pipeline_resources.h"
#include "engine/runtime/runtime_context.h"
#include "engine/runtime/business_runtime_bridge.h"
#include "engine/render/render_graph.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/passes/builtin_passes.h"

class AssetManager;

/**
 * @class FramePipeline
 * @brief 引擎的主循环流水线，负责逐帧调度与直接渲染相关初始化，不承接高层启动期副作用流程。
 */
class FramePipeline {
public:
    [[deprecated("Use EngineInstance::pipeline() or injected FramePipeline instance")]]
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
     * @brief 注入平台原生窗口句柄（Win32 HWND），D3D11/Vulkan 后端初始化时需要
     * @param handle Win32 HWND（或其他平台的等价指针）
     */
    void SetNativeWindowHandle(void* handle);

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
     * @brief 获取上一帧中的材质切换次数
     * @return 材质状态切换总数
     */
    int LastMaterialSwitches() const;

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
     * @brief 设置编辑器相机矩阵，覆盖 Scene 渲染目标使用的相机
     * @param view 视图矩阵
     * @param projection 投影矩阵
     */
    void SetEditorCamera(const glm::mat4& view, const glm::mat4& projection);

    /**
     * @brief 禁用编辑器相机覆盖，恢复使用游戏相机
     */
    void DisableEditorCamera();

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

    /**
     * @brief 读取上一帧场景渲染目标的 RGBA8 像素，用于截图和图像回归
     * @return 像素缓冲，失败或未初始化时返回空数组
     */
    std::vector<unsigned char> ReadSceneColorRgba8() const;

    /**
     * @brief 读取上一帧场景渲染目标的尺寸与 RGBA8 像素，用于截图导出
     * @return 包含宽高与像素缓冲的读回结果
     */
    RenderTargetReadback ReadSceneColorRgba8WithSize() const;

    /**
     * @brief 读取上一帧最终合成渲染目标的 RGBA8 像素，用于编辑器与截图验证
     * @return 像素缓冲，失败或未初始化时返回空数组
     */
    std::vector<unsigned char> ReadMainColorRgba8() const;

    /**
     * @brief 读取上一帧最终合成渲染目标的尺寸与 RGBA8 像素，用于编辑器截图验证
     * @return 包含宽高与像素缓冲的读回结果
     */
    RenderTargetReadback ReadMainColorRgba8WithSize() const;

    /// 基于 DAG 的渲染图
    dse::render::RenderGraph render_graph_dag_;

    friend void dse::runtime::RunFrameUpdate(FramePipeline& pipeline, float delta_time);
    friend void dse::runtime::RunFrameFixedUpdate(FramePipeline& pipeline, float fixed_delta_time);
    friend void dse::runtime::RunFrameRender(FramePipeline& pipeline);
    friend void dse::runtime::BuildFrameRenderGraph(FramePipeline& pipeline);
    friend void dse::runtime::ExecuteFrameRenderGraph(FramePipeline& pipeline, CommandBuffer& cmd_buffer);
    friend void dse::runtime::RunRuntimeUpdateGraph(FramePipeline& pipeline, float delta_time);
    friend void dse::runtime::RunRuntimeFixedUpdateGraph(FramePipeline& pipeline, float fixed_delta_time);
    friend void dse::runtime::BeginRuntimeRenderFrame(FramePipeline& pipeline);
    friend std::shared_ptr<CommandBuffer> dse::runtime::CreateRuntimeRenderCommandBuffer(FramePipeline& pipeline);
    friend void dse::runtime::BindRuntimeShadowMaps(FramePipeline& pipeline);
    friend void dse::runtime::SubmitAndEndRuntimeRenderFrame(FramePipeline& pipeline, std::shared_ptr<CommandBuffer> cmd_buffer);
    friend void dse::runtime::FinalizeRuntimeRenderFrame(FramePipeline& pipeline);

private:
    void RunUpdateInternal(float delta_time);
    void RunFixedUpdateInternal(float fixed_delta_time);
    void RunRenderInternal();
    void BuildRenderGraphInternal();
    void ExecuteRenderGraphInternal(CommandBuffer& cmd_buffer);

    void BuildRenderGraph();
    void ExecuteRenderGraph(CommandBuffer& cmd_buffer);


    dse::runtime::RuntimeContext runtime_context_{};
    
    dse::gameplay2d::Gameplay2DModule gameplay2d_module_;
    dse::gameplay3d::MeshRenderSystem mesh_render_system_;
#if defined(DSE_ENABLE_3D) && defined(DSE_ENABLE_PHYSX)
    dse::physics3d::Physics3DSystem physics3d_system_;
    bool physics3d_system_initialized_ = false;
#endif
#ifdef DSE_ENABLE_3D
    dse::gameplay3d::Gameplay3DModule gameplay3d_module_;
    bool builtin_gameplay3d_enabled_ = false;
#else
    dse::gameplay3d::Particle3DSystem particle3d_system_;
    dse::gameplay3d::SteeringSystem steering_system_;
    dse::gameplay3d::AnimatorSystem animator3d_system_;
    bool builtin_gameplay3d_enabled_ = false;
#endif
    
    // 动态模块优先；未启用完整 3D 构建时保留 Particle3D/Steering/Animator3D 最小内置更新链路。
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
    int last_material_switches_ = 0;
    int last_max_batch_sprites_ = 0;
    int last_sprite_count_ = 0;
    std::size_t callback_budget_per_frame_ = 16;
    float update_time_accumulator_ms_ = 0.0f;
    float fixed_time_accumulator_ms_ = 0.0f;
    float render_time_accumulator_ms_ = 0.0f;
    int update_samples_ = 0;
    int fixed_samples_ = 0;
    int render_samples_ = 0;
    dse::runtime::RenderPipelineResources render_resources_;

    /// 渲染 Pass 共享上下文
    dse::render::RenderPassContext render_pass_context_;

    /// 已注册的渲染 Pass（按注册顺序，DAG 排序由 RenderGraph 决定）
    std::vector<std::unique_ptr<dse::render::IRenderPass>> registered_passes_;
};

#endif

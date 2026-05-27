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
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/scene/transform_system.h"
#include "engine/physics/physics2d/physics2d_system.h"
#ifdef DSE_ENABLE_3D
  #include "engine/physics/physics3d/i_physics3d_system.h"
#endif
#ifdef DSE_ENABLE_NAVMESH
#include "engine/navigation/nav_mesh_system.h"
#endif
#include "engine/audio/audio_system.h"
#include "engine/core/module.h"
#include "engine/core/dynamic_library.h"
#include "engine/runtime/runtime_frame_ops.h"
#include "engine/runtime/runtime_update_graph.h"
#include "engine/runtime/runtime_render_shell.h"
#include "engine/runtime/render_pipeline_resources.h"
#include "engine/runtime/runtime_context.h"
#include "engine/runtime/business_runtime_bridge.h"
#include "engine/render/render_graph.h"
#include "engine/render/pipeline/render_pipeline_profile.h"
#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/render_snapshot.h"
#include "engine/render/passes/builtin_passes.h"
#include "engine/render/light_buffer.h"
#include "engine/render/cluster_grid.h"
#include "engine/render/light_probe_system.h"
#include "engine/render/reflection_probe_system.h"
#include "engine/render/gi/ddgi_system.h"
#include "engine/assets/streaming_manager.h"
#include "engine/ecs/floating_origin_system.h"
#include "engine/runtime/i_builtin_modules.h"
#include "engine/core/dse_export.h"

class AssetManager;

/**
 * @class FramePipeline
 * @brief 引擎的主循环流水线，负责逐帧调度与直接渲染相关初始化，不承接高层启动期副作用流程。
 */
class DSE_EXPORT FramePipeline {
public:
    enum class GpuDrivenPolicy {
        Auto,
        Off,
        Force,
        WithModules,
    };

    FramePipeline();
    ~FramePipeline();

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

    void FlushPhysicsEvents();
    
    /**
     * @brief 执行渲染图和渲染管线
     */
    void Render();

    /**
     * @brief 设置是否启用编辑器模式 (在Init前调用)
     */
    void EnableEditorMode(bool enable);

    /// Phase 2: 注入渲染线程 context 管理回调
    void SetRenderContextCallbacks(std::function<void()> make_current,
                                   std::function<void()> release,
                                   std::function<void()> present);

    /// Phase 2: 查询渲染线程是否已启动
    bool IsRenderThreadActive() const { return render_thread_active_.load(); }

    /// Reset the Physics3D system (release all PhysX actors from play-mode registry).
    /// Call before restoring the edit-mode registry snapshot on Play→Stop transition.
    /// No-op when Physics3D is not compiled in or not initialized.
    void ResetPhysics3D();

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
    int LastGpuDrivenActive() const;
    int LastGpuIndirectDrawCount() const;
    int LastGpuTotalInstances() const;

    /// 将当前帧提交到显示器（DX11/Vulkan 交换链 Present）
    /// 在 Tick() 之后、render 计时之外调用
    void PresentFrame() {
        if (runtime_context_.rhi_device) runtime_context_.rhi_device->PresentFrame();
    }

    /**
     * @brief 获取上一帧的 RHI 帧统计概要（供编辑器 Profiler 使用）
     */
    dse::render::RhiDevice::RhiFrameStats GetRhiFrameStats() const;

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

    void SetQuitCallback(std::function<void()> cb);
    void SetTargetFpsCallbacks(std::function<void(float)> setter, std::function<float()> getter);
    void SetInitKeepAlive(std::function<void()> cb);

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

    void SetEditorBgColor(const glm::vec4& color);

    /**
     * @brief 禁用编辑器相机覆盖，恢复使用游戏相机
     */
    void DisableEditorCamera();

    /**
     * @brief 设置编辑器场景视图模式 (0=Shaded, 1=Wireframe, 2=ShadedWireframe, 3=Unlit, 4=Overdraw)
     */
    void SetSceneViewMode(int mode);

    /**
     * @brief 用指定相机重新渲染场景到已有 scene FBO，返回场景纹理 ID
     *
     * 用于多视口：设置临时相机 → 执行 ForwardScenePass → 返回纹理。
     * 调用者应在主 Render() 完成后调用此方法。
     * 注意：每次调用会覆盖 scene_render_target 的内容。
     */
    unsigned int RenderSceneWithCamera(const glm::mat4& view, const glm::mat4& projection);

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

    RenderTargetReadback ReadBloomMip0Rgba8WithSize() const;
    RenderTargetReadback ReadBloomExtractRgba8WithSize() const;

    /// RHI 后端是否需要回读时 Y 翻转（OpenGL 需要，D3D11/Vulkan 不需要）
    bool NeedsReadbackYFlip() const {
        return runtime_context_.rhi_device ? runtime_context_.rhi_device->NeedsReadbackYFlip() : true;
    }

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
    void BuildRenderSceneQueues();

    /// Phase 2: 渲染线程分离
    void PrepareRenderFrame();           ///< 主线程：收集光源/构建 cluster/捕获快照 (纯 CPU)
    void ExecuteRenderFrame();           ///< 渲染线程：上传/执行 render graph/提交 (全 GPU)
    void StartRenderThread();            ///< Init 末尾启动渲染线程
    void StopRenderThread();             ///< Shutdown 时停止渲染线程
    void RenderThreadFunc();             ///< 渲染线程主循环
    void WaitForRenderComplete();        ///< 主线程等待上一帧渲染完成
    void SignalRenderThread();           ///< 主线程唤醒渲染线程开始新帧

    void BuildRenderGraph();
    void ExecuteRenderGraph(CommandBuffer& cmd_buffer);


    std::function<void()> init_keep_alive_;
    void KeepAlive() { if (init_keep_alive_) init_keep_alive_(); }

    dse::runtime::RuntimeContext runtime_context_{};
    
    std::unique_ptr<IBuiltinModules> modules_impl_;
    int gpu_culled_last_frame_ = 0;  ///< GPU Driven: 上一帧被剔除的 draw command 数
    GpuDrivenPolicy gpu_driven_policy_ = GpuDrivenPolicy::Auto;
    bool gpu_driven_requested_ = true;
    bool gpu_driven_diag_ = false;
    int last_gpu_driven_active_ = 0;
    int last_gpu_indirect_draw_count_ = 0;
    int last_gpu_total_instances_ = 0;
#ifdef DSE_ENABLE_3D
    std::shared_ptr<dse::physics3d::IPhysics3DSystem> physics3d_system_;
#endif
#ifdef DSE_ENABLE_NAVMESH
    dse::navigation::NavMeshSystem nav_mesh_system_;
    bool nav_mesh_system_initialized_ = false;
#endif
    bool builtin_gameplay3d_enabled_ = false;
    
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
    dse::render::RenderPipelineProfile render_pipeline_profile_;
    dse::render::RenderScene render_scene_;

    /// Transform 系统：每帧渲染前更新 dirty 的 local_to_world
    TransformSystem transform_system_;

    /// Clustered Forward+ 光源缓冲（SSBO）
    dse::render::LightBuffer light_buffer_;
    dse::render::ClusterGrid cluster_grid_;

    /// Light Probe SH Bake 系统
    dse::render::LightProbeSystem light_probe_system_;

    /// Reflection Probe + IBL 系统
    dse::render::ReflectionProbeSystem reflection_probe_system_;

    /// DDGI Irradiance Probe 系统
    dse::render::gi::DDGISystem ddgi_system_;

    /// Floating Origin 系统（大世界坐标 Phase 2）
    dse::FloatingOriginSystem floating_origin_system_;

    /// TAA 帧计数器（跨帧 jitter 序列）
    int taa_frame_index_ = 0;

    /// TAA Pass 弱引用（注册后从 registered_passes_ 中查找）
    dse::render::TAAPass* taa_pass_ = nullptr;

    /// 资源流式加载管理器
    dse::streaming::StreamingManager streaming_manager_;

    /// 渲染 Pass 共享上下文
    dse::render::RenderPassContext render_pass_context_;

    /// 双缓冲薄快照池（Phase 1 流水线并行基础设施）
    dse::render::RenderThinSnapshot snapshot_pool_[2];
    int snapshot_write_idx_ = 0;

    dse::render::RenderThinSnapshot& write_snapshot() { return snapshot_pool_[snapshot_write_idx_]; }
    const dse::render::RenderThinSnapshot& read_snapshot() const { return snapshot_pool_[1 - snapshot_write_idx_]; }
    void CaptureThinSnapshot();
    void FlipSnapshotIndex() { snapshot_write_idx_ = 1 - snapshot_write_idx_; }

    /// Phase 2: 渲染线程同步原语
    std::thread render_thread_;
    std::mutex render_mutex_;
    std::condition_variable render_cv_;      ///< 渲染线程等待新帧信号
    std::condition_variable main_cv_;        ///< 主线程等待渲染完成
    bool render_frame_pending_ = false;      ///< 有新帧待渲染
    bool render_frame_done_ = true;          ///< 上一帧渲染已完成
    bool render_thread_exit_ = false;        ///< 退出信号
    std::atomic<bool> render_thread_active_{false};  ///< 渲染线程是否已启动

    /// 已注册的渲染 Pass（按注册顺序，DAG 排序由 RenderGraph 决定）
    std::vector<std::unique_ptr<dse::render::IRenderPass>> registered_passes_;
};

#endif

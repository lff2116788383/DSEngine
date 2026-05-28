/**
 * @file render_pass_context.h
 * @brief 渲染 Pass 共享上下文，提供对运行时资源的只读访问
 */

#ifndef DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H
#define DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H

#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/render_scene.h"
#include "engine/render/render_snapshot.h"

class World;
class AssetManager;

namespace dse {
namespace core {
class IModule;
} // namespace core
} // namespace dse

namespace dse {
namespace render {

class RhiDevice;
class CommandBuffer;

class LightBuffer;
class ClusterGrid;

namespace gi {
class DDGISystem;
} // namespace gi

/**
 * @struct RenderPassContext
 * @brief 所有 Pass 共享的运行时上下文（非拥有型指针）
 */
struct RenderPassContext {
    const RenderThinSnapshot* snapshot = nullptr;  ///< 渲染线程只读快照（Phase 1）
    glm::vec3 camera_offset{0.0f};                ///< Camera-Relative Rendering: model matrix 减去此偏移后传 GPU
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
    RhiDevice* rhi_device = nullptr;
    RenderScene* render_scene = nullptr;
    LightBuffer* light_buffer = nullptr;
    ClusterGrid* cluster_grid = nullptr;
    bool editor_mode = false;
    glm::vec4 editor_bg_color = glm::vec4(0.17f, 0.17f, 0.21f, 1.0f); ///< 编辑器场景清屏色（可由主题切换覆盖）

    /// 引擎内置系统的访问（通过 FramePipeline 注入）
    struct PipelineState {
        unsigned int sprite = 0;
        unsigned int mesh = 0;
        unsigned int prez = 0;
        unsigned int shadow = 0;
        unsigned int composite = 0;
        unsigned int decal_blend = 0;
        unsigned int wboit_accum = 0;
        unsigned int wboit_reveal = 0;
    } pipeline_states;

    struct RenderTargets {
        unsigned int main = 0;
        unsigned int scene = 0;
        unsigned int ui = 0;
        unsigned int prez = 0;
        unsigned int shadow[3] = {0, 0, 0};     // CSM_CASCADES (legacy, kept for spot/point)
        unsigned int shadow_atlas = 0;           // CSM shadow atlas RT
        unsigned int spot_shadow[4] = {0, 0, 0, 0};
        unsigned int point_shadow[4] = {0, 0, 0, 0};
        unsigned int bloom_extract = 0;
        std::vector<unsigned int> bloom_mips;
        unsigned int ssao = 0;
        unsigned int ssao_blur = 0;
        unsigned int contact_shadow = 0;
        unsigned int fxaa = 0;
        unsigned int taa = 0;               // TAA resolve 输出 RT
        unsigned int dof = 0;               // DOF 输出 RT
        unsigned int ssr = 0;               // SSR 输出 RT
        unsigned int motion_vector = 0;     // Motion Vector RT (RG16F)
        unsigned int outline = 0;            // Outline / Edge Detection RT
        unsigned int fog = 0;               // Volumetric Fog RT
        unsigned int cloud = 0;             // Volumetric Cloud RT
        unsigned int wboit_accum = 0;        // WBOIT accumulation RT (RGBA16F)
        unsigned int wboit_reveal = 0;       // WBOIT revealage RT (RGBA16F)
        unsigned int gbuffer = 0;           // GBuffer MRT (3 color + depth)
        unsigned int deferred_lighting = 0; // Deferred lighting output RT
        unsigned int lum_temp = 0;          // 64x64 log luminance
        unsigned int lum_adapted[2] = {0,0}; // 1x1 ping-pong
        unsigned int hiz_texture = 0;       // Hi-Z depth mipmap (R32F, RHI handle)
        unsigned int sss_temp = 0;          // Separable SSS blur intermediate RT (RGBA16F)
    } render_targets;

    /// Hi-Z Occlusion Culling 状态
    BufferHandle hiz_visibility_ssbo;      ///< 可见性 SSBO（每个 mesh 1 uint: 1=visible, 0=occluded）
    BufferHandle hiz_aabb_ssbo;            ///< AABB SSBO（每个 mesh 6 floats: min_xyz, max_xyz）
    size_t hiz_aabb_capacity = 0;          ///< hiz_aabb_ssbo 分配容量（对象数），0 表示未知
    int hiz_object_count = 0;               ///< 当前帧 mesh 数量
    bool hiz_culling_enabled = false;       ///< Hi-Z 剔除是否激活
    unsigned int hiz_copy_shader = 0;       ///< Compute: depth → Hi-Z mip 0（由 FramePipeline 管理生命周期）
    unsigned int hiz_downsample_shader = 0; ///< Compute: mip N-1 → mip N
    unsigned int hiz_cull_shader = 0;       ///< Compute: AABB 遮挡剔除

    /// GPU Driven 渲染状态
    bool gpu_driven_enabled = false;              ///< 兼容字段：后端能力支持，勿作为本帧激活判定
    bool gpu_driven_supported = false;            ///< RHI 能力与 shader 均支持 GPU Driven
    bool gpu_driven_requested = false;            ///< 配置/策略请求启用 GPU Driven
    bool gpu_driven_scene_prepared = false;       ///< 本帧是否成功准备 GPU Scene
    bool gpu_driven_active_this_frame = false;    ///< 本帧是否存在可绘制 GPU Driven draw command
    BufferHandle gpu_indirect_buffer;            ///< indirect draw argument buffer
    BufferHandle gpu_instance_ssbo;              ///< GPUInstanceData[] SSBO
    BufferHandle gpu_material_ssbo;              ///< GPUMaterialData[] SSBO
    BufferHandle gpu_draw_cmd_ssbo;              ///< DrawCommands as SSBO (compute write)
    BufferHandle gpu_aabb_ssbo;
    BufferHandle gpu_visible_indices_ssbo;       ///< visible instance index buffer SSBO
    BufferHandle gpu_atomic_counter_ssbo;        ///< atomic draw count SSBO
    VertexArrayHandle gpu_mega_vao;                ///< mega buffer VAO
    unsigned int gpu_cull_shader = 0;             ///< GPU Driven culling compute shader
    int gpu_indirect_draw_count = 0;              ///< indirect draw command 条数
    int gpu_total_instances = 0;                  ///< 本帧总 instance 数
    size_t gpu_aabb_capacity = 0;
    const TextureBucket* gpu_texture_buckets = nullptr; ///< Phase 5: 纹理分桶数组
    int gpu_texture_bucket_count = 0;             ///< 纹理桶数量
    const GPUMaterialData* gpu_materials = nullptr; ///< CPU 侧 GPUMaterialData 数组（per-bucket PerMaterial 更新用）
    int gpu_material_count = 0;                   ///< GPUMaterialData 元素数

    /// 帧级缓存标志（由各 Pass 写入，后续 Pass 读取，避免重复 ECS 查询）
    bool fxaa_active = false;
    bool taa_active = false;

    struct PipelineFeatures {
        bool bloom = true;
        bool ssao = true;
        bool contact_shadow = true;
        bool auto_exposure = true;
        bool fxaa = true;
        bool taa = true;
        bool ui = true;
        bool gpu_cull = true;
        bool shadows = true;
    } pipeline_features;

    struct PipelineOverrides {
        float bloom_intensity = -1.0f;
        float bloom_threshold = -1.0f;
    } pipeline_overrides;

    /// TAA jitter（每帧由 TAAPass 更新，注入到投影矩阵）
    glm::vec2 taa_jitter = {};

    /// Auto Exposure 帧状态
    int lum_ping_pong_index = 0;          // 当前帧写入哪个 1x1 RT (0 or 1)
    float delta_time = 0.016f;            // 帧间隔（用于 EMA 平滑）
    bool auto_exposure_active = false;    // 本帧 auto exposure 是否启用

    /// 已加载的动态模块实例列表
    struct ModuleRef {
        dse::core::IModule* instance = nullptr;
    };
    std::vector<ModuleRef> modules;

    /// 编辑器相机覆盖（编辑器模式下 Scene 视图使用编辑器相机替代游戏相机）
    bool use_editor_camera = false;
    glm::mat4 editor_view = glm::mat4(1.0f);
    glm::mat4 editor_projection = glm::mat4(1.0f);

    /// 编辑器场景视图模式 (0=Shaded, 1=Wireframe, 2=ShadedWireframe, 3=Unlit, 4=Overdraw)
    int scene_view_mode = 0;

    /// FramePipeline 拥有的子系统回调（避免 Pass 直接依赖 FramePipeline）
    std::function<void(World&, CommandBuffer&)> render_2d_scene;
    std::function<void(World&, CommandBuffer&, int, int, const glm::mat4&)> render_2d_ui;
    std::function<void(World&, CommandBuffer&)> render_meshes;
    std::function<void(World&, CommandBuffer&, int wboit_mode)> render_transparent_meshes;

    /// DDGI 系统（FramePipeline 持有生命周期，Pass 通过指针访问）
    gi::DDGISystem* ddgi_system = nullptr;

    /// RSM (Reflective Shadow Map) 渲染目标
    struct RSMRenderTargets {
        unsigned int position = 0;    ///< 世界坐标 RT (RGBA32F)
        unsigned int normal = 0;      ///< 法线 RT (RGBA16F)
        unsigned int flux = 0;        ///< 辐射通量 RT (RGBA16F)
        int width = 0;
        int height = 0;
    } rsm_targets;

    unsigned int rsm_render_target = 0;  ///< RSM MRT FBO handle（供 RSMRenderPass 使用）

    /// DDGI 探针 atlas 纹理（供 PBR shader 采样）
    unsigned int ddgi_irradiance_atlas = 0;
    unsigned int ddgi_visibility_atlas = 0;
    bool ddgi_active = false;
    float ddgi_gi_intensity = 1.0f;
    float ddgi_normal_bias = 0.2f;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H

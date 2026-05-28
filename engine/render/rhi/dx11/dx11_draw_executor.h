/**
 * @file dx11_draw_executor.h
 * @brief D3D11 绘制执行器 — 负责执行所有渲染命令和绘制调用
 *
 * 对标 VulkanDrawExecutor / GLDrawExecutor：
 * - 2D 精灵批处理绘制
 * - 3D PBR 网格绘制
 * - 天空盒绘制
 * - 后处理绘制
 * - 3D 粒子绘制
 * - 全局阴影贴图/光源矩阵管理
 * - 常量缓冲管理（PerFrame/PerScene/PerMaterial）
 */

#ifndef DSE_RENDER_DX11_DRAW_EXECUTOR_H
#define DSE_RENDER_DX11_DRAW_EXECUTOR_H

#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/draw_executor_common.h"
#include "engine/render/rhi/postprocess_common.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

class DX11Context;
class DX11ResourceManager;
class DX11PipelineStateManager;
class DX11ShaderManager;

/// PerFrame 常量缓冲数据
struct DX11PerFrameCB {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;     ///< xyz=position, w=global_wetness
    glm::vec4 foliage_wind;   ///< x=time, y=strength, z=wind_dir_x, w=wind_dir_z
    glm::vec4 foliage_push;   ///< xyz=character_pos, w=push_radius
};

/// PerObject 常量缓冲数据
struct DX11PerObjectCB {
    glm::mat4 model;
    int skinned;
    int morph_enabled;
    int bone_offset;
    int foliage;
};

/// PerScene 常量缓冲数据
struct DX11PerSceneCB {
    glm::vec4 light_dir_and_enabled;
    glm::vec4 light_color_and_ambient;
    glm::vec4 light_params;
    glm::vec4 cascade_splits;
    glm::mat4 light_space_matrices[3];
    glm::vec4 shadow_atlas_regions[3];  ///< per-cascade atlas region: xy=UV scale, zw=UV offset
};

/// PerMaterial 常量缓冲数据
struct DX11PerMaterialCB {
    glm::vec4 albedo;
    glm::vec4 roughness_ao;
    glm::vec4 emissive;
    glm::vec4 flags;
    glm::vec4 extra_params;  ///< x=sss_strength, y=clear_coat, z=clear_coat_roughness, w=anisotropy
    glm::vec4 extra_params2; ///< x=pom_height_scale, y/z/w=sss_tint RGB
    glm::vec4 toon_shadow_color; ///< xyz=shadow tint, w=shadow_threshold
    glm::vec4 toon_params;       ///< x=shadow_softness, y=specular_size, z=specular_strength, w=rim_strength
};

/// PointLight 单条目（每条 48B，3 × 16B，与 kPbrPS b4 对齐）
struct DX11PointLightEntry {
    glm::vec3 color;     float intensity;
    glm::vec3 position;  float radius;
    int cast_shadow;     int shadow_index;
    int _pad0;           int _pad1;
};
static_assert(sizeof(DX11PointLightEntry) % 16 == 0,
              "DX11PointLightEntry must be 16B aligned");

/// PointLights 常量缓冲
struct DX11PointLightsCB {
    int count;
    int _p0, _p1, _p2;
    DX11PointLightEntry lights[64];
};
static_assert(sizeof(DX11PointLightsCB) % 16 == 0,
              "DX11PointLightsCB must be 16B aligned");

/// SpotLight 单条目（每条 64B，4 × 16B）
struct DX11SpotLightEntry {
    glm::vec3 color;       float intensity;
    glm::vec3 position;    float radius;
    glm::vec3 direction;   float inner_cone;
    float outer_cone;      int cast_shadow;
    int shadow_index;      float _pad0;
};
static_assert(sizeof(DX11SpotLightEntry) % 16 == 0,
              "DX11SpotLightEntry must be 16B aligned");

/// SpotLights 常量缓冲
struct DX11SpotLightsCB {
    int count;
    int _p0, _p1, _p2;
    DX11SpotLightEntry lights[64];
};
static_assert(sizeof(DX11SpotLightsCB) % 16 == 0,
              "DX11SpotLightsCB must be 16B aligned");

/// 聚光灯光源空间矩阵（4×64B = 256B）
struct DX11SpotMatricesCB {
    glm::mat4 spot_light_space_matrices[4];
};
static_assert(sizeof(DX11SpotMatricesCB) % 16 == 0,
              "DX11SpotMatricesCB must be 16B aligned");

/// LightProbeData 常量缓冲（160B）
struct DX11LightProbeDataCB {
    glm::vec4 sh_coefficients[9];
    glm::vec4 probe_params;
};
static_assert(sizeof(DX11LightProbeDataCB) % 16 == 0,
              "DX11LightProbeDataCB must be 16B aligned");

struct DX11TerrainParamsCB {
    glm::vec4 flags;       // x=splat_enabled, y=snow_coverage, z=snow_normal_threshold, w=snow_edge_sharpness
    glm::vec4 tiling;
    glm::vec4 snow_params; // xyz=snow_albedo, w=snow_roughness
};
static_assert(sizeof(DX11TerrainParamsCB) % 16 == 0,
              "DX11TerrainParamsCB must be 16B aligned");

/**
 * @class DX11DrawExecutor
 * @brief D3D11 绘制执行器
 */
class DX11DrawExecutor {
public:
    explicit DX11DrawExecutor(DrawExecutorGlobalState& shared_state)
        : global_state_(shared_state) {}
    ~DX11DrawExecutor() = default;

    /// 初始化常量缓冲和几何缓冲
    void Init(DX11Context* context, DX11ResourceManager* resource_mgr);

    /// 清理
    void Shutdown();

    // --- RenderPass ---
    void BeginRenderPass(const RenderPassDesc& render_pass,
                          DX11ResourceManager& resource_mgr,
                          DX11PipelineStateManager& pipeline_mgr);
    void EndRenderPass();

    // --- 绘制命令 ---
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items,
                          const glm::mat4& view, const glm::mat4& projection,
                          DX11PipelineStateManager& pipeline_mgr,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    void DrawMeshBatch(const std::vector<MeshDrawItem>& items,
                        const glm::mat4& view, const glm::mat4& projection,
                        DX11PipelineStateManager& pipeline_mgr,
                        DX11ShaderManager& shader_mgr,
                        DX11ResourceManager& resource_mgr);

    void DrawSkybox(unsigned int cubemap_texture_handle,
                     const glm::mat4& view, const glm::mat4& projection,
                     DX11PipelineStateManager& pipeline_mgr,
                     DX11ShaderManager& shader_mgr,
                     DX11ResourceManager& resource_mgr);

    void DrawPostProcess(const PostProcessRequest& request,
                          DX11PipelineStateManager& pipeline_mgr,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items,
                          const glm::mat4& view, const glm::mat4& projection,
                          DX11PipelineStateManager& pipeline_mgr,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    void DrawHairStrands(const std::vector<HairDrawItem>& items,
                          const glm::mat4& view, const glm::mat4& projection,
                          DX11PipelineStateManager& pipeline_mgr,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    void DispatchCompute(unsigned int cs_handle,
                          unsigned int srv_texture_handle,
                          unsigned int uav_rt_handle,
                          UINT threads_x, UINT threads_y,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    // --- GPU-Driven PBR 渲染设置 ---
    void SetupGPUDrivenPBR(const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& camera_pos,
                            const glm::vec3& light_dir, const glm::vec3& light_color,
                            float light_intensity, float ambient_intensity,
                            float shadow_strength,
                            DX11PipelineStateManager& pipeline_mgr,
                            DX11ShaderManager& shader_mgr);

    // --- GPU-Driven Shadow 渲染设置 ---
    void SetupGPUDrivenShadow(const glm::mat4& light_view, const glm::mat4& light_proj,
                               DX11PipelineStateManager& pipeline_mgr,
                               DX11ShaderManager& shader_mgr);

    /// 更新 per-object 常量缓冲（GPU-Driven per-draw model 更新用）
    void UpdatePerObjectCB(const DX11PerObjectCB& data);

    /// 更新 GPU-driven draw index（cbuffer b7）
    void UpdateDrawId(uint32_t draw_id);

    /// 更新 GPU-driven per-bucket PerMaterial cbuffer (b3)
    void UpdateGPUDrivenMaterial(const void* mat_data);

    /// GPU-Driven 纹理 fallback
    ID3D11ShaderResourceView* white_srv() const { return white_texture_srv_.Get(); }
    ID3D11SamplerState* white_sampler() const { return white_texture_sampler_.Get(); }

    // --- 渲染统计 ---
    void BeginFrame();
    void EndFrame();
    const RenderStats& current_frame_stats() const { return global_state_.current_frame_stats; }

private:
    /// 创建常量缓冲
    ComPtr<ID3D11Buffer> CreateConstantBuffer(UINT size);

    /// 更新常量缓冲数据
    void UpdateConstantBuffer(ID3D11Buffer* buffer, const void* data, UINT size);

    /// 初始化几何缓冲（精灵/天空盒/后处理/粒子）
    void InitGeometryBuffers();

    /// 动态 VBO/IBO 容量保证
    void EnsureMeshVBOCapacity(size_t needed_bytes);
    void EnsureMeshIBOCapacity(size_t needed_bytes);
    void EnsureInstanceVBOCapacity(size_t needed_bytes);

    DX11Context* context_ = nullptr;
    DX11ResourceManager* resource_mgr_ = nullptr;

    // 常量缓冲
    ComPtr<ID3D11Buffer> per_frame_cb_;
    ComPtr<ID3D11Buffer> per_object_cb_;
    ComPtr<ID3D11Buffer> per_scene_cb_;
    ComPtr<ID3D11Buffer> per_material_cb_;
    ComPtr<ID3D11Buffer> draw_id_cb_;       ///< GPU-driven draw index (register b7)

    // 全局渲染状态（引用 RhiDevice::global_render_state_）
    DrawExecutorGlobalState& global_state_;

    // --- 几何缓冲 ---
    // 精灵四边形（动态 VBO，静态 IBO）
    ComPtr<ID3D11Buffer> sprite_quad_vbo_;
    ComPtr<ID3D11Buffer> sprite_quad_ibo_;
    ComPtr<ID3D11Buffer> sprite_push_cb_;   // 128B: [model(64B) | vp(64B)] for sprite.vert
    ComPtr<ID3D11Buffer> sdf_ps_cb_;        // 144B: [model(64B) | vp(64B) | sdf_params(16B)] for text_sdf.frag
    ComPtr<ID3D11Buffer> vfx_ps_cb_;        // 64B: [gradient_start(16) | gradient_end(16) | rect_size_and_radius(16) | blur_params(16)] for ui_effects.frag

    // 网格（动态，按需扩容）
    ComPtr<ID3D11Buffer> mesh_dynamic_vbo_;
    ComPtr<ID3D11Buffer> mesh_dynamic_ibo_;
    size_t mesh_vbo_capacity_ = 0;
    size_t mesh_ibo_capacity_ = 0;

    // GPU Instancing: instance model matrix VBO
    ComPtr<ID3D11Buffer> instance_vbo_;
    size_t instance_vbo_capacity_ = 0;

    // 天空盒立方体（静态）
    ComPtr<ID3D11Buffer> skybox_vbo_;

    // 后处理全屏四边形（静态）
    ComPtr<ID3D11Buffer> postprocess_vbo_;
    ComPtr<ID3D11Buffer> postprocess_ibo_;

    // Bloom 合成参数 CB（exposure + bloomIntensity）
    ComPtr<ID3D11Buffer> bloom_composite_params_cb_;

    // 通用后处理参数 CB（32B，各效果共用）
    ComPtr<ID3D11Buffer> pp_params_cb_;

    // Motion Blur 参数 CB（160B，含两个 mat4）
    ComPtr<ID3D11Buffer> mb_params_cb_;

    // 粒子公告板四边形（静态 VBO+IBO）
    ComPtr<ID3D11Buffer> particle_quad_vbo_;
    ComPtr<ID3D11Buffer> particle_quad_ibo_;

    // 阴影 pass 检测（深度 only 渲染目标 = shadow pass）
    bool is_depth_only_pass_ = false;

    // 当前渲染目标句柄（MSAA resolve 使用）
    unsigned int current_rt_handle_ = 0;

    // 阴影采样器（用于 PBR pass 采样 shadow map）
    ComPtr<ID3D11SamplerState> shadow_sampler_;

    // 1×1 白色 fallback 纹理（texture_handle=0 时使用，与 OpenGL 行为一致）
    ComPtr<ID3D11ShaderResourceView> white_texture_srv_;
    ComPtr<ID3D11SamplerState> white_texture_sampler_;

    // 天空盒深度状态（LEQUAL + no depth write）
    ComPtr<ID3D11DepthStencilState> skybox_dss_;

    // 双面材质光栅化状态（CullMode=NONE, 与 OpenGL/Vulkan 的 material_double_sided 对齐）
    ComPtr<ID3D11RasterizerState> no_cull_rasterizer_state_;

    // 点光源 / 聚光灯常量缓冲
    ComPtr<ID3D11Buffer> per_point_lights_cb_;
    ComPtr<ID3D11Buffer> per_spot_lights_cb_;
    ComPtr<ID3D11Buffer> per_spot_matrices_cb_;
    ComPtr<ID3D11Buffer> terrain_params_cb_;

    // 骨骼矩阵常量缓冲（旧，保留兼容）
    ComPtr<ID3D11Buffer> bone_matrices_cb_;

    // Bone SSBO: ByteAddressBuffer for all instances' bone matrices (t24)
    ComPtr<ID3D11Buffer> bone_ssbo_buf_;
    ComPtr<ID3D11ShaderResourceView> bone_ssbo_srv_;
    size_t bone_ssbo_capacity_ = 0;
    bool bone_ssbo_uploaded_this_frame_ = false;

    // Skinned Instance SSBO: pre-packed ByteAddressBuffer for all skinned instanced items (t26)
    ComPtr<ID3D11Buffer> skinned_inst_buf_;
    ComPtr<ID3D11ShaderResourceView> skinned_inst_srv_;  // whole-buffer SRV (kept for fallback)
    size_t skinned_inst_capacity_ = 0;
    bool inst_ssbo_uploaded_this_frame_ = false;

    // Static mesh VBO/IBO cache: persistent IMMUTABLE buffers keyed by shared_vertex_ptr
    struct DX11StaticMeshEntry {
        ComPtr<ID3D11Buffer> vbo;
        ComPtr<ID3D11Buffer> ibo;
        size_t vtx_count;
        size_t idx_count;
    };
    std::unordered_map<const void*, DX11StaticMeshEntry> static_mesh_cache_;

    // Light Probe SH 常量缓冲（b9, 160B）
    ComPtr<ID3D11Buffer> light_probe_data_cb_;

    // --- Hair rendering ---
    struct HairVSCB {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
        glm::vec3 camera_pos;
        float _pad0;
    };
    static_assert(sizeof(HairVSCB) == 208, "HairVSCB must be 208 bytes");

    struct HairPSCB {
        glm::vec3 light_dir;
        float light_intensity;
        glm::vec3 light_color;
        float ambient_intensity;
        glm::vec4 root_color;
        glm::vec4 tip_color;
        float opacity;
        float spec_primary;
        float spec_secondary;
        float spec_strength1;
        float spec_strength2;
        glm::vec3 spec_color;
    };
    static_assert(sizeof(HairPSCB) % 16 == 0, "HairPSCB must be 16B aligned");

    unsigned int hair_shader_handle_ = 0;
    ComPtr<ID3D11Buffer> hair_vs_cb_;
    ComPtr<ID3D11Buffer> hair_ps_cb_;
    ComPtr<ID3D11BlendState> hair_blend_state_;
    ComPtr<ID3D11DepthStencilState> hair_depth_state_;
    ComPtr<ID3D11RasterizerState> hair_raster_state_;

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_DRAW_EXECUTOR_H

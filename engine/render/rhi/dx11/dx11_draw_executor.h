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
#include <map>
#include <string>
#include <unordered_map>

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

class DX11Context;
class DX11ResourceManager;
class DX11PipelineStateManager;
class DX11ShaderManager;

/// PerFrame 常量缓冲数据（布局与共享 PerFrameUBO 一致，直接复用）
using DX11PerFrameCB = PerFrameUBO;

/// PerObject 常量缓冲数据
struct DX11PerObjectCB {
    glm::mat4 model;
    int skinned;
    int morph_enabled;
    int bone_offset;
    int foliage;
};

/// PerScene 常量缓冲数据（布局与共享 PerSceneUBO 一致，直接复用）
using DX11PerSceneCB = PerSceneUBO;

/// PerMaterial 常量缓冲数据（布局与共享 PerMaterialUBO 一致，直接复用）
using DX11PerMaterialCB = PerMaterialUBO;

/// PointLights 常量缓冲（布局与共享 PointLightsUBO 一致，每条 48B，与 kPbrPS b4 对齐）
using DX11PointLightsCB = PointLightsUBO;

/// SpotLights 常量缓冲（布局与共享 SpotLightsUBO 一致，每条 64B）
using DX11SpotLightsCB = SpotLightsUBO;

/// 聚光灯光源空间矩阵（4×64B = 256B，布局与共享 SpotLightDataUBO 一致）
using DX11SpotMatricesCB = SpotLightDataUBO;

/// LightProbeData 常量缓冲（160B，布局与共享 LightProbeDataUBO 一致）
using DX11LightProbeDataCB = LightProbeDataUBO;

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

    // CommandBuffer 级 compute 原语：输入 dispatch.source_texture，输出当前绑定 RT 的 UAV。
    // 复刻原 bloom CS 路径（解绑 RTV → CSSetUAV → Dispatch → 重绑 RTV）。
    void DispatchComputePass(const ComputeDispatch& dispatch,
                             DX11ShaderManager& shader_mgr,
                             DX11ResourceManager& resource_mgr);

    // --- 通用绘制原语 (A1) ---
    // Bind* 仅暂存累积状态；PrimDraw 时组装 shader/cbuffer/纹理/VB 并发出 draw。
    // 深度/光栅/混合由 BindPipeline→ApplyPipelineState 设定，PrimDraw 不再 save/restore。
    /// 设置后续 Prim* 绘制的图元拓扑（由 BindPipeline 从 PSO desc 推送）。
    /// 默认 TriangleList；毛发等线状几何用 LineStrip/LineList。
    void PrimSetTopology(PrimitiveTopology topology);
    void PrimBindShaderProgram(unsigned int program_handle);
    void PrimBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                              const std::vector<VertexAttr>& attrs,
                              VertexInputRate rate = VertexInputRate::PerVertex);
    void PrimPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size);
    void PrimDraw(uint32_t vertex_count, uint32_t first_vertex,
                  DX11ShaderManager& shader_mgr,
                  DX11ResourceManager& resource_mgr);

    // --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---
    void PrimBindIndexBuffer(unsigned int buffer_handle, IndexType type);
    void PrimBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim);
    void PrimBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                               uint32_t offset, uint32_t size);
    void PrimBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                               uint32_t offset, uint32_t size);
    void PrimDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex,
                         DX11ShaderManager& shader_mgr,
                         DX11ResourceManager& resource_mgr);

    // --- 通用绘制原语 (B2b 前置): 实例化索引绘制 ---
    void PrimDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                  uint32_t first_index, int32_t base_vertex, uint32_t first_instance,
                                  DX11ShaderManager& shader_mgr,
                                  DX11ResourceManager& resource_mgr);

    // --- 通用绘制原语 (B2b-5): GPU-driven 间接索引绘制 ---
    void PrimDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset,
                                 DX11ShaderManager& shader_mgr,
                                 DX11ResourceManager& resource_mgr);

    void DispatchCompute(unsigned int cs_handle,
                          unsigned int srv_texture_handle,
                          unsigned int uav_rt_handle,
                          UINT threads_x, UINT threads_y,
                          float blend_weight,
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
    /// 解析本次绘制的 InputLayout：无 slot>0 且 slot0 为 per-vertex 时沿用反射布局；
    /// 否则据各 slot 属性 + rate 显式组装多 slot 布局（GetOrCreatePrimInputLayout）。
    ID3D11InputLayout* ResolvePrimInputLayout(DX11ShaderManager& shader_mgr) const;
    /// 绑定各 slot 顶点缓冲：slot 0（旧路径）+ prim_extra_vbs_ 各附加 slot。
    void BindPrimVertexBuffers(ID3D11DeviceContext* dc, DX11ResourceManager& resource_mgr) const;
    /// 绘制后清除 slot>0 的瞬态顶点流绑定（slot 0 保持旧语义）。
    void ClearExtraVertexSlots() { prim_extra_vbs_.clear(); }

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
    ComPtr<ID3D11Buffer> prim_push_cb_;     // 通用 push cbuffer（b0），承载 PushConstants 字节块
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

    // 通用绘制原语 (A1) 累积状态：Bind* 暂存，PrimDraw 时组装并发出 draw
    unsigned int prim_program_handle_ = 0;   ///< 当前绑定的着色器句柄
    unsigned int prim_vbo_handle_ = 0;       ///< slot 0 顶点缓冲句柄（旧单流路径，跨绘制保持）
    uint32_t prim_stride_ = 0;               ///< slot 0 顶点步长（字节）
    std::vector<VertexAttr> prim_attrs_;     ///< slot 0 顶点属性（仅 slot>0/per-instance 出现时才据此显式建布局）
    VertexInputRate prim_slot0_rate_ = VertexInputRate::PerVertex;  ///< slot 0 步进频率
    /// slot>0 的附加顶点流（slot 化兑现契约 §3 终态：如 per-instance 实例顶点流）。每次绘制后清空，
    /// 仅 slot 0 沿用旧的「跨绘制保持」语义，避免泄漏到后续 vertexless 绘制。
    struct PrimVbExtra {
        unsigned int handle = 0;
        uint32_t stride = 0;
        std::vector<VertexAttr> attrs;
        VertexInputRate rate = VertexInputRate::PerVertex;
    };
    std::map<uint32_t, PrimVbExtra> prim_extra_vbs_;
    // 通用 push constant 字节块（→ push cbuffer b0；VS/PS 按 stage 绑定）。
    // 真 UBO 在 HLSL 均显式 register(b1+)，push cbuffer 恒落 b0，故统一绑 b0 安全。
    static constexpr uint32_t kPrimPushMaxBytes = 256;
    uint8_t prim_push_data_[kPrimPushMaxBytes] = {};  ///< CPU 暂存（零填充，整块上传）
    uint32_t prim_push_stage_mask_ = 0;      ///< 已写入的阶段位掩码（ShaderStage）
    bool prim_has_push_ = false;             ///< 本次绘制是否设置过 push constant

    // B0 通用原语累积状态：索引缓冲 / 2D 纹理(slot→t/s) / UBO(slot→b)
    unsigned int prim_index_buffer_handle_ = 0;       ///< 当前绑定的索引缓冲句柄（0=无）
    DXGI_FORMAT prim_index_format_ = DXGI_FORMAT_R16_UINT;  ///< 索引格式
    std::unordered_map<uint32_t, unsigned int> prim_textures_;  ///< slot → 2D 纹理句柄
    std::unordered_map<uint32_t, unsigned int> prim_ubos_;      ///< slot → constant buffer 句柄
    struct PrimSSBOBinding { unsigned int handle = 0; uint32_t offset = 0; uint32_t size = 0; };
    std::unordered_map<uint32_t, PrimSSBOBinding> prim_ssbos_;  ///< slot → SSBO 句柄+子区间
    D3D11_PRIMITIVE_TOPOLOGY prim_topology_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;  ///< 当前 PSO 拓扑（BindPipeline 推送）

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

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_DRAW_EXECUTOR_H

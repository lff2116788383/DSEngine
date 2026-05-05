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
    glm::vec4 camera_pos;
};

/// PerObject 常量缓冲数据
struct DX11PerObjectCB {
    glm::mat4 model;
    int skinned;
    int morph_enabled;
    int _pad0;
    int _pad1;
};

/// PerScene 常量缓冲数据
struct DX11PerSceneCB {
    glm::vec4 light_dir_and_enabled;
    glm::vec4 light_color_and_ambient;
    glm::vec4 light_params;
    glm::vec4 cascade_splits;
    glm::mat4 light_space_matrices[3];
};

/// PerMaterial 常量缓冲数据
struct DX11PerMaterialCB {
    glm::vec4 albedo;
    glm::vec4 roughness_ao;
    glm::vec4 emissive;
    glm::vec4 flags;
};

/**
 * @class DX11DrawExecutor
 * @brief D3D11 绘制执行器
 */
class DX11DrawExecutor {
public:
    DX11DrawExecutor() = default;
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

    void DrawPostProcess(unsigned int source_texture,
                          const std::string& effect_name,
                          const std::vector<float>& params,
                          DX11PipelineStateManager& pipeline_mgr,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items,
                          const glm::mat4& view, const glm::mat4& projection,
                          DX11PipelineStateManager& pipeline_mgr,
                          DX11ShaderManager& shader_mgr,
                          DX11ResourceManager& resource_mgr);

    // --- 全局阴影/光源矩阵 ---
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) {
        if (index < 3) global_shadow_map_[index] = handle;
    }
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) {
        if (index < 4) global_spot_shadow_map_[index] = handle;
    }
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) {
        if (index < 4) global_point_shadow_map_[index] = handle;
    }
    void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 3) global_light_space_matrix_[index] = mat;
    }
    void SetGlobalCascadeSplit(unsigned int index, float split) {
        if (index < 3) global_cascade_splits_[index] = split;
    }
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 4) global_spot_light_space_matrix_[index] = mat;
    }

    // --- 渲染统计 ---
    void BeginFrame();
    void EndFrame();
    const RenderStats& current_frame_stats() const { return current_frame_stats_; }

private:
    /// 创建常量缓冲
    ComPtr<ID3D11Buffer> CreateConstantBuffer(UINT size);

    /// 更新常量缓冲数据
    void UpdateConstantBuffer(ID3D11Buffer* buffer, const void* data, UINT size);

    DX11Context* context_ = nullptr;
    DX11ResourceManager* resource_mgr_ = nullptr;

    // 常量缓冲
    ComPtr<ID3D11Buffer> per_frame_cb_;
    ComPtr<ID3D11Buffer> per_object_cb_;
    ComPtr<ID3D11Buffer> per_scene_cb_;
    ComPtr<ID3D11Buffer> per_material_cb_;

    // 全局阴影/光源状态
    glm::mat4 global_light_space_matrix_[3];
    glm::mat4 global_spot_light_space_matrix_[4] = {
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)
    };
    float global_cascade_splits_[3] = {};
    unsigned int global_shadow_map_[3] = {};
    unsigned int global_spot_shadow_map_[4] = {};
    unsigned int global_point_shadow_map_[4] = {};

    // 渲染统计
    RenderStats current_frame_stats_;

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_DRAW_EXECUTOR_H

/**
 * @file vulkan_draw_executor.h
 * @brief Vulkan 绘制执行器 - 负责执行所有渲染命令和绘制调用
 *
 * 对标 GLDrawExecutor，但使用 Vulkan 命令缓冲录制模式：
 * - 2D 精灵批处理绘制
 * - 3D PBR 网格绘制
 * - 天空盒绘制
 * - 后处理绘制
 * - 3D 粒子绘制
 * - RenderPass 管理
 * - 全局阴影贴图/光源矩阵管理
 */

#ifndef DSE_RENDER_VULKAN_DRAW_EXECUTOR_H
#define DSE_RENDER_VULKAN_DRAW_EXECUTOR_H

#include "engine/render/rhi/rhi_types.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace dse {
namespace render {

class VulkanContext;
class VulkanResourceManager;
class VulkanPipelineStateManager;
class VulkanShaderManager;

/**
 * @class VulkanDrawExecutor
 * @brief Vulkan 绘制执行器
 *
 * 职责：
 * 1. 管理几何缓冲区（VkBuffer/VkDeviceMemory）
 * 2. 录制各类绘制命令到 VkCommandBuffer
 * 3. 管理全局阴影贴图和光源空间矩阵
 * 4. 追踪渲染统计数据
 */
class VulkanDrawExecutor {
public:
    VulkanDrawExecutor() = default;
    ~VulkanDrawExecutor() = default;

    /// 初始化几何缓冲区
    void InitGeometryBuffers(VulkanContext* context, VulkanResourceManager* resource_mgr);

    /// 清理所有几何缓冲区资源
    void ShutdownGeometryBuffers();

    // --- RenderPass ---
    void BeginRenderPass(VkCommandBuffer cmd_buf, const RenderPassDesc& render_pass,
                          VulkanResourceManager& resource_mgr,
                          VulkanPipelineStateManager& pipeline_mgr);
    void EndRenderPass(VkCommandBuffer cmd_buf);

    // --- 绘制命令（录制到 VkCommandBuffer） ---
    void DrawSpriteBatch(VkCommandBuffer cmd_buf,
                          const std::vector<SpriteDrawItem>& items,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          VulkanPipelineStateManager& pipeline_mgr,
                          VulkanShaderManager& shader_mgr);

    void DrawMeshBatch(VkCommandBuffer cmd_buf,
                        const std::vector<MeshDrawItem>& items,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        VulkanPipelineStateManager& pipeline_mgr,
                        VulkanShaderManager& shader_mgr,
                        VulkanResourceManager& resource_mgr);

    void DrawSkybox(VkCommandBuffer cmd_buf,
                     unsigned int cubemap_texture_handle,
                     const glm::mat4& view,
                     const glm::mat4& projection,
                     VulkanPipelineStateManager& pipeline_mgr,
                     VulkanShaderManager& shader_mgr);

    void DrawPostProcess(VkCommandBuffer cmd_buf,
                          unsigned int source_texture,
                          const std::string& effect_name,
                          const std::vector<float>& params,
                          VulkanPipelineStateManager& pipeline_mgr,
                          VulkanShaderManager& shader_mgr);

    void DrawParticles3D(VkCommandBuffer cmd_buf,
                          const std::vector<Particle3DDrawItem>& items,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          VulkanPipelineStateManager& pipeline_mgr,
                          VulkanShaderManager& shader_mgr);

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
    const RenderStats& last_frame_stats() const { return last_frame_stats_; }
    const RenderStats& current_frame_stats() const { return current_frame_stats_; }

    // --- 几何缓冲区访问器 ---
    VkBuffer sprite_vbo() const { return sprite_vbo_; }
    VkBuffer sprite_ibo() const { return sprite_ibo_; }
    VkBuffer mesh_vbo() const { return mesh_vbo_; }
    VkBuffer mesh_ibo() const { return mesh_ibo_; }
    VkBuffer skybox_vbo() const { return skybox_vbo_; }
    VkBuffer pp_vbo() const { return pp_vbo_; }
    VkBuffer particle_vbo() const { return particle_vbo_; }

    unsigned int white_texture_handle() const { return white_texture_handle_; }

private:
    VulkanContext* context_ = nullptr;
    VulkanResourceManager* resource_mgr_ = nullptr;

    // 几何缓冲区
    VkBuffer sprite_vbo_ = VK_NULL_HANDLE;
    VkBuffer sprite_ibo_ = VK_NULL_HANDLE;
    VkDeviceMemory sprite_vbo_mem_ = VK_NULL_HANDLE;
    VkDeviceMemory sprite_ibo_mem_ = VK_NULL_HANDLE;

    VkBuffer mesh_vbo_ = VK_NULL_HANDLE;
    VkBuffer mesh_ibo_ = VK_NULL_HANDLE;
    VkDeviceMemory mesh_vbo_mem_ = VK_NULL_HANDLE;
    VkDeviceMemory mesh_ibo_mem_ = VK_NULL_HANDLE;

    VkBuffer skybox_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory skybox_vbo_mem_ = VK_NULL_HANDLE;

    VkBuffer pp_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory pp_vbo_mem_ = VK_NULL_HANDLE;

    VkBuffer particle_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory particle_vbo_mem_ = VK_NULL_HANDLE;

    // 白色纹理
    unsigned int white_texture_handle_ = 0;

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
    RenderStats last_frame_stats_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_DRAW_EXECUTOR_H

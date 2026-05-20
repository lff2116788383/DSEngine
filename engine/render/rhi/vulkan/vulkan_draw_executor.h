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
 * - UBO 管理（PerFrame/PerScene/PerMaterial）
 * - DescriptorSet 分配与绑定
 */

#ifndef DSE_RENDER_VULKAN_DRAW_EXECUTOR_H
#define DSE_RENDER_VULKAN_DRAW_EXECUTOR_H

#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/draw_executor_common.h"
#include "engine/render/rhi/postprocess_common.h"
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
struct VulkanShaderProgram;

/// PerFrame UBO 数据布局（与着色器 set=0, binding=0 对齐）
struct VulkanPerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;   ///< xyz=position, w=unused
};

/// PerScene UBO 数据布局（与着色器 set=1, binding=0 对齐）
struct VulkanPerSceneUBO {
    glm::vec4 light_dir_and_enabled;    ///< xyz=direction, w=enabled
    glm::vec4 light_color_and_ambient;  ///< xyz=color, w=ambient
    glm::vec4 light_params;             ///< x=intensity, y=shadow_strength, z=receive_shadow, w=unused
    glm::vec4 cascade_splits;           ///< xyz=cascade_splits, w=unused
    glm::mat4 light_space_matrices[3];
};

/// PerMaterial UBO 数据布局（与着色器 set=2, binding=0 对齐）
struct VulkanPerMaterialUBO {
    glm::vec4 albedo;           ///< xyz=albedo, w=metallic
    glm::vec4 roughness_ao;     ///< x=roughness, y=ao, z=normal_strength, w=alpha_cutoff
    glm::vec4 emissive;         ///< xyz=emissive, w=alpha_test
    glm::vec4 flags;            ///< x=has_normal_map, y=has_metallic_roughness_map, z=has_emissive_map, w=has_occlusion_map
    glm::vec4 extra_params;     ///< x=sss_strength, y=clear_coat, z=clear_coat_roughness, w=anisotropy
    glm::vec4 extra_params2;    ///< x=pom_height_scale, y/z/w=sss_tint RGB
    glm::vec4 toon_shadow_color; ///< xyz=shadow tint, w=shadow_threshold
    glm::vec4 toon_params;       ///< x=shadow_softness, y=specular_size, z=specular_strength, w=rim_strength
};

/// PointLights UBO（set=1, binding=1，与 GLSL std140 PointLights block 对齐）
struct VulkanPointLightsUBO {
    int  u_point_light_count;
    int  _pad0, _pad1, _pad2;
    struct Entry {
        glm::vec3 color;     float intensity;
        glm::vec3 position;  float radius;
        int cast_shadow;     int shadow_index;
        glm::vec2 _pad;
    } lights[64];
};
static_assert(sizeof(VulkanPointLightsUBO) % 16 == 0,
              "VulkanPointLightsUBO must be 16B aligned");

/// SpotLights UBO（set=1, binding=2，与 GLSL std140 SpotLights block 对齐）
struct VulkanSpotLightsUBO {
    int  u_spot_light_count;
    int  _pad0, _pad1, _pad2;
    struct Entry {
        glm::vec3 color;       float intensity;
        glm::vec3 position;    float radius;
        glm::vec3 direction;   float inner_cone;
        float outer_cone;      int cast_shadow;
        int shadow_index;      float _pad2;
    } lights[64];
};
static_assert(sizeof(VulkanSpotLightsUBO) % 16 == 0,
              "VulkanSpotLightsUBO must be 16B aligned");

/**
 * @class VulkanDrawExecutor
 * @brief Vulkan 绘制执行器
 *
 * 职责：
 * 1. 管理几何缓冲区（VkBuffer/VkDeviceMemory）
 * 2. 管理 UBO 缓冲区（PerFrame/PerScene/PerMaterial，按 swapchain image 双缓冲）
 * 3. 录制各类绘制命令到 VkCommandBuffer
 * 4. 分配与更新 DescriptorSet，在 draw call 前绑定
 * 5. 管理全局阴影贴图和光源空间矩阵
 * 6. 追踪渲染统计数据
 */
class VulkanDrawExecutor {
public:
    explicit VulkanDrawExecutor(DrawExecutorGlobalState& shared_state)
        : global_state_(shared_state) {}
    ~VulkanDrawExecutor() = default;

    /// 初始化几何缓冲区和 UBO 缓冲区
    void InitGeometryBuffers(VulkanContext* context, VulkanResourceManager* resource_mgr);

    /// 清理所有几何缓冲区和 UBO 资源
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
                          VulkanShaderManager& shader_mgr,
                          VulkanResourceManager& resource_mgr);

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
                          const PostProcessRequest& request,
                          VulkanPipelineStateManager& pipeline_mgr,
                          VulkanShaderManager& shader_mgr);

    void DrawParticles3D(VkCommandBuffer cmd_buf,
                          const std::vector<Particle3DDrawItem>& items,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          VulkanPipelineStateManager& pipeline_mgr,
                          VulkanShaderManager& shader_mgr);

    void DrawHairStrands(VkCommandBuffer cmd_buf,
                          const std::vector<HairDrawItem>& items,
                          const glm::mat4& view,
                          const glm::mat4& projection,
                          VulkanPipelineStateManager& pipeline_mgr,
                          VulkanShaderManager& shader_mgr);

    // --- GPU-Driven PBR 渲染设置 ---
    void SetupGPUDrivenPBR(VkCommandBuffer cmd_buf,
                            const glm::mat4& view, const glm::mat4& proj,
                            const glm::vec3& camera_pos,
                            const glm::vec3& light_dir, const glm::vec3& light_color,
                            float light_intensity, float ambient_intensity,
                            VulkanPipelineStateManager& pipeline_mgr,
                            VulkanShaderManager& shader_mgr);

    // --- 渲染统计 ---
    void BeginFrame();
    void EndFrame();
    const RenderStats& last_frame_stats() const { return global_state_.last_frame_stats; }
    const RenderStats& current_frame_stats() const { return global_state_.current_frame_stats; }

    // --- 几何缓冲区访问器 ---
    VkBuffer sprite_vbo() const { return sprite_vbo_; }
    VkBuffer sprite_ibo() const { return sprite_ibo_; }
    VkBuffer mesh_vbo() const { return mesh_vbo_; }
    VkBuffer mesh_ibo() const { return mesh_ibo_; }
    VkBuffer skybox_vbo() const { return skybox_vbo_; }
    VkBuffer pp_vbo() const { return pp_vbo_; }
    VkBuffer particle_vbo() const { return particle_vbo_; }

    unsigned int white_texture_handle() const { return white_texture_handle_; }

    /// 设置当前帧绑定的 SSBO 状态 (binding_point → RHI handle)
    void SetBoundSSBOs(const std::unordered_map<unsigned int, unsigned int>& ssbos) {
        bound_ssbos_ = ssbos;
    }

private:
    /// 创建单个 UBO 缓冲区（host-visible + coherent）
    bool CreateUBOBuffer(VkDeviceSize size, VkBuffer& out_buf, VkDeviceMemory& out_mem);

    /// 更新 PerFrame UBO 数据并写入缓冲
    void UpdatePerFrameUBO(const glm::mat4& view, const glm::mat4& projection,
                            const std::unordered_map<std::string, glm::mat4>& pending_mat4);

    /// 更新 PerScene UBO 数据并写入缓冲（从 MeshDrawItem 中提取光源信息）
    void UpdatePerSceneUBO(const MeshDrawItem& item);

    /// 更新 PerMaterial UBO 数据并写入缓冲
    void UpdatePerMaterialUBO(const MeshDrawItem& item);

    /// 更新 PointLights/SpotLights UBO 数据并写入缓冲
    void UpdatePointSpotLightUBOs(const MeshDrawItem& item);

    /// 为 3D mesh 绘制分配并更新 DescriptorSet（Set 0/1/2/3）
    VkDescriptorSet AllocateAndUpdateMeshDescriptorSets(
        VkCommandBuffer cmd_buf,
        const VulkanShaderProgram* program,
        const MeshDrawItem& item,
        VulkanResourceManager& resource_mgr,
        VkDeviceSize bone_offset = 0,
        VkDeviceSize per_frame_offset = 0,
        VkDeviceSize per_scene_offset = 0,
        VkDeviceSize per_material_offset = 0,
        VkDeviceSize per_pl_offset = 0,
        VkDeviceSize per_sl_offset = 0,
        bool gbuffer_mode = false);

    /// 为天空盒绘制分配并更新 DescriptorSet
    VkDescriptorSet AllocateAndUpdateSkyboxDescriptorSets(
        VkCommandBuffer cmd_buf,
        const VulkanShaderProgram* program,
        unsigned int cubemap_texture_handle,
        VulkanResourceManager& resource_mgr);

    /// 分配全部 4 个 DescriptorSet 并填充 dummy 数据，返回 sets vector
    /// override_set: 如果 >= 0，在该 set 的 override_binding 处写入 override_tex
    std::vector<VkDescriptorSet> AllocateAllSetsWithDummies(
        const VulkanShaderProgram* program,
        VulkanResourceManager& resource_mgr);

    /// 为粒子绘制分配并更新 DescriptorSet
    VkDescriptorSet AllocateAndUpdateParticleDescriptorSets(
        VkCommandBuffer cmd_buf,
        const VulkanShaderProgram* program,
        unsigned int texture_handle,
        VulkanResourceManager& resource_mgr);

    /// 为后处理绘制分配并更新 DescriptorSet
    /// extra_bindings: set2 中额外纹理 {binding, texture_handle} 列表
    VkDescriptorSet AllocateAndUpdatePostProcessDescriptorSets(
        VkCommandBuffer cmd_buf,
        const VulkanShaderProgram* program,
        unsigned int source_texture,
        VulkanResourceManager& resource_mgr,
        const std::vector<std::pair<uint32_t, unsigned int>>& extra_bindings = {});

    /// Bloom Compute Shader 调度（是另开的 Compute 流程）
    void DispatchBloomCompute(VkCommandBuffer cmd_buf,
                              unsigned int cs_handle,
                              unsigned int src_texture_handle,
                              unsigned int dst_rt_handle,
                              VulkanShaderManager& shader_mgr);

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

    VkBuffer instance_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory instance_vbo_mem_ = VK_NULL_HANDLE;
    VkDeviceSize instance_vbo_capacity_ = 0;  ///< GPU Instancing VBO 容量（字节）

    VkBuffer skybox_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory skybox_vbo_mem_ = VK_NULL_HANDLE;

    VkBuffer pp_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory pp_vbo_mem_ = VK_NULL_HANDLE;

    VkBuffer particle_vbo_ = VK_NULL_HANDLE;
    VkDeviceMemory particle_vbo_mem_ = VK_NULL_HANDLE;

    // 白色纹理
    unsigned int white_texture_handle_ = 0;

    // UBO 缓冲区（双缓冲，按 swapchain image）
    static constexpr int MAX_FRAMES = 2;
    VkBuffer per_frame_ubo_[MAX_FRAMES] = {};
    VkDeviceMemory per_frame_ubo_mem_[MAX_FRAMES] = {};
    VkBuffer per_scene_ubo_[MAX_FRAMES] = {};
    VkDeviceMemory per_scene_ubo_mem_[MAX_FRAMES] = {};
    VkBuffer per_material_ubo_[MAX_FRAMES] = {};
    VkDeviceMemory per_material_ubo_mem_[MAX_FRAMES] = {};
    VkBuffer       per_point_lights_ubo_[MAX_FRAMES] = {};
    VkDeviceMemory per_point_lights_ubo_mem_[MAX_FRAMES] = {};
    VkBuffer       per_spot_lights_ubo_[MAX_FRAMES] = {};
    VkDeviceMemory per_spot_lights_ubo_mem_[MAX_FRAMES] = {};
    VkBuffer       bone_matrices_ubo_ = VK_NULL_HANDLE;
    VkDeviceMemory bone_matrices_ubo_mem_ = VK_NULL_HANDLE;
    VkBuffer       morph_weights_ubo_ = VK_NULL_HANDLE;
    VkDeviceMemory morph_weights_ubo_mem_ = VK_NULL_HANDLE;
    VkBuffer       light_probe_ubo_[MAX_FRAMES] = {};
    VkDeviceMemory light_probe_ubo_mem_[MAX_FRAMES] = {};

    // 当前帧索引（与 VulkanContext::current_frame() 对齐）
    uint32_t current_frame_index_ = 0;

    // 当前活跃的 RenderTarget 和 RenderPass（由 BeginRenderPass 设置）
    unsigned int current_rt_handle_ = 0;
    VkRenderPass current_render_pass_ = VK_NULL_HANDLE;
    VkSampleCountFlagBits current_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
    uint32_t current_color_attachment_count_ = 1;
    VkDeviceSize mesh_vbo_offset_ = 0;   ///< 当前帧 mesh VBO 写入偏移
    VkDeviceSize mesh_ibo_offset_ = 0;   ///< 当前帧 mesh IBO 写入偏移
    VkDeviceSize bone_matrices_offset_ = 0; ///< 当前帧 bone matrices UBO 写入偏移
    VkDeviceSize per_frame_ubo_offset_ = 0;   ///< 当前帧 per-frame UBO 写入偏移（每个 batch 一个 slot）
    VkDeviceSize per_scene_ubo_offset_ = 0;   ///< 当前帧 per-scene UBO 写入偏移（每个 item 一个 slot）
    VkDeviceSize per_material_ubo_offset_ = 0; ///< 当前帧 per-material UBO 写入偏移
    VkDeviceSize per_point_lights_ubo_offset_ = 0;
    VkDeviceSize per_spot_lights_ubo_offset_ = 0;
    static constexpr VkDeviceSize kUboSlotAlignment = 256; ///< UBO offset 对齐（覆盖大多数 GPU 的 minUniformBufferOffsetAlignment）
    /// 光源 UBO slot 大小：sizeof(VulkanPointLightsUBO)=3088, sizeof(VulkanSpotLightsUBO)=4112，向上对齐到 256 的倍数
    static constexpr VkDeviceSize kLightUboSlotAlignment = 4352; ///< ceil(4112/256)*256 = 4352
    unsigned int nocull_pipeline_state_ = 0; ///< 双面材质无剔除管线状态句柄
    unsigned int sprite_pipeline_state_ = 0; ///< 2D 精灵管线状态句柄（缓存避免每帧重建）
    unsigned int skybox_pipeline_state_ = 0; ///< 天空盒管线状态句柄
    unsigned int pp_pipeline_state_ = 0;     ///< 后处理管线状态句柄（无混合）
    unsigned int pp_blend_pipeline_state_ = 0; ///< 后处理管线状态句柄（alpha 混合，ui_overlay）
    int render_pass_counter_ = 0;
    int max_render_passes_ = -1;  // -1 = 无限制
    bool skip_current_pass_ = false;

    // 全局渲染状态（引用 RhiDevice::global_render_state_）
    DrawExecutorGlobalState& global_state_;

    // 当前帧绑定的 SSBO 状态 (binding_point → RHI handle)
    std::unordered_map<unsigned int, unsigned int> bound_ssbos_;

    // Hair rendering 缓存
    unsigned int hair_shader_handle_ = 0;
    VkPipeline hair_pipeline_ = VK_NULL_HANDLE;
    VkRenderPass hair_pipeline_rp_ = VK_NULL_HANDLE;

};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_DRAW_EXECUTOR_H

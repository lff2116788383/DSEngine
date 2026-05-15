/**
 * @file vulkan_draw_executor.cpp
 * @brief Vulkan 绘制执行器实现
 *
 * 实现完整的 Vulkan 命令缓冲录制逻辑：
 * - 几何缓冲区初始化（VBO/IBO 创建与数据上传）
 * - UBO 缓冲区管理（PerFrame/PerScene/PerMaterial 双缓冲）
 * - DescriptorSet 分配/更新/绑定
 * - DrawSpriteBatch：2D 精灵批处理
 * - DrawMeshBatch：3D PBR 网格绘制（push constant + descriptor set）
 * - DrawSkybox：天空盒绘制
 * - DrawPostProcess：后处理全屏四边形
 * - DrawParticles3D：3D 粒子 billboard
 */

#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_sources.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstddef>

namespace dse {
namespace render {

// 几何缓冲区容量常量（与 GL 版本对齐）
constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_SPRITE_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_SPRITE_INDICES = MAX_SPRITES * 6;
constexpr size_t MAX_MESH_VERTICES = 131072;
constexpr size_t MAX_MESH_INDICES = 262144;

// ============================================================================
// 辅助：创建 VkBuffer + VkDeviceMemory
// ============================================================================

namespace {

/// 创建 Vulkan 缓冲区（host-visible，用于动态更新）
bool CreateVulkanBuffer(VkDevice device, VkPhysicalDevice physical_device,
                        VkDeviceSize size, VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties,
                        VkBuffer& out_buffer, VkDeviceMemory& out_memory) {
    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size = size;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_ci, nullptr, &out_buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, out_buffer, &mem_reqs);

    // 查找合适的内存类型
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

    uint32_t memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((mem_reqs.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            memory_type_index = i;
            break;
        }
    }
    if (memory_type_index == UINT32_MAX) return false;

    VkMemoryAllocateInfo alloc_ci{};
    alloc_ci.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize = mem_reqs.size;
    alloc_ci.memoryTypeIndex = memory_type_index;

    if (vkAllocateMemory(device, &alloc_ci, nullptr, &out_memory) != VK_SUCCESS) {
        return false;
    }

    vkBindBufferMemory(device, out_buffer, out_memory, 0);
    return true;
}

/// 将数据写入 host-visible 缓冲区
void WriteToBuffer(VkDevice device, VkDeviceMemory memory,
                   VkDeviceSize offset, VkDeviceSize size, const void* data) {
    void* mapped = nullptr;
    if (vkMapMemory(device, memory, offset, size, 0, &mapped) != VK_SUCCESS) return;
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(device, memory);
}

/// 创建 UBO 缓冲区辅助（UBO 用途 + host-visible + coherent）
bool CreateUBOBufferInternal(VkDevice device, VkPhysicalDevice physical_device,
                              VkDeviceSize size,
                              VkBuffer& out_buf, VkDeviceMemory& out_mem) {
    return CreateVulkanBuffer(device, physical_device, size,
                              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              out_buf, out_mem);
}

} // anonymous namespace

// ============================================================================
// InitGeometryBuffers / ShutdownGeometryBuffers
// ============================================================================

void VulkanDrawExecutor::InitGeometryBuffers(
    VulkanContext* context, VulkanResourceManager* resource_mgr) {
    context_ = context;
    resource_mgr_ = resource_mgr;

    auto device = context->device();
    auto physical_device = context->physical_device();

    // --- 精灵批处理 VBO/IBO ---
    // 顶点格式：vec2 pos, vec2 texcoord, vec4 color = 8 floats * 4 = 32 bytes
    const VkDeviceSize sprite_vbo_size = MAX_SPRITE_VERTICES * 32;
    const VkDeviceSize sprite_ibo_size = MAX_SPRITE_INDICES * sizeof(uint16_t);

    CreateVulkanBuffer(device, physical_device, sprite_vbo_size,
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       sprite_vbo_, sprite_vbo_mem_);

    CreateVulkanBuffer(device, physical_device, sprite_ibo_size,
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       sprite_ibo_, sprite_ibo_mem_);

    // --- 3D 网格 VBO/IBO ---
    // 多 pass 每帧累积写入，需要足够大的缓冲区
    const VkDeviceSize mesh_vbo_size = 64 * 1024 * 1024;  // 64 MB
    const VkDeviceSize mesh_ibo_size = 8 * 1024 * 1024;   //  8 MB

    CreateVulkanBuffer(device, physical_device, mesh_vbo_size,
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       mesh_vbo_, mesh_vbo_mem_);

    CreateVulkanBuffer(device, physical_device, mesh_ibo_size,
                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       mesh_ibo_, mesh_ibo_mem_);

    // --- 天空盒 VBO ---
    // 36 顶点 * vec3 = 36 * 12 = 432 bytes
    const float skybox_vertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,
    };

    CreateVulkanBuffer(device, physical_device, sizeof(skybox_vertices),
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       skybox_vbo_, skybox_vbo_mem_);
    WriteToBuffer(device, skybox_vbo_mem_, 0, sizeof(skybox_vertices), skybox_vertices);

    // --- 后处理全屏四边形 VBO ---
    const float pp_vertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    CreateVulkanBuffer(device, physical_device, sizeof(pp_vertices),
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       pp_vbo_, pp_vbo_mem_);
    WriteToBuffer(device, pp_vbo_mem_, 0, sizeof(pp_vertices), pp_vertices);

    // --- 粒子四边形 VBO ---
    // 4 顶点 billboard：vec3 pos, vec2 uv
    const float particle_vertices[] = {
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
    };

    CreateVulkanBuffer(device, physical_device, sizeof(particle_vertices),
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       particle_vbo_, particle_vbo_mem_);
    WriteToBuffer(device, particle_vbo_mem_, 0, sizeof(particle_vertices), particle_vertices);

    // --- UBO 缓冲区（双缓冲，每个缓冲区扩大到多 slot，避免 GPU 延迟执行时覆盙） ---
    // per_frame: 16 batches/frame × 256B = 4KB
    // per_scene/material/lights: 512 items/frame × 256B = 128KB
    constexpr size_t kPerFrameSlots  = 16;
    constexpr size_t kPerItemSlots   = 512;
    constexpr size_t kSlotAlign      = kUboSlotAlignment;
    for (int i = 0; i < MAX_FRAMES; ++i) {
        CreateUBOBufferInternal(device, physical_device, kPerFrameSlots * kSlotAlign,
                                per_frame_ubo_[i], per_frame_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kSlotAlign,
                                per_scene_ubo_[i], per_scene_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kSlotAlign,
                                per_material_ubo_[i], per_material_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kLightUboSlotAlignment,
                                per_point_lights_ubo_[i], per_point_lights_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kLightUboSlotAlignment,
                                per_spot_lights_ubo_[i], per_spot_lights_ubo_mem_[i]);
    }

    // --- BoneMatrices / MorphWeights UBO（多 mesh 累积偏移，避免 GPU 延迟执行覆盖） ---
    constexpr size_t kBoneMatricesSize = 64 * 100 * sizeof(glm::mat4); // 64 meshes * 6400 bytes = 400KB
    constexpr size_t kMorphWeightsSize = 16; // 4 floats
    CreateUBOBufferInternal(device, physical_device, kBoneMatricesSize,
                            bone_matrices_ubo_, bone_matrices_ubo_mem_);
    CreateUBOBufferInternal(device, physical_device, kMorphWeightsSize,
                            morph_weights_ubo_, morph_weights_ubo_mem_);
    // 初始化 BoneMatrices 为单位矩阵
    {
        std::vector<glm::mat4> identity_bones(64 * 100, glm::mat4(1.0f));
        WriteToBuffer(device, bone_matrices_ubo_mem_, 0, kBoneMatricesSize, identity_bones.data());
    }
    // MorphWeights 初始化为 0
    {
        float zero_weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        WriteToBuffer(device, morph_weights_ubo_mem_, 0, kMorphWeightsSize, zero_weights);
    }

    // --- LightProbeData UBO（双缓冲，每帧一份） ---
    constexpr size_t kLightProbeSize = sizeof(glm::vec4) * 10; // 9 SH + 1 params = 160B
    for (int i = 0; i < MAX_FRAMES; ++i) {
        CreateUBOBufferInternal(device, physical_device, kLightProbeSize,
                                light_probe_ubo_[i], light_probe_ubo_mem_[i]);
        // 初始化为零（probe_params.x = 0 = disabled）
        glm::vec4 zero_lp[10] = {};
        WriteToBuffer(device, light_probe_ubo_mem_[i], 0, kLightProbeSize, zero_lp);
    }

    // --- GPU Instancing VBO（初始 256 实例 = 16KB）---
    {
        constexpr size_t kInitialInstanceCapacity = 256;
        instance_vbo_capacity_ = kInitialInstanceCapacity * sizeof(glm::mat4);
        CreateVulkanBuffer(device, physical_device, instance_vbo_capacity_,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           instance_vbo_, instance_vbo_mem_);
    }

    // --- 白色纹理 ---
    unsigned char white_pixel[4] = {255, 255, 255, 255};
    white_texture_handle_ = resource_mgr->CreateTexture2D(1, 1, white_pixel, true);

    DEBUG_LOG_INFO("VulkanDrawExecutor geometry buffers + UBO buffers initialized");
}

void VulkanDrawExecutor::ShutdownGeometryBuffers() {
    auto device = context_->device();

    auto destroy_buffer = [&](VkBuffer& buf, VkDeviceMemory& mem) {
        if (buf != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buf, nullptr);
            buf = VK_NULL_HANDLE;
        }
        if (mem != VK_NULL_HANDLE) {
            vkFreeMemory(device, mem, nullptr);
            mem = VK_NULL_HANDLE;
        }
    };

    destroy_buffer(sprite_vbo_, sprite_vbo_mem_);
    destroy_buffer(sprite_ibo_, sprite_ibo_mem_);
    destroy_buffer(mesh_vbo_, mesh_vbo_mem_);
    destroy_buffer(mesh_ibo_, mesh_ibo_mem_);
    destroy_buffer(instance_vbo_, instance_vbo_mem_);
    instance_vbo_capacity_ = 0;
    destroy_buffer(skybox_vbo_, skybox_vbo_mem_);
    destroy_buffer(pp_vbo_, pp_vbo_mem_);
    destroy_buffer(particle_vbo_, particle_vbo_mem_);

    for (int i = 0; i < MAX_FRAMES; ++i) {
        destroy_buffer(per_frame_ubo_[i], per_frame_ubo_mem_[i]);
        destroy_buffer(per_scene_ubo_[i], per_scene_ubo_mem_[i]);
        destroy_buffer(per_material_ubo_[i], per_material_ubo_mem_[i]);
        destroy_buffer(per_point_lights_ubo_[i], per_point_lights_ubo_mem_[i]);
        destroy_buffer(per_spot_lights_ubo_[i], per_spot_lights_ubo_mem_[i]);
    }
    destroy_buffer(bone_matrices_ubo_, bone_matrices_ubo_mem_);
    destroy_buffer(morph_weights_ubo_, morph_weights_ubo_mem_);
    for (int i = 0; i < MAX_FRAMES; ++i)
        destroy_buffer(light_probe_ubo_[i], light_probe_ubo_mem_[i]);

    if (white_texture_handle_ != 0) {
        resource_mgr_->DeleteTexture(white_texture_handle_);
        white_texture_handle_ = 0;
    }

    DEBUG_LOG_INFO("VulkanDrawExecutor geometry buffers destroyed");
}

// ============================================================================
// UBO 更新
// ============================================================================

void VulkanDrawExecutor::UpdatePerFrameUBO(
    const glm::mat4& view, const glm::mat4& projection,
    const std::unordered_map<std::string, glm::mat4>& pending_mat4) {

    VulkanPerFrameUBO ubo{};
    ubo.vp = projection * view;
    ubo.view = view;

    // 从 view 矩阵的逆矩阵提取相机世界位置（与 OpenGL 一致）
    glm::mat4 inv_view = glm::inverse(view);
    ubo.camera_pos = glm::vec4(inv_view[3][0], inv_view[3][1], inv_view[3][2], 0.0f);

    WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                  per_frame_ubo_offset_, sizeof(VulkanPerFrameUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerSceneUBO(const MeshDrawItem& item) {
    VulkanPerSceneUBO ubo{};
    ubo.light_dir_and_enabled = glm::vec4(item.light_direction, item.lighting_enabled ? 1.0f : 0.0f);
    ubo.light_color_and_ambient = glm::vec4(item.light_color, item.ambient_intensity);
    ubo.light_params = glm::vec4(item.light_intensity, item.shadow_strength,
                                  item.receive_shadow ? 1.0f : 0.0f, static_cast<float>(item.shading_mode));
    ubo.cascade_splits = glm::vec4(global_state_.cascade_splits[0], global_state_.cascade_splits[1],
                                    global_state_.cascade_splits[2], static_cast<float>(item.wboit_mode));
    for (int i = 0; i < 3; ++i) {
        ubo.light_space_matrices[i] = global_state_.light_space_matrix[i];
    }

    WriteToBuffer(context_->device(), per_scene_ubo_mem_[current_frame_index_],
                  per_scene_ubo_offset_, sizeof(VulkanPerSceneUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerMaterialUBO(const MeshDrawItem& item) {
    VulkanPerMaterialUBO ubo{};
    ubo.albedo = glm::vec4(item.material_albedo, item.material_metallic);
    ubo.roughness_ao = glm::vec4(item.material_roughness, item.material_ao,
                                  item.material_normal_strength, item.material_alpha_cutoff);
    ubo.emissive = glm::vec4(item.material_emissive, item.material_alpha_test ? 1.0f : 0.0f);
    ubo.flags = glm::vec4(
        item.normal_map_handle != 0 ? 1.0f : 0.0f,
        item.metallic_roughness_map_handle != 0 ? 1.0f : 0.0f,
        item.emissive_map_handle != 0 ? 1.0f : 0.0f,
        item.occlusion_map_handle != 0 ? 1.0f : 0.0f
    );
    ubo.extra_params = glm::vec4(
        item.material_sss_strength,
        item.material_clear_coat,
        item.material_clear_coat_roughness,
        item.material_anisotropy
    );
    ubo.extra_params2 = glm::vec4(
        item.material_pom_height_scale,
        item.material_sss_tint.x, item.material_sss_tint.y, item.material_sss_tint.z
    );
    if (item.shading_mode == 5) {
        ubo.toon_shadow_color = glm::vec4(
            item.watercolor_paper_strength, item.watercolor_edge_darkening,
            item.watercolor_color_bleed, item.watercolor_pigment_density);
        ubo.toon_params = glm::vec4(0.0f);
    } else {
        ubo.toon_shadow_color = glm::vec4(item.toon_shadow_color, item.toon_shadow_threshold);
        ubo.toon_params = glm::vec4(
            item.toon_shadow_softness, item.toon_specular_size,
            item.toon_specular_strength, item.toon_rim_strength);
    }

    WriteToBuffer(context_->device(), per_material_ubo_mem_[current_frame_index_],
                  per_material_ubo_offset_, sizeof(VulkanPerMaterialUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePointSpotLightUBOs(const MeshDrawItem& item) {
    uint32_t fi = current_frame_index_;

    VulkanPointLightsUBO pl_ubo{};
    pl_ubo.u_point_light_count = static_cast<int>(
        (std::min)(item.point_lights.size(), (size_t)64));
    for (int i = 0; i < pl_ubo.u_point_light_count; ++i) {
        const auto& src = item.point_lights[i];
        auto& dst = pl_ubo.lights[i];
        dst.color        = src.color;
        dst.intensity    = src.intensity;
        dst.position     = src.position;
        dst.radius       = src.radius;
        dst.cast_shadow  = src.cast_shadow ? 1 : 0;
        dst.shadow_index = src.shadow_index;
    }
    WriteToBuffer(context_->device(), per_point_lights_ubo_mem_[fi],
                  per_point_lights_ubo_offset_, sizeof(pl_ubo), &pl_ubo);

    VulkanSpotLightsUBO sl_ubo{};
    sl_ubo.u_spot_light_count = static_cast<int>(
        (std::min)(item.spot_lights.size(), (size_t)64));
    for (int i = 0; i < sl_ubo.u_spot_light_count; ++i) {
        const auto& src = item.spot_lights[i];
        auto& dst = sl_ubo.lights[i];
        dst.color        = src.color;
        dst.intensity    = src.intensity;
        dst.position     = src.position;
        dst.radius       = src.radius;
        dst.direction    = src.direction;
        dst.inner_cone   = src.inner_cone;
        dst.outer_cone   = src.outer_cone;
        dst.cast_shadow  = src.cast_shadow ? 1 : 0;
        dst.shadow_index = src.shadow_index;
    }
    WriteToBuffer(context_->device(), per_spot_lights_ubo_mem_[fi],
                  per_spot_lights_ubo_offset_, sizeof(sl_ubo), &sl_ubo);
}

// ============================================================================
// DescriptorSet 分配与更新
// ============================================================================

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdateMeshDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    const MeshDrawItem& item,
    VulkanResourceManager& resource_mgr,
    VkDeviceSize bone_offset,
    VkDeviceSize per_frame_offset,
    VkDeviceSize per_scene_offset,
    VkDeviceSize per_material_offset,
    VkDeviceSize per_pl_offset,
    VkDeviceSize per_sl_offset,
    bool gbuffer_mode) {

    auto device = context_->device();
    uint32_t fi = current_frame_index_;

    // 程序至少需要 3 个 set layout（Set 0=PerFrame, Set 1=PerScene+Lights, Set 2=PerMaterial+Samplers， Set 3=点光阴影）
    if (program->descriptor_set_layouts.size() < 3) {
        DEBUG_LOG_WARN("Mesh shader program has insufficient descriptor set layouts ({})",
                       program->descriptor_set_layouts.size());
        return VK_NULL_HANDLE;
    }

    // 为每个 set 分配一个 DescriptorSet（最多 4 个）
    VkDescriptorSet sets[4] = {};
    const int set_count = static_cast<int>(program->descriptor_set_layouts.size());
    for (int s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) {
            DEBUG_LOG_WARN("Failed to allocate descriptor set for set {}", s);
            return VK_NULL_HANDLE;
        }
    }

    // --- Set 0: PerFrame UBO ---
    {
        VkDescriptorBufferInfo buf_info{};
        buf_info.buffer = per_frame_ubo_[fi];
        buf_info.offset = per_frame_offset;
        buf_info.range = sizeof(VulkanPerFrameUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets[0];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buf_info;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // --- Set 1: PerScene UBO (binding 0) ---
    {
        VkDescriptorBufferInfo buf_info{};
        buf_info.buffer = per_scene_ubo_[fi];
        buf_info.offset = per_scene_offset;
        buf_info.range = sizeof(VulkanPerSceneUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets[1];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buf_info;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // --- Set 1 binding 1+: PBR 专用 SSBO/UBO（GBuffer 模式跳过）---
    if (!gbuffer_mode) {

    // --- Set 1 binding 1: PointLights SSBO ---
    {
        VkDescriptorBufferInfo pl_buf{};
        auto pl_it = bound_ssbos_.find(1); // binding 1 = PointLightSSBO
        const VulkanBuffer* pl_ssbo = (pl_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(pl_it->second) : nullptr;
        if (pl_ssbo && pl_ssbo->buffer != VK_NULL_HANDLE) {
            pl_buf.buffer = pl_ssbo->buffer;
            pl_buf.offset = 0;
            pl_buf.range  = pl_ssbo->size;
        } else {
            // fallback: 空 SSBO（避免 descriptor 未初始化）
            pl_buf.buffer = per_point_lights_ubo_[fi];
            pl_buf.offset = 0;
            pl_buf.range  = 16; // 仅 header (count=0)
        }

        VkWriteDescriptorSet pl_write{};
        pl_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pl_write.dstSet          = sets[1];
        pl_write.dstBinding      = 1;
        pl_write.descriptorCount = 1;
        pl_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pl_write.pBufferInfo     = &pl_buf;
        vkUpdateDescriptorSets(device, 1, &pl_write, 0, nullptr);
    }

    // --- Set 1 binding 2: SpotLights SSBO ---
    {
        VkDescriptorBufferInfo sl_buf{};
        auto sl_it = bound_ssbos_.find(2); // binding 2 = SpotLightSSBO
        const VulkanBuffer* sl_ssbo = (sl_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(sl_it->second) : nullptr;
        if (sl_ssbo && sl_ssbo->buffer != VK_NULL_HANDLE) {
            sl_buf.buffer = sl_ssbo->buffer;
            sl_buf.offset = 0;
            sl_buf.range  = sl_ssbo->size;
        } else {
            sl_buf.buffer = per_spot_lights_ubo_[fi];
            sl_buf.offset = 0;
            sl_buf.range  = 16;
        }

        VkWriteDescriptorSet sl_write{};
        sl_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sl_write.dstSet          = sets[1];
        sl_write.dstBinding      = 2;
        sl_write.descriptorCount = 1;
        sl_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        sl_write.pBufferInfo     = &sl_buf;
        vkUpdateDescriptorSets(device, 1, &sl_write, 0, nullptr);
    }

    // --- Set 1 binding 3: ClusterInfo SSBO ---
    {
        VkDescriptorBufferInfo ci_buf{};
        auto ci_it = bound_ssbos_.find(3); // binding 3 = ClusterInfoSSBO
        const VulkanBuffer* ci_ssbo = (ci_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(ci_it->second) : nullptr;
        if (ci_ssbo && ci_ssbo->buffer != VK_NULL_HANDLE) {
            ci_buf.buffer = ci_ssbo->buffer;
            ci_buf.offset = 0;
            ci_buf.range  = ci_ssbo->size;
        } else {
            ci_buf.buffer = per_point_lights_ubo_[fi];
            ci_buf.offset = 0;
            ci_buf.range  = 16;
        }

        VkWriteDescriptorSet ci_write{};
        ci_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ci_write.dstSet          = sets[1];
        ci_write.dstBinding      = 3;
        ci_write.descriptorCount = 1;
        ci_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ci_write.pBufferInfo     = &ci_buf;
        vkUpdateDescriptorSets(device, 1, &ci_write, 0, nullptr);
    }

    // --- Set 1 binding 4: LightIndex SSBO ---
    {
        VkDescriptorBufferInfo li_buf{};
        auto li_it = bound_ssbos_.find(4); // binding 4 = LightIndexSSBO
        const VulkanBuffer* li_ssbo = (li_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(li_it->second) : nullptr;
        if (li_ssbo && li_ssbo->buffer != VK_NULL_HANDLE) {
            li_buf.buffer = li_ssbo->buffer;
            li_buf.offset = 0;
            li_buf.range  = li_ssbo->size;
        } else {
            li_buf.buffer = per_point_lights_ubo_[fi];
            li_buf.offset = 0;
            li_buf.range  = 16;
        }

        VkWriteDescriptorSet li_write{};
        li_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        li_write.dstSet          = sets[1];
        li_write.dstBinding      = 4;
        li_write.descriptorCount = 1;
        li_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        li_write.pBufferInfo     = &li_buf;
        vkUpdateDescriptorSets(device, 1, &li_write, 0, nullptr);
    }

    // --- Set 1 binding 5: LightProbeData UBO ---
    {
        // 写入 SH 数据到当前帧的 UBO
        struct LightProbeGPU {
            glm::vec4 sh[9];
            glm::vec4 params;
        } lp_data{};
        for (int i = 0; i < 9; ++i) lp_data.sh[i] = global_state_.light_probe_sh[i];
        lp_data.params = glm::vec4(global_state_.light_probe_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
        WriteToBuffer(device, light_probe_ubo_mem_[fi], 0, sizeof(lp_data), &lp_data);

        VkDescriptorBufferInfo lp_buf{};
        lp_buf.buffer = light_probe_ubo_[fi];
        lp_buf.offset = 0;
        lp_buf.range  = sizeof(lp_data);

        VkWriteDescriptorSet lp_write{};
        lp_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lp_write.dstSet          = sets[1];
        lp_write.dstBinding      = 5;
        lp_write.descriptorCount = 1;
        lp_write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lp_write.pBufferInfo     = &lp_buf;
        vkUpdateDescriptorSets(device, 1, &lp_write, 0, nullptr);
    }

    } // !gbuffer_mode — Set 1 PBR 专用绑定结束

    // --- Set 2: PerMaterial UBO + 采样器 ---
    if (gbuffer_mode) {
        // GBuffer 模式只绑定 albedo 纹理到 binding 1
        unsigned int tex_handle = item.texture_handle;
        if (tex_handle == 0) tex_handle = white_texture_handle_;
        const VulkanTexture* tex = resource_mgr.GetTexture(tex_handle);
        if (!tex) tex = resource_mgr.GetTexture(white_texture_handle_);
        if (tex) {
            VkDescriptorImageInfo img_info{};
            img_info.sampler = resource_mgr.default_sampler();
            img_info.imageView = tex->image_view;
            img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet tex_write{};
            tex_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            tex_write.dstSet = sets[2];
            tex_write.dstBinding = 1;
            tex_write.dstArrayElement = 0;
            tex_write.descriptorCount = 1;
            tex_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            tex_write.pImageInfo = &img_info;
            vkUpdateDescriptorSets(device, 1, &tex_write, 0, nullptr);
        }
    } else {
        // PerMaterial UBO (binding 0)
        VkDescriptorBufferInfo mat_buf_info{};
        mat_buf_info.buffer = per_material_ubo_[fi];
        mat_buf_info.offset = per_material_offset;
        mat_buf_info.range = sizeof(VulkanPerMaterialUBO);

        VkWriteDescriptorSet mat_write{};
        mat_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        mat_write.dstSet = sets[2];
        mat_write.dstBinding = 0;
        mat_write.dstArrayElement = 0;
        mat_write.descriptorCount = 1;
        mat_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mat_write.pBufferInfo = &mat_buf_info;

        // 纹理采样器 (binding 1-5)
        struct TexBinding {
            unsigned int handle;
            uint32_t binding;
        };
        TexBinding tex_bindings[] = {
            {item.texture_handle, 1},                     // albedo
            {item.normal_map_handle, 2},                   // normal
            {item.metallic_roughness_map_handle, 3},       // metallic-roughness
            {item.emissive_map_handle, 4},                  // emissive
            {item.occlusion_map_handle, 5},                 // occlusion
        };

        VkDescriptorImageInfo image_infos[5] = {};
        VkWriteDescriptorSet tex_writes[5] = {};

        int write_count = 1; // mat_write 算第 0 个
        for (int i = 0; i < 5; ++i) {
            unsigned int tex_handle = tex_bindings[i].handle;
            if (tex_handle == 0) tex_handle = white_texture_handle_;

            const VulkanTexture* tex = resource_mgr.GetTexture(tex_handle);
            if (!tex) tex = resource_mgr.GetTexture(white_texture_handle_);
            if (!tex) continue;

            image_infos[i].sampler = resource_mgr.default_sampler();
            image_infos[i].imageView = tex->image_view;
            image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            tex_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            tex_writes[i].dstSet = sets[2];
            tex_writes[i].dstBinding = tex_bindings[i].binding;
            tex_writes[i].dstArrayElement = 0;
            tex_writes[i].descriptorCount = 1;
            tex_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            tex_writes[i].pImageInfo = &image_infos[i];
            write_count++;
        }

        // 绑定 CSM 阴影贴图到 binding 6（sampler2DShadow，使用比较采样器）
        VkDescriptorImageInfo shadow_image_infos[3] = {};
        VkWriteDescriptorSet shadow_write{};
        {
            VkSampler cmp_sampler = resource_mgr.shadow_comparison_sampler();
            const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
            for (int i = 0; i < 3; ++i) {
                unsigned int sm_handle = global_state_.shadow_map[i];
                // shadow map handle 是 RT handle，需从 RT 获取 depth image view
                VkImageView depth_view = (sm_handle != 0)
                    ? resource_mgr.GetRenderTargetDepthImageView(sm_handle) : VK_NULL_HANDLE;
                if (depth_view != VK_NULL_HANDLE) {
                    shadow_image_infos[i].sampler     = cmp_sampler;
                    shadow_image_infos[i].imageView   = depth_view;
                    shadow_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                } else if (white_tex) {
                    shadow_image_infos[i].sampler     = resource_mgr.default_sampler();
                    shadow_image_infos[i].imageView   = white_tex->image_view;
                    shadow_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }
            shadow_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            shadow_write.dstSet          = sets[2];
            shadow_write.dstBinding      = 6;
            shadow_write.dstArrayElement = 0;
            shadow_write.descriptorCount = 3;
            shadow_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            shadow_write.pImageInfo      = shadow_image_infos;
        }

        // 绑定 Spot 阴影贴图到 binding 7
        VkDescriptorImageInfo spot_shadow_image_infos[4] = {};
        VkWriteDescriptorSet spot_shadow_write{};
        {
            const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
            for (int i = 0; i < 4; ++i) {
                unsigned int ss_handle = global_state_.spot_shadow_map[i];
                // spot shadow map handle 是 RT handle，需从 RT 获取 depth image view
                VkImageView depth_view = (ss_handle != 0)
                    ? resource_mgr.GetRenderTargetDepthImageView(ss_handle) : VK_NULL_HANDLE;
                if (depth_view != VK_NULL_HANDLE) {
                    spot_shadow_image_infos[i].sampler     = resource_mgr.shadow_comparison_sampler();
                    spot_shadow_image_infos[i].imageView   = depth_view;
                    spot_shadow_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                } else if (white_tex) {
                    spot_shadow_image_infos[i].sampler     = resource_mgr.default_sampler();
                    spot_shadow_image_infos[i].imageView   = white_tex->image_view;
                    spot_shadow_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }
            spot_shadow_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            spot_shadow_write.dstSet          = sets[2];
            spot_shadow_write.dstBinding      = 7;
            spot_shadow_write.dstArrayElement = 0;
            spot_shadow_write.descriptorCount = 4;
            spot_shadow_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            spot_shadow_write.pImageInfo      = spot_shadow_image_infos;
        }

        // BoneMatrices UBO (binding 8) — 使用累积偏移避免 GPU 延迟执行覆盖
        VkDescriptorBufferInfo bone_buf_info{};
        bone_buf_info.buffer = bone_matrices_ubo_;
        bone_buf_info.offset = bone_offset;
        bone_buf_info.range  = 100 * sizeof(glm::mat4);

        VkWriteDescriptorSet bone_write{};
        bone_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bone_write.dstSet          = sets[2];
        bone_write.dstBinding      = 8;
        bone_write.descriptorCount = 1;
        bone_write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bone_write.pBufferInfo     = &bone_buf_info;

        // MorphWeights UBO (binding 9)
        VkDescriptorBufferInfo morph_buf_info{};
        morph_buf_info.buffer = morph_weights_ubo_;
        morph_buf_info.offset = 0;
        morph_buf_info.range  = 16;

        VkWriteDescriptorSet morph_write{};
        morph_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        morph_write.dstSet          = sets[2];
        morph_write.dstBinding      = 9;
        morph_write.descriptorCount = 1;
        morph_write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        morph_write.pBufferInfo     = &morph_buf_info;

        // 合并写入
        std::vector<VkWriteDescriptorSet> all_writes;
        all_writes.push_back(mat_write);
        for (int i = 0; i < 5; ++i) {
            if (tex_writes[i].sType != 0) {
                all_writes.push_back(tex_writes[i]);
            }
        }
        all_writes.push_back(shadow_write);
        all_writes.push_back(spot_shadow_write);
        all_writes.push_back(bone_write);
        all_writes.push_back(morph_write);
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(all_writes.size()), all_writes.data(), 0, nullptr);
    } // else (PBR mode Set 2)

    // --- Set 3 binding 0: 点光源立方体阴影贴图 (u_point_shadow_maps[4]) ---
    if (!gbuffer_mode && set_count >= 4 && sets[3] != VK_NULL_HANDLE) {
        VkDescriptorImageInfo point_shadow_infos[4] = {};
        VkWriteDescriptorSet  point_shadow_write{};
        {
            VkSampler lin_sampler = resource_mgr.default_sampler();
            const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
            for (int i = 0; i < 4; ++i) {
                unsigned int ps_handle = global_state_.point_shadow_map[i];
                const VulkanTexture* ps_tex = (ps_handle != 0)
                    ? resource_mgr.GetTexture(ps_handle) : nullptr;
                if (ps_tex) {
                    point_shadow_infos[i].sampler     = lin_sampler;
                    point_shadow_infos[i].imageView   = ps_tex->image_view;
                    point_shadow_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                } else if (white_tex) {
                    point_shadow_infos[i].sampler     = lin_sampler;
                    point_shadow_infos[i].imageView   = white_tex->image_view;
                    point_shadow_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }
            point_shadow_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            point_shadow_write.dstSet          = sets[3];
            point_shadow_write.dstBinding      = 0;
            point_shadow_write.dstArrayElement = 0;
            point_shadow_write.descriptorCount = 4;
            point_shadow_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            point_shadow_write.pImageInfo      = point_shadow_infos;
        }
        vkUpdateDescriptorSets(device, 1, &point_shadow_write, 0, nullptr);
    }

    // 绑定所有 DescriptorSet
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0,
                            static_cast<uint32_t>(program->descriptor_set_layouts.size()),
                            sets, 0, nullptr);

    return sets[0];
}

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdateSkyboxDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    unsigned int cubemap_texture_handle,
    VulkanResourceManager& resource_mgr) {

    auto device = context_->device();
    uint32_t fi = current_frame_index_;

    // 天空盒着色器使用 Set 0 (PerFrame) + Set 2 (skybox sampler)
    if (program->descriptor_set_layouts.size() < 2) {
        DEBUG_LOG_WARN("Skybox shader program has insufficient descriptor set layouts");
        return VK_NULL_HANDLE;
    }

    const uint32_t set_count = static_cast<uint32_t>(program->descriptor_set_layouts.size());
    std::vector<VkDescriptorSet> sets(set_count, VK_NULL_HANDLE);
    for (uint32_t s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    }

    // 获取 dummy 资源用于填充未使用的绑定
    const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
    VkSampler default_samp = resource_mgr.default_sampler();

    VkDescriptorBufferInfo dummy_ubo_info{};
    dummy_ubo_info.buffer = per_frame_ubo_[fi];
    dummy_ubo_info.offset = 0;
    dummy_ubo_info.range = sizeof(VulkanPerFrameUBO);

    VkDescriptorImageInfo dummy_img_info{};
    dummy_img_info.sampler = default_samp;
    dummy_img_info.imageView = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
    dummy_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 保持所有 image info 的生命周期直到 vkUpdateDescriptorSets
    std::vector<VkDescriptorImageInfo> img_pool(20, dummy_img_info);
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(24);

    auto push_ubo = [&](VkDescriptorSet dstSet, uint32_t binding) {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = dstSet;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &dummy_ubo_info;
        writes.push_back(w);
    };
    auto push_ssbo = [&](VkDescriptorSet dstSet, uint32_t binding) {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = dstSet;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &dummy_ubo_info;
        writes.push_back(w);
    };
    auto push_img = [&](VkDescriptorSet dstSet, uint32_t binding, uint32_t count) -> size_t {
        size_t base = img_pool.size();
        for (uint32_t j = 0; j < count; ++j) img_pool.push_back(dummy_img_info);
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = dstSet;
        w.dstBinding = binding;
        w.descriptorCount = count;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // pImageInfo 稍后通过 base 索引设置（img_pool 可能 realloc）
        writes.push_back(w);
        return base;
    };

    // 记录 <writes_index, img_pool_base> 以便后续修正指针
    std::vector<std::pair<size_t, size_t>> img_fixups;

    // Set 0 (layouts[0]): PerFrame UBO — binding 0（仅当着色器反射包含该绑定时写入）
    bool set0_has_ubo = false;
    for (const auto& b : program->reflection.bindings) {
        if (b.set == 0 && b.binding == 0) { set0_has_ubo = true; break; }
    }
    if (set0_has_ubo) {
        push_ubo(sets[0], 0);
    }

    // Set 1 (layouts[1]): PerScene(b0) + PointLightSSBO(b1) + SpotLightSSBO(b2) + ClusterInfo(b3) + LightIndex(b4)
    if (set_count > 1) {
        push_ubo(sets[1], 0);
        push_ssbo(sets[1], 1);
        push_ssbo(sets[1], 2);
        push_ssbo(sets[1], 3);
        push_ssbo(sets[1], 4);
    }

    // Set 2 (layouts[2]): PerMaterial(b0) + textures(b1-5) + shadow(b6,b7) + bone(b8,b9)
    if (set_count > 2) {
        push_ubo(sets[2], 0);

        // binding 1: skybox cubemap（实际纹理）
        const VulkanTexture* cubemap_tex = resource_mgr.GetTexture(cubemap_texture_handle);
        {
            size_t base = img_pool.size();
            VkDescriptorImageInfo ci{};
            ci.sampler = default_samp;
            ci.imageView = cubemap_tex ? cubemap_tex->image_view
                                       : (white_tex ? white_tex->image_view : VK_NULL_HANDLE);
            ci.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img_pool.push_back(ci);
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = sets[2];
            w.dstBinding = 1;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes.push_back(w);
            img_fixups.push_back({writes.size() - 1, base});
        }

        // binding 2-5: dummy textures
        for (uint32_t b = 2; b <= 5; ++b) {
            size_t base = push_img(sets[2], b, 1);
            img_fixups.push_back({writes.size() - 1, base});
        }

        // binding 6: shadow_map[3]
        { size_t base = push_img(sets[2], 6, 3); img_fixups.push_back({writes.size()-1, base}); }

        // binding 7: spot_shadow_map[4]
        { size_t base = push_img(sets[2], 7, 4); img_fixups.push_back({writes.size()-1, base}); }

        // binding 8-9: BoneMatrices / MorphWeights UBO
        push_ubo(sets[2], 8);
        push_ubo(sets[2], 9);
    }

    // Set 3 (layouts[3]): point_shadow_maps[4] — binding 0
    if (set_count > 3) {
        size_t base = push_img(sets[3], 0, 4);
        img_fixups.push_back({writes.size() - 1, base});
    }

    // 修正所有 image 描述符的指针（img_pool 不再 realloc）
    for (auto& [wi, base] : img_fixups) {
        writes[wi].pImageInfo = &img_pool[base];
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0,
                            set_count, sets.data(), 0, nullptr);

    return sets[0];
}

// ============================================================================
// AllocateAllSetsWithDummies — 分配全部 descriptor sets 并用 dummy 数据填满
// ============================================================================
std::vector<VkDescriptorSet> VulkanDrawExecutor::AllocateAllSetsWithDummies(
    const VulkanShaderProgram* program,
    VulkanResourceManager& resource_mgr) {

    auto device = context_->device();
    uint32_t fi = current_frame_index_;
    const uint32_t set_count = static_cast<uint32_t>(program->descriptor_set_layouts.size());

    std::vector<VkDescriptorSet> sets(set_count, VK_NULL_HANDLE);
    for (uint32_t s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return {};
    }

    const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
    VkSampler samp = resource_mgr.default_sampler();

    VkDescriptorBufferInfo dummy_ubo{};
    dummy_ubo.buffer = per_frame_ubo_[fi];
    dummy_ubo.offset = 0;
    dummy_ubo.range  = sizeof(VulkanPerFrameUBO);

    VkDescriptorImageInfo dummy_img{};
    dummy_img.sampler     = samp;
    dummy_img.imageView   = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
    dummy_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 预分配 image info 池（避免指针悬空）
    std::vector<VkDescriptorImageInfo> img_pool;
    img_pool.reserve(32);
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(24);
    std::vector<std::pair<size_t, size_t>> fixups; // <write_idx, img_pool_base>

    auto push_ubo = [&](uint32_t set_idx, uint32_t binding) {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets[set_idx];
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &dummy_ubo;
        writes.push_back(w);
    };
    auto push_ssbo2 = [&](uint32_t set_idx, uint32_t binding) {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets[set_idx];
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &dummy_ubo;
        writes.push_back(w);
    };
    auto push_img = [&](uint32_t set_idx, uint32_t binding, uint32_t count) {
        size_t base = img_pool.size();
        for (uint32_t j = 0; j < count; ++j) img_pool.push_back(dummy_img);
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets[set_idx];
        w.dstBinding = binding;
        w.descriptorCount = count;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes.push_back(w);
        fixups.push_back({writes.size() - 1, base});
    };

    // Set 0: binding 0 (PerFrame UBO)
    if (set_count > 0) push_ubo(0, 0);

    // Set 1: binding 0 (PerScene UBO), 1-2 (Point/SpotLightSSBO), 3-4 (ClusterInfo/LightIndex SSBO), 5 (LightProbeData UBO)
    if (set_count > 1) { push_ubo(1, 0); push_ssbo2(1, 1); push_ssbo2(1, 2); push_ssbo2(1, 3); push_ssbo2(1, 4); push_ubo(1, 5); }

    // Set 2: binding 0 (PerMaterial UBO), 1-5 (textures), 6 (shadow[3]), 7 (spot_shadow[4]), 8-9 (bones/morph UBO)
    if (set_count > 2) {
        push_ubo(2, 0);
        for (uint32_t b = 1; b <= 5; ++b) push_img(2, b, 1);
        push_img(2, 6, 3);
        push_img(2, 7, 4);
        push_ubo(2, 8);
        push_ubo(2, 9);
    }

    // Set 3: binding 0 (point_shadow_maps[4])
    if (set_count > 3) push_img(3, 0, 4);

    // 修正 image 指针
    for (auto& [wi, base] : fixups) writes[wi].pImageInfo = &img_pool[base];

    if (!writes.empty())
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return sets;
}

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdateParticleDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    unsigned int texture_handle,
    VulkanResourceManager& resource_mgr) {

    if (program->descriptor_set_layouts.empty()) return VK_NULL_HANDLE;

    auto device = context_->device();
    uint32_t fi = current_frame_index_;
    const uint32_t set_count = static_cast<uint32_t>(program->descriptor_set_layouts.size());

    // 分配所有 set（包括空 layout 的 set）
    std::vector<VkDescriptorSet> sets(set_count, VK_NULL_HANDLE);
    for (uint32_t s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    }

    std::vector<VkWriteDescriptorSet> writes;

    // Set 0 binding 0: PerFrame UBO（particle VS 使用此 binding；sprite VS 已改用 push constants）
    // 通过 SPIR-V reflection 检查 set 0 是否有 bindings
    VkDescriptorBufferInfo frame_buf{};
    frame_buf.buffer = per_frame_ubo_[fi];
    frame_buf.offset = 0;
    frame_buf.range  = sizeof(VulkanPerFrameUBO);
    bool set0_has_ubo = false;
    for (const auto& b : program->reflection.bindings) {
        if (b.set == 0 && b.binding == 0) { set0_has_ubo = true; break; }
    }
    if (set_count > 0 && set0_has_ubo) {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets[0]; w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &frame_buf;
        writes.push_back(w);
    }

    // Set 2 binding 1: 纹理（sprite/particle FS 都使用此 binding）
    VkDescriptorImageInfo img_info{};
    unsigned int tex_h = (texture_handle != 0) ? texture_handle : white_texture_handle_;
    const VulkanTexture* tex = resource_mgr.GetTexture(tex_h);
    if (!tex) tex = resource_mgr.GetTexture(white_texture_handle_);
    if (tex && set_count > 2) {
        img_info.sampler     = resource_mgr.default_sampler();
        img_info.imageView   = tex->image_view;
        img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets[2]; w.dstBinding = 1;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &img_info;
        writes.push_back(w);
    }

    if (!writes.empty())
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0,
                            set_count, sets.data(), 0, nullptr);
    return sets[0];
}

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdatePostProcessDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    unsigned int source_texture,
    VulkanResourceManager& resource_mgr,
    const std::vector<std::pair<uint32_t, unsigned int>>& extra_bindings) {

    if (program->descriptor_set_layouts.empty()) return VK_NULL_HANDLE;

    auto device = context_->device();
    const uint32_t set_count = static_cast<uint32_t>(program->descriptor_set_layouts.size());

    // 分配所有 set（包括空 layout 的 set）
    std::vector<VkDescriptorSet> sets(set_count, VK_NULL_HANDLE);
    for (uint32_t s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    }

    // 后处理 shader 仅使用 set 2, binding 1 (screenTexture)
    // 只写实际存在的 bindings，避免写入空 layout 或不存在的 binding
    VkDescriptorImageInfo src_img{};
    const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
    src_img.sampler     = resource_mgr.default_sampler();
    src_img.imageView   = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
    src_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (source_texture != 0) {
        const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(source_texture);
        if (rt && rt->color_texture.image_view != VK_NULL_HANDLE) {
            src_img.imageView = rt->color_texture.image_view;
            DEBUG_LOG_TRACE("[Vulkan] PostProcess src: RT handle={} imageView={}", source_texture, (void*)rt->color_texture.image_view);
        } else {
            const VulkanTexture* tex = resource_mgr.GetTexture(source_texture);
            if (tex) {
                src_img.imageView = tex->image_view;
                DEBUG_LOG_TRACE("[Vulkan] PostProcess src: Texture handle={} imageView={}", source_texture, (void*)tex->image_view);
            } else {
                DEBUG_LOG_WARN("[Vulkan] PostProcess src: handle={} NOT FOUND as RT or Texture, using dummy", source_texture);
            }
        }
    }

    if (set_count > 2) {
        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorImageInfo> extra_imgs(extra_bindings.size());

        // binding 1: screenTexture
        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = sets[2];
        w.dstBinding      = 1;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo      = &src_img;
        writes.push_back(w);

        // extra bindings (bloom, ssao, ae, lut 等) — 仅写 shader 反射中存在的 binding
        for (size_t i = 0; i < extra_bindings.size(); ++i) {
            auto [binding, tex_handle] = extra_bindings[i];
            bool binding_exists = false;
            for (const auto& b : program->reflection.bindings) {
                if (b.set == 2 && b.binding == binding) { binding_exists = true; break; }
            }
            if (!binding_exists) continue;
            VkDescriptorImageInfo& ei = extra_imgs[i];
            ei.sampler     = resource_mgr.default_sampler();
            ei.imageView   = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
            ei.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            if (tex_handle != 0) {
                const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(tex_handle);
                if (rt && rt->color_texture.image_view != VK_NULL_HANDLE) {
                    ei.imageView = rt->color_texture.image_view;
                } else {
                    const VulkanTexture* tex = resource_mgr.GetTexture(tex_handle);
                    if (tex) {
                        ei.imageView = tex->image_view;
                        if (tex->sampler != VK_NULL_HANDLE) ei.sampler = tex->sampler;
                    }
                }
            }

            VkWriteDescriptorSet ew{};
            ew.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ew.dstSet          = sets[2];
            ew.dstBinding      = binding;
            ew.descriptorCount = 1;
            ew.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            ew.pImageInfo      = &extra_imgs[i];
            writes.push_back(ew);
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0,
                            set_count, sets.data(), 0, nullptr);
    return sets[0];
}

// ============================================================================
// RenderPass
// ============================================================================

void VulkanDrawExecutor::BeginRenderPass(
    VkCommandBuffer cmd_buf,
    const RenderPassDesc& render_pass,
    VulkanResourceManager& resource_mgr,
    VulkanPipelineStateManager& pipeline_mgr) {

    // 更新帧索引和当前 RT 句柄
    current_frame_index_ = context_->current_frame() % MAX_FRAMES;
    current_rt_handle_ = render_pass.render_target;

    // 确定 Framebuffer 和 RenderPass
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    if (render_pass.render_target != 0) {
        const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt) {
            framebuffer = rt->framebuffer;
            vk_render_pass = rt->render_pass;
        }
    } else {
        // 渲染到屏幕：使用当前 swapchain framebuffer
        framebuffer = context_->current_swapchain_framebuffer();
        vk_render_pass = context_->swapchain_render_pass();
    }

    if (framebuffer == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::BeginRenderPass: framebuffer is VK_NULL_HANDLE");
        return;
    }
    if (vk_render_pass == VK_NULL_HANDLE) {
        // fallback: 使用 pipeline_mgr 缓存的 RenderPass
        VulkanPipelineStateManager::RenderPassKey rp_key;
        rp_key.has_color = true;
        rp_key.has_depth = true;
        rp_key.color_clear = render_pass.clear_color_enabled;
        rp_key.depth_clear = true;
        vk_render_pass = pipeline_mgr.GetOrCreateRenderPass(rp_key);
    }

    if (vk_render_pass == VK_NULL_HANDLE) {
        DEBUG_LOG_ERROR("Failed to get VkRenderPass for BeginRenderPass");
        return;
    }

    // 记录当前激活的 render pass，供后续 Draw 函数创建 pipeline 时使用
    current_render_pass_ = vk_render_pass;

    // 确定渲染区域大小和 MSAA 采样数
    VkExtent2D render_extent = context_->swapchain_extent();
    current_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
    if (render_pass.render_target != 0) {
        const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt && rt->width > 0 && rt->height > 0) {
            render_extent.width = static_cast<uint32_t>(rt->width);
            render_extent.height = static_cast<uint32_t>(rt->height);
        }
        if (rt && rt->is_msaa && rt->msaa_samples > 1) {
            current_msaa_samples_ = static_cast<VkSampleCountFlagBits>(rt->msaa_samples);
        }
    }

    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = vk_render_pass;
    begin_info.framebuffer = framebuffer;
    begin_info.renderArea.offset = {0, 0};
    begin_info.renderArea.extent = render_extent;

    // MSAA render target 有 3 个 attachments：MSAA color, depth, resolve
    const bool is_msaa_rt = (current_msaa_samples_ != VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkClearValue> clear_values;
    VkClearValue color_cv{};
    color_cv.color = {{render_pass.clear_color.x,
                        render_pass.clear_color.y,
                        render_pass.clear_color.z,
                        render_pass.clear_color.w}};
    VkClearValue depth_cv{};
    depth_cv.depthStencil = {1.0f, 0};

    clear_values.push_back(color_cv);       // attachment 0: color (or MSAA color)
    clear_values.push_back(depth_cv);       // attachment 1: depth
    if (is_msaa_rt) {
        clear_values.push_back(color_cv);   // attachment 2: resolve target
    }

    begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    begin_info.pClearValues = clear_values.data();

    // 二分法诊断：跳过超限的 render pass（在 vkCmdBeginRenderPass 之前检查）
    if (max_render_passes_ >= 0 && render_pass_counter_ >= max_render_passes_) {
        skip_current_pass_ = true;
        DEBUG_LOG_TRACE("[Vulkan] BeginRenderPass: SKIPPED rt={} (pass {} >= max {})",
                       render_pass.render_target, render_pass_counter_, max_render_passes_);
        render_pass_counter_++;
        return;
    }
    skip_current_pass_ = false;
    render_pass_counter_++;

    vkCmdBeginRenderPass(cmd_buf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    {
        bool rt_has_color = true, rt_has_depth = false;
        if (render_pass.render_target != 0) {
            const VulkanRenderTarget* rt_info = resource_mgr.GetRenderTarget(render_pass.render_target);
            if (rt_info) { rt_has_color = rt_info->has_color; rt_has_depth = rt_info->has_depth; }
        }
        current_color_attachment_count_ = rt_has_color ? 1 : 0;
        DEBUG_LOG_TRACE("[Vulkan] BeginRenderPass: rt={} extent={}x{} msaa={} color={} depth={} pass#={}",
                       render_pass.render_target,
                       render_extent.width, render_extent.height,
                       static_cast<int>(current_msaa_samples_),
                       rt_has_color, rt_has_depth,
                       render_pass_counter_ - 1);
    }

    // 设置动态 viewport 和 scissor（pipeline 使用 VK_DYNAMIC_STATE_VIEWPORT/SCISSOR）
    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(render_extent.width);
    vp.height = static_cast<float>(render_extent.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buf, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = render_extent;
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);
}

void VulkanDrawExecutor::EndRenderPass(VkCommandBuffer cmd_buf) {
    if (skip_current_pass_) {
        DEBUG_LOG_TRACE("[Vulkan] EndRenderPass: SKIPPED rt={}", current_rt_handle_);
        skip_current_pass_ = false;
        return;
    }
    DEBUG_LOG_TRACE("[Vulkan] EndRenderPass: rt={}", current_rt_handle_);
    vkCmdEndRenderPass(cmd_buf);

    // barrier：确保 color/depth writes 在后续 shader reads 可见
    VkMemoryBarrier mem_barrier{};
    mem_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mem_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                              | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    mem_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
                              | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 1, &mem_barrier, 0, nullptr, 0, nullptr);

    current_render_pass_ = VK_NULL_HANDLE;
    current_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
}

// ============================================================================
// DrawSpriteBatch — 2D 精灵批处理
// ============================================================================

void VulkanDrawExecutor::DrawSpriteBatch(
    VkCommandBuffer cmd_buf,
    const std::vector<SpriteDrawItem>& items,
    const glm::mat4& view,
    const glm::mat4& projection,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr,
    VulkanResourceManager& resource_mgr) {

    DEBUG_LOG_TRACE("[Vulkan] DrawSpriteBatch: items={} skip={}", items.size(), skip_current_pass_);
    if (items.empty() || skip_current_pass_) return;

    const VulkanShaderProgram* sprite_program = shader_mgr.GetProgram(shader_mgr.sprite_shader_handle());
    if (!sprite_program) {
        // Fallback：无 sprite shader 时仅统计
        global_state_.current_frame_stats.draw_calls++;
        global_state_.current_frame_stats.sprite_count += static_cast<int>(items.size());
        return;
    }

    // 2D 精灵管线状态：alpha 混合、无深度写、无剔除（缓存 handle 避免每帧重建）
    if (sprite_pipeline_state_ == 0) {
        PipelineStateDesc sprite_desc;
        sprite_desc.blend_enabled = true;
        sprite_desc.blend_src = BlendFactor::SrcAlpha;
        sprite_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        sprite_desc.depth_test_enabled = false;
        sprite_desc.depth_write_enabled = false;
        sprite_desc.culling_enabled = false;
        sprite_pipeline_state_ = pipeline_mgr.CreatePipelineState(sprite_desc);
    }
    unsigned int sprite_state = sprite_pipeline_state_;

    // 使用当前激活的 render pass（可能是离屏 RT 的 render pass）
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // 延迟创建 Pipeline
    VkPipeline sprite_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        sprite_state, sprite_program, active_rp,
        // 2D 精灵顶点格式：vec2 pos, vec2 texcoord, vec4 color = 8 floats = 32 bytes
        { VkVertexInputBindingDescription{0, 32, VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},          // aPos
            {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},          // aTexCoord
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},   // aColor
        },
        context_->swapchain_extent(),
        current_msaa_samples_,
        current_color_attachment_count_);

    if (sprite_pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to create sprite pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, sprite_pipeline);

    // VP 矩阵通过 push constants 传递（避免 per_frame_ubo_ 被后续 pass 覆盖）
    const glm::mat4 sprite_vp = projection * view;

    // 逐精灵组绘制（按纹理分组批处理可优化，当前逐个绘制）
    // 精灵顶点格式：vec2 pos, vec2 uv, vec4 color
    struct SpriteVertex {
        float pos[2];
        float uv[2];
        float color[4];
    };

    int sprite_idx = 0;
    VkDeviceSize sprite_vbo_offset = 0;

    for (const auto& item : items) {
        sprite_idx++;

        // UV 解释逻辑与 OpenGL/DX11 DrawBatch 保持一致
        float u_min = item.uv.x, v_min = item.uv.y;
        float u_max, v_max;
        if (item.uv.z > 0.0f && item.uv.w > 0.0f) {
            const bool use_max_uv = item.uv.z > item.uv.x && item.uv.w > item.uv.y;
            u_max = use_max_uv ? item.uv.z : (item.uv.x + item.uv.z);
            v_max = use_max_uv ? item.uv.w : (item.uv.y + item.uv.w);
        } else {
            u_min = 0.0f; v_min = 0.0f; u_max = 1.0f; v_max = 1.0f;
        }

        // NeedsTextureYFlip=true → 纹理已 Y-flip 加载，UV 直接使用
        float r = item.color.r, g = item.color.g, b = item.color.b, a = item.color.a;
        SpriteVertex vertices[4] = {
            {{-0.5f, -0.5f}, {u_min, v_min}, {r, g, b, a}},  // BL
            {{ 0.5f, -0.5f}, {u_max, v_min}, {r, g, b, a}},  // BR
            {{ 0.5f,  0.5f}, {u_max, v_max}, {r, g, b, a}},  // TR
            {{-0.5f,  0.5f}, {u_min, v_max}, {r, g, b, a}},  // TL
        };

        uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

        // 上传顶点到累积偏移
        VkDeviceSize cur_vbo_offset = sprite_vbo_offset;
        WriteToBuffer(context_->device(), sprite_vbo_mem_, sprite_vbo_offset, sizeof(vertices), vertices);
        sprite_vbo_offset += sizeof(vertices);
        if (sprite_idx == 1) {
            WriteToBuffer(context_->device(), sprite_ibo_mem_, 0, sizeof(indices), indices);
        }

        // Push constants: model + VP（128 bytes）— 使用实际 model 矩阵
        struct { glm::mat4 model; glm::mat4 vp; } push_data;
        push_data.model = item.model;
        push_data.vp = sprite_vp;
        vkCmdPushConstants(cmd_buf, sprite_program->pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(push_data), &push_data);

        // 分配并绑定 DescriptorSet
        AllocateAndUpdateParticleDescriptorSets(cmd_buf, sprite_program,
                                                 item.texture_handle, resource_mgr);

        // 绑定 VBO + IBO
        VkDeviceSize vbo_offsets[] = {cur_vbo_offset};
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &sprite_vbo_, vbo_offsets);
        vkCmdBindIndexBuffer(cmd_buf, sprite_ibo_, 0, VK_INDEX_TYPE_UINT16);

        vkCmdDrawIndexed(cmd_buf, 6, 1, 0, 0, 0);

        global_state_.current_frame_stats.draw_calls++;
    }

    global_state_.current_frame_stats.sprite_count += static_cast<int>(items.size());
}

// ============================================================================
// DrawMeshBatch — 3D PBR 网格绘制
// ============================================================================

void VulkanDrawExecutor::DrawMeshBatch(
    VkCommandBuffer cmd_buf,
    const std::vector<MeshDrawItem>& items,
    const glm::mat4& view,
    const glm::mat4& projection,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr,
    VulkanResourceManager& resource_mgr) {

    if (items.empty()) return;
    if (skip_current_pass_) return;

    const bool gbuffer_mode = global_state_.gbuffer_rendering_mode;
    unsigned int active_shader_handle = gbuffer_mode
        ? shader_mgr.gbuffer_shader_handle()
        : shader_mgr.pbr_shader_handle();
    const VulkanShaderProgram* pbr_program = shader_mgr.GetProgram(active_shader_handle);
    if (!pbr_program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: {} shader not available", gbuffer_mode ? "GBuffer" : "PBR");
        return;
    }

    // 使用当前激活的 render pass（可能是离屏 RT 的 render pass）
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // 3D Mesh 顶点格式定义（两种管线共用）
    std::vector<VkVertexInputBindingDescription> mesh_bindings = {
        {0, sizeof(BatchVertex), VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(glm::mat4), VK_VERTEX_INPUT_RATE_INSTANCE},  // GPU Instancing: model matrix per instance
    };
    std::vector<VkVertexInputAttributeDescription> mesh_attrs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},       // aPos
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12},   // aColor
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 28},          // aTexCoord
        {3, 0, VK_FORMAT_R32G32B32_SFLOAT, 36},       // aNormal
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 48},       // aTangent
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 60},   // aBoneWeights
        {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 76},   // aBoneIndices
        // GPU Instancing: instance model matrix (4 columns, binding 1)
        {7,  1, VK_FORMAT_R32G32B32A32_SFLOAT, 0},                       // aInstModelCol0
        {8,  1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4)},       // aInstModelCol1
        {9,  1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 2},   // aInstModelCol2
        {10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(glm::vec4) * 3},   // aInstModelCol3
    };

    // 延迟创建 VkPipeline（按需，首次 Draw 时创建）— 有剔除版本
    VkPipeline vk_pipeline_cull = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        pbr_program, active_rp, mesh_bindings, mesh_attrs,
        context_->swapchain_extent(), current_msaa_samples_,
        current_color_attachment_count_);

    // 双面材质需要无剔除管线（与 OpenGL 的 material_double_sided 行为对齐）
    if (nocull_pipeline_state_ == 0) {
        PipelineStateDesc nocull_desc;
        nocull_desc.culling_enabled = false;
        nocull_desc.depth_test_enabled = true;
        nocull_desc.depth_write_enabled = true;
        nocull_desc.depth_func = CompareFunc::Less;
        nocull_desc.blend_enabled = false;
        nocull_pipeline_state_ = pipeline_mgr.CreatePipelineState(nocull_desc);
    }
    VkPipeline vk_pipeline_nocull = pipeline_mgr.GetOrCreateVkPipeline(
        nocull_pipeline_state_,
        pbr_program, active_rp, mesh_bindings, mesh_attrs,
        context_->swapchain_extent(), current_msaa_samples_,
        current_color_attachment_count_);

    if (vk_pipeline_cull == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to get/create VkPipeline for mesh draw");
        return;
    }

    // 初始绑定带剔除的管线
    VkPipeline current_bound_pipeline = VK_NULL_HANDLE;

    // PerFrame UBO: 每个 batch 写一次（所有 item 共享同一 view/projection）
    VkDeviceSize cur_per_frame_offset = per_frame_ubo_offset_;
    UpdatePerFrameUBO(view, projection, {});
    per_frame_ubo_offset_ += kUboSlotAlignment;

    // 逐 mesh 绘制
    for (const auto& item : items) {
        // 双面材质切换管线（与 OpenGL 的 material_double_sided 行为对齐）
        VkPipeline desired_pipeline = item.material_double_sided
            ? vk_pipeline_nocull : vk_pipeline_cull;
        if (desired_pipeline != current_bound_pipeline) {
            vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, desired_pipeline);
            current_bound_pipeline = desired_pipeline;
        }

        // 更新 per-item UBO 数据（每个 item 独立 slot，避免 GPU 延迟执行覆盖）
        VkDeviceSize cur_scene_offset    = per_scene_ubo_offset_;
        VkDeviceSize cur_material_offset = per_material_ubo_offset_;
        VkDeviceSize cur_pl_offset       = 0; // 光源数据由 SSBO 提供，不再 per-draw UBO
        VkDeviceSize cur_sl_offset       = 0;
        UpdatePerSceneUBO(item);
        UpdatePerMaterialUBO(item);
        // 光源数据由 LightBuffer SSBO 提供，跳过 per-draw UBO 上传
        per_scene_ubo_offset_        += kUboSlotAlignment;
        per_material_ubo_offset_     += kUboSlotAlignment;

        // 上传骨骼矩阵到 UBO（累积偏移，避免 GPU 延迟执行时覆盖前面 mesh 的数据）
        VkDeviceSize cur_bone_offset = bone_matrices_offset_;
        if (item.skinned && !item.bone_matrices.empty()) {
            size_t count = (std::min)(item.bone_matrices.size(), static_cast<size_t>(100));
            size_t bone_data_size = count * sizeof(glm::mat4);
            WriteToBuffer(context_->device(), bone_matrices_ubo_mem_, bone_matrices_offset_,
                          bone_data_size, item.bone_matrices.data());
            bone_matrices_offset_ += 100 * sizeof(glm::mat4); // 固定步长，保持对齐
        }

        // 分配并绑定 DescriptorSet（传入各 UBO 的当前偏移）
        AllocateAndUpdateMeshDescriptorSets(cmd_buf, pbr_program, item, resource_mgr,
            cur_bone_offset, cur_per_frame_offset, cur_scene_offset,
            cur_material_offset, cur_pl_offset, cur_sl_offset, gbuffer_mode);

        // 上传顶点数据到 mesh VBO（累积偏移，避免后续 mesh 覆盖前面的数据）
        VkDeviceSize cur_vbo_offset = mesh_vbo_offset_;
        if (!item.vertices.empty()) {
            size_t vdata_size = item.vertices.size() * sizeof(item.vertices[0]);
            WriteToBuffer(context_->device(), mesh_vbo_mem_, mesh_vbo_offset_, vdata_size, item.vertices.data());
            mesh_vbo_offset_ += vdata_size;
        }

        // 上传索引数据到 mesh IBO（累积偏移）
        VkDeviceSize cur_ibo_offset = mesh_ibo_offset_;
        if (!item.indices.empty()) {
            size_t idata_size = item.indices.size() * sizeof(item.indices[0]);
            WriteToBuffer(context_->device(), mesh_ibo_mem_, mesh_ibo_offset_, idata_size, item.indices.data());
            mesh_ibo_offset_ += idata_size;
        }

        // Push constant: model + skinned + morph_enabled + use_instancing
        const bool is_instanced = item.instance_transforms.size() > 1;
        struct {
            glm::mat4 model;
            int skinned;
            int morph_enabled;
            int use_instancing;
        } pc_data;
        pc_data.model = item.model;
        pc_data.skinned = item.skinned ? 1 : 0;
        pc_data.morph_enabled = item.morph_enabled ? 1 : 0;
        pc_data.use_instancing = is_instanced ? 1 : 0;
        vkCmdPushConstants(cmd_buf, pbr_program->pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(pc_data), &pc_data);

        // GPU Instancing: 上传 instance 数据（在绑定之前处理可能的扩容）
        uint32_t instance_count = 1;
        if (is_instanced) {
            const VkDeviceSize inst_data_size = item.instance_transforms.size() * sizeof(glm::mat4);

            // 动态扩容 instance VBO
            if (inst_data_size > instance_vbo_capacity_) {
                auto device = context_->device();
                if (instance_vbo_ != VK_NULL_HANDLE) {
                    vkDestroyBuffer(device, instance_vbo_, nullptr);
                    instance_vbo_ = VK_NULL_HANDLE;
                }
                if (instance_vbo_mem_ != VK_NULL_HANDLE) {
                    vkFreeMemory(device, instance_vbo_mem_, nullptr);
                    instance_vbo_mem_ = VK_NULL_HANDLE;
                }
                instance_vbo_capacity_ = inst_data_size * 2;
                CreateVulkanBuffer(context_->device(), context_->physical_device(),
                                   instance_vbo_capacity_,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   instance_vbo_, instance_vbo_mem_);
            }

            WriteToBuffer(context_->device(), instance_vbo_mem_, 0, inst_data_size, item.instance_transforms.data());
            instance_count = static_cast<uint32_t>(item.instance_transforms.size());
        }

        // 绑定 VBO binding 0 (mesh) + binding 1 (instance)
        VkBuffer vbo_buffers[] = {mesh_vbo_, instance_vbo_};
        VkDeviceSize vbo_offsets[] = {cur_vbo_offset, 0};
        vkCmdBindVertexBuffers(cmd_buf, 0, 2, vbo_buffers, vbo_offsets);

        // 绑定 IBO（使用该 mesh 的偏移）
        vkCmdBindIndexBuffer(cmd_buf, mesh_ibo_, cur_ibo_offset, VK_INDEX_TYPE_UINT16);

        // 绘制
        vkCmdDrawIndexed(cmd_buf,
                         static_cast<uint32_t>(item.indices.size()),
                         instance_count, 0, 0, 0);

        if (is_instanced) {
            global_state_.current_frame_stats.instanced_draw_calls++;
            global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(instance_count);
        }
        global_state_.current_frame_stats.draw_calls++;
    }

    global_state_.current_frame_stats.mesh_count += static_cast<int>(items.size());
}

// ============================================================================
// DrawSkybox — 天空盒绘制
// ============================================================================

void VulkanDrawExecutor::DrawSkybox(
    VkCommandBuffer cmd_buf,
    unsigned int cubemap_texture_handle,
    const glm::mat4& view,
    const glm::mat4& projection,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr) {

    if (skip_current_pass_) return;
    const VulkanShaderProgram* skybox_program = shader_mgr.GetProgram(shader_mgr.skybox_shader_handle());
    if (!skybox_program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: Skybox shader not available");
        return;
    }

    // 使用当前激活的 render pass
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // 天空盒专用管线状态：depth test=LEQUAL, depth write=OFF, no cull（缓存 handle 避免每帧重建）
    if (skybox_pipeline_state_ == 0) {
        PipelineStateDesc skybox_desc;
        skybox_desc.depth_test_enabled = true;
        skybox_desc.depth_write_enabled = false;
        skybox_desc.depth_func = CompareFunc::LessEqual;
        skybox_desc.culling_enabled = false;
        skybox_desc.blend_enabled = false;
        skybox_pipeline_state_ = pipeline_mgr.CreatePipelineState(skybox_desc);
    }
    unsigned int skybox_state = skybox_pipeline_state_;

    VkPipeline skybox_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        skybox_state,
        skybox_program,
        active_rp,
        // 天空盒顶点格式：vec3 pos
        { VkVertexInputBindingDescription{0, 12, VK_VERTEX_INPUT_RATE_VERTEX} },
        { {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0} },
        context_->swapchain_extent(),
        current_msaa_samples_,
        current_color_attachment_count_);

    if (skybox_pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to create skybox pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline);

    // Push constants: skybox VP（去除 view 平移，仅保留旋转）
    glm::mat4 skybox_view = glm::mat4(glm::mat3(view));
    glm::mat4 skybox_vp = projection * skybox_view;
    vkCmdPushConstants(cmd_buf, skybox_program->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skybox_vp);

    AllocateAndUpdateSkyboxDescriptorSets(cmd_buf, skybox_program, cubemap_texture_handle,
                                           *resource_mgr_);

    // 绑定天空盒 VBO
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &skybox_vbo_, offsets);

    // 36 个顶点（6 面 * 2 三角形 * 3 顶点）
    vkCmdDraw(cmd_buf, 36, 1, 0, 0);

    global_state_.current_frame_stats.draw_calls++;
}

// ============================================================================
// DrawPostProcess — 后处理绘制
// ============================================================================

void VulkanDrawExecutor::DrawPostProcess(
    VkCommandBuffer cmd_buf,
    unsigned int source_texture,
    const std::string& effect_name,
    const std::vector<float>& params,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr) {

    DEBUG_LOG_TRACE("[Vulkan] DrawPostProcess: effect='{}' source_texture={} skip={}", effect_name, source_texture, skip_current_pass_);
    if (skip_current_pass_) return;

    // Bloom CS 路径：绕过图形管线，直接 Dispatch
    const bool is_bloom_ds = (effect_name == "bloom_downsample");
    const bool is_bloom_us = (effect_name == "bloom_upsample");
    if ((is_bloom_ds || is_bloom_us) && current_rt_handle_ != 0) {
        const unsigned int cs_handle = is_bloom_ds
            ? shader_mgr.bloom_downsample_cs_handle()
            : shader_mgr.bloom_upsample_cs_handle();
        if (cs_handle != 0) {
            DispatchBloomCompute(cmd_buf, cs_handle, source_texture,
                                  current_rt_handle_, shader_mgr);
            return;
        }
    }

    // 后处理管线状态：无深度、无剪裁；ui_overlay 需要 alpha 混合（缓存 handle 避免每帧重建）
    const bool needs_blend = (effect_name == "ui_overlay");
    if (needs_blend && pp_blend_pipeline_state_ == 0) {
        PipelineStateDesc pp_desc;
        pp_desc.blend_enabled = true;
        pp_desc.blend_src = BlendFactor::SrcAlpha;
        pp_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        pp_desc.depth_test_enabled = false;
        pp_desc.depth_write_enabled = false;
        pp_desc.culling_enabled = false;
        pp_blend_pipeline_state_ = pipeline_mgr.CreatePipelineState(pp_desc);
    }
    if (!needs_blend && pp_pipeline_state_ == 0) {
        PipelineStateDesc pp_desc;
        pp_desc.blend_enabled = false;
        pp_desc.depth_test_enabled = false;
        pp_desc.depth_write_enabled = false;
        pp_desc.culling_enabled = false;
        pp_pipeline_state_ = pipeline_mgr.CreatePipelineState(pp_desc);
    }
    unsigned int pp_state = needs_blend ? pp_blend_pipeline_state_ : pp_pipeline_state_;

    // 根据 effect 名称选择专用着色器
    unsigned int selected_shader_handle = shader_mgr.postprocess_shader_handle();
    if (effect_name == "fxaa" && shader_mgr.fxaa_shader_handle())
        selected_shader_handle = shader_mgr.fxaa_shader_handle();
    else if (effect_name == "ssao" && shader_mgr.ssao_shader_handle())
        selected_shader_handle = shader_mgr.ssao_shader_handle();
    else if (effect_name == "ssao_blur" && shader_mgr.ssao_blur_shader_handle())
        selected_shader_handle = shader_mgr.ssao_blur_shader_handle();
    else if (effect_name == "lum_compute" && shader_mgr.lum_compute_shader_handle())
        selected_shader_handle = shader_mgr.lum_compute_shader_handle();
    else if (effect_name == "lum_adapt" && shader_mgr.lum_adapt_shader_handle())
        selected_shader_handle = shader_mgr.lum_adapt_shader_handle();
    else if (effect_name == "tonemapping" && shader_mgr.tonemapping_shader_handle())
        selected_shader_handle = shader_mgr.tonemapping_shader_handle();
    else if (effect_name == "bloom_composite" && shader_mgr.bloom_composite_ssao_ae_shader_handle())
        selected_shader_handle = shader_mgr.bloom_composite_ssao_ae_shader_handle();
    else if (effect_name == "color_grading" && shader_mgr.color_grading_shader_handle())
        selected_shader_handle = shader_mgr.color_grading_shader_handle();
    else if (effect_name == "contact_shadow" && shader_mgr.contact_shadow_shader_handle())
        selected_shader_handle = shader_mgr.contact_shadow_shader_handle();
    else if (effect_name == "taa_resolve" && shader_mgr.taa_resolve_shader_handle())
        selected_shader_handle = shader_mgr.taa_resolve_shader_handle();
    else if (effect_name == "dof" && shader_mgr.dof_shader_handle())
        selected_shader_handle = shader_mgr.dof_shader_handle();
    else if (effect_name == "motion_blur" && shader_mgr.motion_blur_shader_handle())
        selected_shader_handle = shader_mgr.motion_blur_shader_handle();
    else if (effect_name == "ssr" && shader_mgr.ssr_shader_handle())
        selected_shader_handle = shader_mgr.ssr_shader_handle();
    else if (effect_name == "deferred_lighting" && shader_mgr.deferred_lighting_shader_handle())
        selected_shader_handle = shader_mgr.deferred_lighting_shader_handle();
    else if (effect_name == "edge_detect" && shader_mgr.edge_detect_shader_handle())
        selected_shader_handle = shader_mgr.edge_detect_shader_handle();
    else if (effect_name == "volumetric_fog" && shader_mgr.volumetric_fog_shader_handle())
        selected_shader_handle = shader_mgr.volumetric_fog_shader_handle();
    else if (effect_name == "decal" && shader_mgr.decal_shader_handle())
        selected_shader_handle = shader_mgr.decal_shader_handle();
    else if (effect_name == "wboit_composite" && shader_mgr.wboit_composite_shader_handle())
        selected_shader_handle = shader_mgr.wboit_composite_shader_handle();

    const VulkanShaderProgram* pp_program = shader_mgr.GetProgram(selected_shader_handle);
    if (!pp_program) {
        pp_program = shader_mgr.GetProgram(shader_mgr.pbr_shader_handle());
    }

    // 使用当前激活的 render pass
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    VkPipeline pp_pipeline = VK_NULL_HANDLE;
    if (pp_program) {
        pp_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
            pp_state, pp_program, active_rp,
            // 后处理顶点格式：vec2 pos, vec2 texcoord
            { VkVertexInputBindingDescription{0, 16, VK_VERTEX_INPUT_RATE_VERTEX} },
            {
                {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},   // aPos
                {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},   // aTexCoord
            },
            context_->swapchain_extent(),
            current_msaa_samples_,
            current_color_attachment_count_);
    }

    if (pp_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pp_pipeline);
    }

    // 构建额外纹理绑定列表 {set2_binding, texture_handle}
    std::vector<std::pair<uint32_t, unsigned int>> extra_bindings;
    if (effect_name == "bloom_composite") {
        const CompositeParamsView composite(params);
        extra_bindings = {
            {2, composite.Texture(CompositeParamsView::kBloomTex)},
            {3, composite.Texture(CompositeParamsView::kSsaoTex)},
            {4, composite.Texture(CompositeParamsView::kAutoExposureTex)},
            {5, composite.Texture(CompositeParamsView::kLutTex)},
            {6, composite.Texture(CompositeParamsView::kContactShadowTex)}
        };
    } else if (effect_name == "tonemapping") {
        unsigned int ae_h  = (params.size() >= 2) ? static_cast<unsigned int>(params[1]) : 0;
        unsigned int lut_h = (params.size() >= 4) ? static_cast<unsigned int>(params[2]) : 0;
        extra_bindings = {{2, ae_h}, {5, lut_h}};
    } else if (effect_name == "ssao_apply") {
        unsigned int ssao_h = (params.size() >= 1) ? static_cast<unsigned int>(params[0]) : 0;
        unsigned int ae_h   = (params.size() >= 3) ? static_cast<unsigned int>(params[2]) : 0;
        unsigned int lut_h  = (params.size() >= 5) ? static_cast<unsigned int>(params[3]) : 0;
        extra_bindings = {{2, ssao_h}, {3, ae_h}, {5, lut_h}};
    } else if (effect_name == "lum_adapt") {
        unsigned int prev_h = (params.size() >= 1) ? static_cast<unsigned int>(params[0]) : 0;
        extra_bindings = {{2, prev_h}};
    } else if (effect_name == "color_grading") {
        unsigned int lut_h = (params.size() >= 1) ? static_cast<unsigned int>(params[0]) : 0;
        extra_bindings = {{5, lut_h}};
    } else if (effect_name == "taa_resolve") {
        unsigned int hist_h = (params.size() >= 1) ? static_cast<unsigned int>(params[0]) : 0;
        unsigned int mv_h = (params.size() >= 6) ? static_cast<unsigned int>(params[5]) : 0;
        extra_bindings = {{5, hist_h}, {2, mv_h}};
    } else if (effect_name == "dof") {
        unsigned int color_h = (params.size() >= 8) ? static_cast<unsigned int>(params[7]) : 0;
        extra_bindings = {{2, color_h}};
    } else if (effect_name == "motion_blur") {
        unsigned int color_h = (params.size() >= 5) ? static_cast<unsigned int>(params[4]) : 0;
        extra_bindings = {{2, color_h}};
    } else if (effect_name == "ssr") {
        unsigned int color_h = (params.size() >= 9) ? static_cast<unsigned int>(params[8]) : 0;
        extra_bindings = {{2, color_h}};
    } else if (effect_name == "deferred_lighting" && params.size() >= 2) {
        unsigned int normal_h = static_cast<unsigned int>(params[0]);
        unsigned int pos_h = static_cast<unsigned int>(params[1]);
        extra_bindings = {{2, normal_h}, {3, pos_h}};
    } else if (effect_name == "volumetric_fog" && params.size() >= 1) {
        unsigned int depth_h = static_cast<unsigned int>(params[0]);
        extra_bindings = {{2, depth_h}};
    } else if (effect_name == "decal" && params.size() >= 2) {
        unsigned int depth_h = static_cast<unsigned int>(params[0]);
        unsigned int decal_h = static_cast<unsigned int>(params[1]);
        extra_bindings = {{2, depth_h}, {3, decal_h}};
    } else if (effect_name == "wboit_composite" && params.size() >= 1) {
        unsigned int reveal_h = static_cast<unsigned int>(params[0]);
        extra_bindings = {{2, reveal_h}};
    }

    // 分配并绑定后处理 DescriptorSet
    if (pp_program) {
        AllocateAndUpdatePostProcessDescriptorSets(cmd_buf, pp_program,
                                                    source_texture, *resource_mgr_,
                                                    extra_bindings);
    }

    // 传递 push constants
    if (pp_program && pp_program->pipeline_layout != VK_NULL_HANDLE) {
        if (effect_name == "fxaa" && params.size() >= 2) {
            float pc[2] = {params[0], params[1]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "ssao" && params.size() >= 6) {
            float pc[6] = {params[0], params[1], params[2], params[3], params[4], params[5]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "lum_adapt" && params.size() >= 7) {
            // params: [prevAdaptedTex(ignored here), dt, speed_up, speed_down, min, max, compensation]
            float pc[6] = {params[1], params[2], params[3], params[4], params[5], params[6]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "tonemapping" && params.size() >= 2) {
            struct { float manual_exposure; int ae_enabled; int lut_enabled; float lut_intensity; } pc{};
            pc.manual_exposure = params[0];
            pc.ae_enabled = static_cast<unsigned int>(params[1]) != 0 ? 1 : 0;
            pc.lut_enabled = (params.size() >= 4 && static_cast<unsigned int>(params[2]) != 0) ? 1 : 0;
            pc.lut_intensity = (params.size() >= 4) ? params[3] : 0.0f;
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "bloom_composite" && params.size() >= 2) {
            const CompositeParamsView composite(params);
            struct {
                float exposure;
                float bloomIntensity;
                int bloomEnabled;
                int ssaoEnabled;
                int aeEnabled;
                int lutEnabled;
                float lutIntensity;
                int csEnabled;
                float csStrength;
                int vignetteEnabled;
                float vignetteIntensity;
                float vignetteRadius;
                float vignetteSoftness;
                int filmGrainEnabled;
                float filmGrainIntensity;
                float filmGrainTime;
            } pc{};
            pc.exposure           = composite.Float(CompositeParamsView::kExposure, 1.0f);
            pc.bloomIntensity     = composite.Float(CompositeParamsView::kBloomIntensity, 0.5f);
            pc.bloomEnabled       = (composite.Flag(CompositeParamsView::kBloomEnabled) &&
                                     composite.Texture(CompositeParamsView::kBloomTex) != 0) ? 1 : 0;
            pc.ssaoEnabled        = composite.Texture(CompositeParamsView::kSsaoTex) != 0 ? 1 : 0;
            pc.aeEnabled          = composite.Texture(CompositeParamsView::kAutoExposureTex) != 0 ? 1 : 0;
            pc.lutEnabled         = composite.Texture(CompositeParamsView::kLutTex) != 0 ? 1 : 0;
            pc.lutIntensity       = composite.Float(CompositeParamsView::kLutIntensity, 0.0f);
            pc.csEnabled          = composite.Texture(CompositeParamsView::kContactShadowTex) != 0 ? 1 : 0;
            pc.csStrength         = composite.Float(CompositeParamsView::kContactShadowStrength, 0.0f);
            pc.vignetteEnabled    = composite.Flag(CompositeParamsView::kVignetteEnabled) ? 1 : 0;
            pc.vignetteIntensity  = composite.Float(CompositeParamsView::kVignetteIntensity, 0.0f);
            pc.vignetteRadius     = composite.Float(CompositeParamsView::kVignetteRadius, 0.75f);
            pc.vignetteSoftness   = composite.Float(CompositeParamsView::kVignetteSoftness, 0.35f);
            pc.filmGrainEnabled   = composite.Flag(CompositeParamsView::kFilmGrainEnabled) ? 1 : 0;
            pc.filmGrainIntensity = composite.Float(CompositeParamsView::kFilmGrainIntensity, 0.0f);
            pc.filmGrainTime      = composite.Float(CompositeParamsView::kFilmGrainTime, 0.0f);
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "contact_shadow" && params.size() >= 10) {
            struct { float light_dir_x, light_dir_y, light_dir_z, near_p, far_p, screen_w, screen_h, strength, step_size; int num_steps; } pc{};
            pc.light_dir_x = params[0]; pc.light_dir_y = params[1]; pc.light_dir_z = params[2];
            pc.near_p = params[3]; pc.far_p = params[4];
            pc.screen_w = params[5]; pc.screen_h = params[6];
            pc.strength = params[7]; pc.num_steps = static_cast<int>(params[8]); pc.step_size = params[9];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "color_grading" && params.size() >= 2) {
            float pc = params[1];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "taa_resolve" && params.size() >= 8) {
            struct { float blend_factor, jitter_x, jitter_y; int frame_index; float screen_w, screen_h; } pc{};
            pc.blend_factor = params[1]; pc.jitter_x = params[2]; pc.jitter_y = params[3];
            pc.frame_index = static_cast<int>(params[4]);
            pc.screen_w = params[6]; pc.screen_h = params[7];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "motion_vector" && params.size() >= 18) {
            struct { float screen_w, screen_h; float _pad[2]; float reproj[16]; } pc{};
            pc.screen_w = params[0]; pc.screen_h = params[1];
            for (int i = 0; i < 16; ++i) pc.reproj[i] = params[2 + i];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "dof" && params.size() >= 7) {
            struct { float focus_distance, focus_range, bokeh_radius, near_p, far_p, screen_w, screen_h; } pc{
                params[0], params[1], params[2], params[3], params[4], params[5], params[6]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "motion_blur" && params.size() >= 4) {
            struct { float intensity, num_samples, screen_w, screen_h; } pc{
                params[0], params[1], params[2], params[3]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "ssr" && params.size() >= 8) {
            struct { float max_distance, thickness, step_size; int max_steps; float near_p, far_p, screen_w, screen_h; } pc{};
            pc.max_distance = params[0]; pc.thickness = params[1]; pc.step_size = params[2];
            pc.max_steps = static_cast<int>(params[3]);
            pc.near_p = params[4]; pc.far_p = params[5];
            pc.screen_w = params[6]; pc.screen_h = params[7];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "deferred_lighting" && params.size() >= 10) {
            // params: [normal_tex, position_tex, light_dir.xyz, light_color.xyz, intensity, ambient]
            struct { float lx, ly, lz; float intensity; float cx, cy, cz; float ambient; } pc{};
            pc.lx = params[2]; pc.ly = params[3]; pc.lz = params[4];
            pc.intensity = params[8];
            pc.cx = params[5]; pc.cy = params[6]; pc.cz = params[7];
            pc.ambient = params[9];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "edge_detect" && params.size() >= 10) {
            struct { float thickness, depth_threshold, normal_threshold, outline_r, outline_g, outline_b, near_p, far_p, screen_w, screen_h; } pc{};
            pc.thickness = params[0]; pc.depth_threshold = params[1]; pc.normal_threshold = params[2];
            pc.outline_r = params[3]; pc.outline_g = params[4]; pc.outline_b = params[5];
            pc.near_p = params[6]; pc.far_p = params[7];
            pc.screen_w = params[8]; pc.screen_h = params[9];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "volumetric_fog" && params.size() >= 30) {
            float pc[30];
            for (int i = 0; i < 30; ++i) pc[i] = params[i];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "decal" && params.size() >= 26) {
            float pc[26];
            for (int i = 0; i < 26; ++i) pc[i] = params[i];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "wboit_composite" && params.size() >= 1) {
            float pc[1] = { params[0] };
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        }
    }

    // 绑定后处理 VBO（全屏四边形，6 顶点）
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &pp_vbo_, offsets);

    vkCmdDraw(cmd_buf, 6, 1, 0, 0);

    global_state_.current_frame_stats.draw_calls++;
}

// ============================================================================
// DrawParticles3D — 3D 粒子绘制
// ============================================================================

void VulkanDrawExecutor::DrawParticles3D(
    VkCommandBuffer cmd_buf,
    const std::vector<Particle3DDrawItem>& items,
    const glm::mat4& view,
    const glm::mat4& projection,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr) {

    if (items.empty()) return;

    const VulkanShaderProgram* particle_program = shader_mgr.GetProgram(shader_mgr.particle_shader_handle());
    if (!particle_program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: Particle shader not available");
        return;
    }

    // 使用当前激活的 render pass
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // 延迟创建粒子 Pipeline
    VkPipeline particle_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        particle_program,
        active_rp,
        // 粒子顶点格式：vec3 pos, vec2 uv
        { VkVertexInputBindingDescription{0, 20, VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},  // aPos
            {1, 0, VK_FORMAT_R32G32_SFLOAT, 12},     // aTexCoord
        },
        context_->swapchain_extent(),
        current_msaa_samples_,
        current_color_attachment_count_);

    if (particle_pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to create particle pipeline");
        return;
    }

    // 绑定粒子管线
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, particle_pipeline);

    // 更新 PerFrame UBO
    UpdatePerFrameUBO(view, projection, {});

    // 绑定粒子 VBO
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &particle_vbo_, offsets);

    // 每个粒子绘制一次 billboard（4 顶点三角带）
    for (const auto& item : items) {
        // 分配并绑定 DescriptorSet
        AllocateAndUpdateParticleDescriptorSets(cmd_buf, particle_program,
                                                 item.texture_handle, *resource_mgr_);

        vkCmdDraw(cmd_buf, 4, 1, 0, 0);
        global_state_.current_frame_stats.draw_calls++;
    }

    global_state_.current_frame_stats.particle_count += static_cast<int>(items.size());
}

// ============================================================================
// 渲染统计
// ============================================================================

void VulkanDrawExecutor::BeginFrame() {
    global_state_.current_frame_stats = {};
    current_frame_index_ = context_->current_frame() % MAX_FRAMES;
    render_pass_counter_ = 0;
    skip_current_pass_ = false;
    mesh_vbo_offset_ = 0;
    mesh_ibo_offset_ = 0;
    bone_matrices_offset_ = 0;
    per_frame_ubo_offset_ = 0;
    per_scene_ubo_offset_ = 0;
    per_material_ubo_offset_ = 0;
    per_point_lights_ubo_offset_ = 0;
    per_spot_lights_ubo_offset_ = 0;
    const char* env = std::getenv("DSE_VULKAN_MAX_PASSES");
    max_render_passes_ = env ? std::atoi(env) : -1;
}

void VulkanDrawExecutor::EndFrame() {
    global_state_.last_frame_stats = global_state_.current_frame_stats;
}

// ============================================================================
// DispatchBloomCompute
// ============================================================================

void VulkanDrawExecutor::DispatchBloomCompute(
    VkCommandBuffer cmd_buf,
    unsigned int cs_handle,
    unsigned int src_texture_handle,
    unsigned int dst_rt_handle,
    VulkanShaderManager& shader_mgr) {

    const VulkanComputeProgram* cs = shader_mgr.GetComputeProgram(cs_handle);
    if (!cs || cs->pipeline == VK_NULL_HANDLE) return;

    const VulkanTexture*      src_tex = resource_mgr_->GetTexture(src_texture_handle);
    const VulkanRenderTarget* dst_rt  = resource_mgr_->GetRenderTarget(dst_rt_handle);
    if (!src_tex || !dst_rt || !dst_rt->allow_uav) return;

    VkDevice device = context_->device();

    // 1. 将 dst image 过渡到 GENERAL（以支持 Storage 写入）
    VkImageMemoryBarrier to_general{};
    to_general.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_general.oldLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_general.newLayout           = VK_IMAGE_LAYOUT_GENERAL;
    to_general.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_general.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_general.image               = dst_rt->color_texture.image;
    to_general.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    to_general.srcAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    to_general.dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_general);

    // 2. 绑定 Compute Pipeline
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, cs->pipeline);

    // 3. 分配并更新 DescriptorSet
    VkDescriptorSet desc_set = resource_mgr_->AllocateDescriptorSet(cs->descriptor_set_layout);
    if (desc_set != VK_NULL_HANDLE) {
        VkDescriptorImageInfo src_info{};
        src_info.sampler     = resource_mgr_->default_sampler();
        src_info.imageView   = src_tex->image_view;
        src_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo dst_info{};
        dst_info.sampler     = VK_NULL_HANDLE;
        dst_info.imageView   = dst_rt->color_texture.image_view;
        dst_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = desc_set;
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo      = &src_info;
        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = desc_set;
        writes[1].dstBinding      = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo      = &dst_info;
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                cs->pipeline_layout, 0, 1, &desc_set, 0, nullptr);
    }

    // 4. Push constants （texel 大小）
    struct BloomParams { float src_w, src_h, dst_w, dst_h; };
    const BloomParams bp {
        src_tex->width  > 0 ? 1.0f / static_cast<float>(src_tex->width)  : 1.0f,
        src_tex->height > 0 ? 1.0f / static_cast<float>(src_tex->height) : 1.0f,
        dst_rt->width   > 0 ? 1.0f / static_cast<float>(dst_rt->width)   : 1.0f,
        dst_rt->height  > 0 ? 1.0f / static_cast<float>(dst_rt->height)  : 1.0f,
    };
    vkCmdPushConstants(cmd_buf, cs->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bp), &bp);

    // 5. Dispatch
    const uint32_t gx = (static_cast<uint32_t>(dst_rt->width)  + 7) / 8;
    const uint32_t gy = (static_cast<uint32_t>(dst_rt->height) + 7) / 8;
    vkCmdDispatch(cmd_buf, gx, gy, 1);

    // 6. 过渡回 SHADER_READ_ONLY
    VkImageMemoryBarrier to_readonly = to_general;
    to_readonly.oldLayout    = VK_IMAGE_LAYOUT_GENERAL;
    to_readonly.newLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_readonly.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    to_readonly.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_readonly);

    global_state_.current_frame_stats.draw_calls++;
}

} // namespace render
} // namespace dse

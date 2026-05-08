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
    // 顶点格式：vec3 pos, vec4 color, vec2 uv, vec3 normal, vec3 tangent = 15 floats * 4 = 60 bytes
    const VkDeviceSize mesh_vbo_size = MAX_MESH_VERTICES * 64;
    const VkDeviceSize mesh_ibo_size = MAX_MESH_INDICES * sizeof(uint16_t);

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

    // --- UBO 缓冲区（双缓冲） ---
    for (int i = 0; i < MAX_FRAMES; ++i) {
        CreateUBOBufferInternal(device, physical_device, sizeof(VulkanPerFrameUBO),
                                per_frame_ubo_[i], per_frame_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, sizeof(VulkanPerSceneUBO),
                                per_scene_ubo_[i], per_scene_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, sizeof(VulkanPerMaterialUBO),
                                per_material_ubo_[i], per_material_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, sizeof(VulkanPointLightsUBO),
                                per_point_lights_ubo_[i], per_point_lights_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, sizeof(VulkanSpotLightsUBO),
                                per_spot_lights_ubo_[i], per_spot_lights_ubo_mem_[i]);
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

    // 从 pending uniforms 中提取 camera_pos
    auto it = pending_mat4.find("u_camera_pos");
    if (it != pending_mat4.end()) {
        // camera_pos 存储为 mat4，取第一列
        ubo.camera_pos = it->second[0];
    }

    WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                  0, sizeof(VulkanPerFrameUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerSceneUBO(const MeshDrawItem& item) {
    VulkanPerSceneUBO ubo{};
    ubo.light_dir_and_enabled = glm::vec4(item.light_direction, item.lighting_enabled ? 1.0f : 0.0f);
    ubo.light_color_and_ambient = glm::vec4(item.light_color, item.ambient_intensity);
    ubo.light_params = glm::vec4(item.light_intensity, item.shadow_strength,
                                  item.receive_shadow ? 1.0f : 0.0f, static_cast<float>(item.shading_mode));
    ubo.cascade_splits = glm::vec4(global_cascade_splits_[0], global_cascade_splits_[1],
                                    global_cascade_splits_[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        ubo.light_space_matrices[i] = global_light_space_matrix_[i];
    }

    WriteToBuffer(context_->device(), per_scene_ubo_mem_[current_frame_index_],
                  0, sizeof(VulkanPerSceneUBO), &ubo);
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

    WriteToBuffer(context_->device(), per_material_ubo_mem_[current_frame_index_],
                  0, sizeof(VulkanPerMaterialUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePointSpotLightUBOs(const MeshDrawItem& item) {
    uint32_t fi = current_frame_index_;

    VulkanPointLightsUBO pl_ubo{};
    pl_ubo.u_point_light_count = static_cast<int>(
        (std::min)(item.point_lights.size(), (size_t)4));
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
                  0, sizeof(pl_ubo), &pl_ubo);

    VulkanSpotLightsUBO sl_ubo{};
    sl_ubo.u_spot_light_count = static_cast<int>(
        (std::min)(item.spot_lights.size(), (size_t)4));
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
                  0, sizeof(sl_ubo), &sl_ubo);
}

// ============================================================================
// DescriptorSet 分配与更新
// ============================================================================

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdateMeshDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    const MeshDrawItem& item,
    VulkanResourceManager& resource_mgr) {

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
        buf_info.offset = 0;
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
        buf_info.offset = 0;
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

    // --- Set 1 binding 1: PointLights UBO ---
    {
        VkDescriptorBufferInfo pl_buf{};
        pl_buf.buffer = per_point_lights_ubo_[fi];
        pl_buf.offset = 0;
        pl_buf.range  = sizeof(VulkanPointLightsUBO);

        VkWriteDescriptorSet pl_write{};
        pl_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pl_write.dstSet          = sets[1];
        pl_write.dstBinding      = 1;
        pl_write.descriptorCount = 1;
        pl_write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        pl_write.pBufferInfo     = &pl_buf;
        vkUpdateDescriptorSets(device, 1, &pl_write, 0, nullptr);
    }

    // --- Set 1 binding 2: SpotLights UBO ---
    {
        VkDescriptorBufferInfo sl_buf{};
        sl_buf.buffer = per_spot_lights_ubo_[fi];
        sl_buf.offset = 0;
        sl_buf.range  = sizeof(VulkanSpotLightsUBO);

        VkWriteDescriptorSet sl_write{};
        sl_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        sl_write.dstSet          = sets[1];
        sl_write.dstBinding      = 2;
        sl_write.descriptorCount = 1;
        sl_write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        sl_write.pBufferInfo     = &sl_buf;
        vkUpdateDescriptorSets(device, 1, &sl_write, 0, nullptr);
    }

    // --- Set 2: PerMaterial UBO + 采样器 ---
    {
        // PerMaterial UBO (binding 0)
        VkDescriptorBufferInfo mat_buf_info{};
        mat_buf_info.buffer = per_material_ubo_[fi];
        mat_buf_info.offset = 0;
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

            image_infos[i].sampler = VK_NULL_HANDLE; // 使用着色器中的 immutable sampler 或创建默认 sampler
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
        bool has_shadow = false;
        {
            VkSampler cmp_sampler = resource_mgr.shadow_comparison_sampler();
            for (int i = 0; i < 3; ++i) {
                unsigned int sm_handle = global_shadow_map_[i];
                const VulkanTexture* sm_tex = (sm_handle != 0)
                    ? resource_mgr.GetTexture(sm_handle) : nullptr;
                VkImageView view = sm_tex ? sm_tex->image_view : VK_NULL_HANDLE;
                if (view == VK_NULL_HANDLE) {
                    const VulkanTexture* white = resource_mgr.GetTexture(white_texture_handle_);
                    view = white ? white->image_view : VK_NULL_HANDLE;
                }
                shadow_image_infos[i].sampler     = cmp_sampler;
                shadow_image_infos[i].imageView   = view;
                shadow_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            }
            if (shadow_image_infos[0].imageView != VK_NULL_HANDLE) {
                shadow_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                shadow_write.dstSet          = sets[2];
                shadow_write.dstBinding      = 6;
                shadow_write.dstArrayElement = 0;
                shadow_write.descriptorCount = 3;
                shadow_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                shadow_write.pImageInfo      = shadow_image_infos;
                has_shadow = true;
            }
        }

        // 合并写入
        std::vector<VkWriteDescriptorSet> all_writes;
        all_writes.push_back(mat_write);
        for (int i = 0; i < 5; ++i) {
            if (tex_writes[i].sType != 0) {
                all_writes.push_back(tex_writes[i]);
            }
        }
        if (has_shadow) all_writes.push_back(shadow_write);
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(all_writes.size()), all_writes.data(), 0, nullptr);
    }

    // --- Set 3 binding 0: 点光源立方体阴影贴图 (u_point_shadow_maps[4]) ---
    if (set_count >= 4 && sets[3] != VK_NULL_HANDLE) {
        VkDescriptorImageInfo point_shadow_infos[4] = {};
        VkWriteDescriptorSet  point_shadow_write{};
        bool has_point_shadow = false;
        {
            VkSampler lin_sampler = resource_mgr.default_sampler();
            for (int i = 0; i < 4; ++i) {
                unsigned int ps_handle = global_point_shadow_map_[i];
                const VulkanTexture* ps_tex = (ps_handle != 0)
                    ? resource_mgr.GetTexture(ps_handle) : nullptr;
                VkImageView view = ps_tex ? ps_tex->image_view : VK_NULL_HANDLE;
                if (view == VK_NULL_HANDLE) {
                    const VulkanTexture* white = resource_mgr.GetTexture(white_texture_handle_);
                    view = white ? white->image_view : VK_NULL_HANDLE;
                }
                point_shadow_infos[i].sampler     = lin_sampler;
                point_shadow_infos[i].imageView   = view;
                point_shadow_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            if (point_shadow_infos[0].imageView != VK_NULL_HANDLE) {
                point_shadow_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                point_shadow_write.dstSet          = sets[3];
                point_shadow_write.dstBinding      = 0;
                point_shadow_write.dstArrayElement = 0;
                point_shadow_write.descriptorCount = 4;
                point_shadow_write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                point_shadow_write.pImageInfo      = point_shadow_infos;
                has_point_shadow = true;
            }
        }
        if (has_point_shadow) vkUpdateDescriptorSets(device, 1, &point_shadow_write, 0, nullptr);
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

    VkDescriptorSet sets[2] = {};
    for (int s = 0; s < static_cast<int>(program->descriptor_set_layouts.size()) && s < 2; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    }

    // Set 0: PerFrame UBO
    {
        VkDescriptorBufferInfo buf_info{};
        buf_info.buffer = per_frame_ubo_[fi];
        buf_info.offset = 0;
        buf_info.range = sizeof(VulkanPerFrameUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets[0];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buf_info;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Set 2: skybox cubemap sampler (binding 1)
    {
        const VulkanTexture* tex = resource_mgr.GetTexture(cubemap_texture_handle);
        if (!tex) {
            DEBUG_LOG_WARN("Skybox cubemap texture not found: handle={}", cubemap_texture_handle);
            return VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo image_info{};
        image_info.imageView = tex->image_view;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = sets[1];
        write.dstBinding = 1;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image_info;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0,
                            static_cast<uint32_t>(program->descriptor_set_layouts.size()),
                            sets, 0, nullptr);

    return sets[0];
}

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdateParticleDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    unsigned int texture_handle,
    VulkanResourceManager& resource_mgr) {

    auto device = context_->device();
    uint32_t fi = current_frame_index_;

    if (program->descriptor_set_layouts.empty()) return VK_NULL_HANDLE;

    VkDescriptorSet set = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[0]);
    if (set == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    // Set 0: PerFrame UBO
    {
        VkDescriptorBufferInfo buf_info{};
        buf_info.buffer = per_frame_ubo_[fi];
        buf_info.offset = 0;
        buf_info.range = sizeof(VulkanPerFrameUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &buf_info;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0, 1, &set, 0, nullptr);

    return set;
}

VkDescriptorSet VulkanDrawExecutor::AllocateAndUpdatePostProcessDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    unsigned int source_texture,
    VulkanResourceManager& resource_mgr) {

    auto device = context_->device();

    if (program->descriptor_set_layouts.empty()) return VK_NULL_HANDLE;

    VkDescriptorSet set = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[0]);
    if (set == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    // 后处理采样器
    if (source_texture != 0) {
        const VulkanTexture* tex = resource_mgr.GetTexture(source_texture);
        if (tex) {
            VkDescriptorImageInfo image_info{};
            image_info.imageView = tex->image_view;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = 1;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo = &image_info;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
    }

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0, 1, &set, 0, nullptr);

    return set;
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

    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = vk_render_pass;
    begin_info.framebuffer = framebuffer;
    begin_info.renderArea.offset = {0, 0};
    begin_info.renderArea.extent = context_->swapchain_extent();

    std::vector<VkClearValue> clear_values(2);
    clear_values[0].color = {{render_pass.clear_color.x,
                               render_pass.clear_color.y,
                               render_pass.clear_color.z,
                               render_pass.clear_color.w}};
    clear_values[1].depthStencil = {1.0f, 0};

    begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(cmd_buf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanDrawExecutor::EndRenderPass(VkCommandBuffer cmd_buf) {
    vkCmdEndRenderPass(cmd_buf);
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

    if (items.empty()) return;

    const VulkanShaderProgram* sprite_program = shader_mgr.GetProgram(shader_mgr.sprite_shader_handle());
    if (!sprite_program) {
        // Fallback：无 sprite shader 时仅统计
        current_frame_stats_.draw_calls++;
        current_frame_stats_.sprite_count += static_cast<int>(items.size());
        return;
    }

    // 2D 精灵管线状态：alpha 混合、无深度写、无剔除
    PipelineStateDesc sprite_desc;
    sprite_desc.blend_enabled = true;
    sprite_desc.blend_src = BlendFactor::SrcAlpha;
    sprite_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    sprite_desc.depth_test_enabled = false;
    sprite_desc.depth_write_enabled = false;
    sprite_desc.culling_enabled = false;
    unsigned int sprite_state = pipeline_mgr.CreatePipelineState(sprite_desc);

    // 延迟创建 Pipeline
    VkPipeline sprite_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        sprite_state, sprite_program, context_->swapchain_render_pass(),
        // 2D 精灵顶点格式：vec2 pos, vec2 texcoord, vec4 color = 8 floats = 32 bytes
        { VkVertexInputBindingDescription{0, 32, VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},          // aPos
            {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},          // aTexCoord
            {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},   // aColor
        },
        context_->swapchain_extent());

    if (sprite_pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to create sprite pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, sprite_pipeline);

    // 更新 PerFrame UBO
    UpdatePerFrameUBO(view, projection, {});

    // 逐精灵组绘制（按纹理分组批处理可优化，当前逐个绘制）
    // 精灵顶点格式：vec2 pos, vec2 uv, vec4 color
    struct SpriteVertex {
        float pos[2];
        float uv[2];
        float color[4];
    };

    for (const auto& item : items) {
        // 组装 4 个顶点（四边形）
        float x = item.model[3][0];
        float y = item.model[3][1];
        // 从 model 矩阵提取缩放
        float sx = glm::length(glm::vec3(item.model[0]));
        float sy = glm::length(glm::vec3(item.model[1]));
        float w = sx;
        float h = sy;

        SpriteVertex vertices[4] = {
            {{x,     y + h}, {item.uv.x, item.uv.w}, {item.color.r, item.color.g, item.color.b, item.color.a}},
            {{x,     y},     {item.uv.x, item.uv.y}, {item.color.r, item.color.g, item.color.b, item.color.a}},
            {{x + w, y},     {item.uv.z, item.uv.y}, {item.color.r, item.color.g, item.color.b, item.color.a}},
            {{x + w, y + h}, {item.uv.z, item.uv.w}, {item.color.r, item.color.g, item.color.b, item.color.a}},
        };

        uint16_t indices[6] = {0, 1, 2, 0, 2, 3};

        // 上传顶点
        WriteToBuffer(context_->device(), sprite_vbo_mem_, 0, sizeof(vertices), vertices);
        WriteToBuffer(context_->device(), sprite_ibo_mem_, 0, sizeof(indices), indices);

        // Push constant: model matrix
        vkCmdPushConstants(cmd_buf, sprite_program->pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &item.model);

        // 分配并绑定 DescriptorSet
        AllocateAndUpdateParticleDescriptorSets(cmd_buf, sprite_program,
                                                 item.texture_handle, resource_mgr);

        // 绑定 VBO + IBO
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &sprite_vbo_, offsets);
        vkCmdBindIndexBuffer(cmd_buf, sprite_ibo_, 0, VK_INDEX_TYPE_UINT16);

        vkCmdDrawIndexed(cmd_buf, 6, 1, 0, 0, 0);

        current_frame_stats_.draw_calls++;
    }

    current_frame_stats_.sprite_count += static_cast<int>(items.size());
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

    const VulkanShaderProgram* pbr_program = shader_mgr.GetProgram(shader_mgr.pbr_shader_handle());
    if (!pbr_program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: PBR shader not available");
        return;
    }

    // 延迟创建 VkPipeline（按需，首次 Draw 时创建）
    VkPipeline vk_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        pbr_program,
        context_->swapchain_render_pass(),
        // 3D Mesh 顶点格式：vec3 pos, vec4 color, vec2 uv, vec3 normal, vec3 tangent
        { VkVertexInputBindingDescription{0, 60, VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},       // aPos
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12},   // aColor
            {2, 0, VK_FORMAT_R32G32_SFLOAT, 28},          // aTexCoord
            {3, 0, VK_FORMAT_R32G32B32_SFLOAT, 36},       // aNormal
            {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 48},       // aTangent
        },
        context_->swapchain_extent());

    if (vk_pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to get/create VkPipeline for mesh draw");
        return;
    }

    // 绑定 PBR 管线
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

    // 逐 mesh 绘制
    for (const auto& item : items) {
        // 更新 UBO 数据
        UpdatePerFrameUBO(view, projection, {});
        UpdatePerSceneUBO(item);
        UpdatePerMaterialUBO(item);
        UpdatePointSpotLightUBOs(item);

        // 分配并绑定 DescriptorSet
        AllocateAndUpdateMeshDescriptorSets(cmd_buf, pbr_program, item, resource_mgr);

        // 上传顶点数据到 mesh VBO
        if (!item.vertices.empty()) {
            size_t vdata_size = item.vertices.size() * sizeof(item.vertices[0]);
            WriteToBuffer(context_->device(), mesh_vbo_mem_, 0, vdata_size, item.vertices.data());
        }

        // 上传索引数据到 mesh IBO
        if (!item.indices.empty()) {
            size_t idata_size = item.indices.size() * sizeof(item.indices[0]);
            WriteToBuffer(context_->device(), mesh_ibo_mem_, 0, idata_size, item.indices.data());
        }

        // Push constant: model matrix
        glm::mat4 model = item.model;
        vkCmdPushConstants(cmd_buf, pbr_program->pipeline_layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(glm::mat4), &model);

        // 绑定 VBO
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, &mesh_vbo_, offsets);

        // 绑定 IBO
        vkCmdBindIndexBuffer(cmd_buf, mesh_ibo_, 0, VK_INDEX_TYPE_UINT16);

        // 绘制
        vkCmdDrawIndexed(cmd_buf,
                         static_cast<uint32_t>(item.indices.size()),
                         1, 0, 0, 0);

        current_frame_stats_.draw_calls++;
    }

    current_frame_stats_.mesh_count += static_cast<int>(items.size());
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

    const VulkanShaderProgram* skybox_program = shader_mgr.GetProgram(shader_mgr.skybox_shader_handle());
    if (!skybox_program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: Skybox shader not available");
        return;
    }

    // 延迟创建天空盒 Pipeline
    VkPipeline skybox_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        skybox_program,
        context_->swapchain_render_pass(),
        // 天空盒顶点格式：vec3 pos
        { VkVertexInputBindingDescription{0, 12, VK_VERTEX_INPUT_RATE_VERTEX} },
        { {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0} },
        context_->swapchain_extent());

    if (skybox_pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: failed to create skybox pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline);

    // 更新 PerFrame UBO 并分配 DescriptorSet
    UpdatePerFrameUBO(view, projection, {});
    AllocateAndUpdateSkyboxDescriptorSets(cmd_buf, skybox_program, cubemap_texture_handle,
                                           *resource_mgr_);

    // 绑定天空盒 VBO
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &skybox_vbo_, offsets);

    // 36 个顶点（6 面 * 2 三角形 * 3 顶点）
    vkCmdDraw(cmd_buf, 36, 1, 0, 0);

    current_frame_stats_.draw_calls++;
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

    // 后处理管线状态：无深度、无剪裁；ui_overlay 需要 alpha 混合
    PipelineStateDesc pp_desc;
    pp_desc.blend_enabled = (effect_name == "ui_overlay");
    pp_desc.blend_src = BlendFactor::SrcAlpha;
    pp_desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
    pp_desc.depth_test_enabled = false;
    pp_desc.depth_write_enabled = false;
    pp_desc.culling_enabled = false;
    unsigned int pp_state = pipeline_mgr.CreatePipelineState(pp_desc);

    // 优先使用专用后处理着色器，fallback 到 PBR 着色器
    const VulkanShaderProgram* pp_program = shader_mgr.GetProgram(shader_mgr.postprocess_shader_handle());
    if (!pp_program) {
        pp_program = shader_mgr.GetProgram(shader_mgr.pbr_shader_handle());
    }

    VkPipeline pp_pipeline = VK_NULL_HANDLE;
    if (pp_program) {
        pp_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
            pp_state, pp_program, context_->swapchain_render_pass(),
            // 后处理顶点格式：vec2 pos, vec2 texcoord
            { VkVertexInputBindingDescription{0, 16, VK_VERTEX_INPUT_RATE_VERTEX} },
            {
                {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},   // aPos
                {1, 0, VK_FORMAT_R32G32_SFLOAT, 8},   // aTexCoord
            },
            context_->swapchain_extent());
    }

    if (pp_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pp_pipeline);
    }

    // 分配并绑定后处理 DescriptorSet
    if (pp_program) {
        AllocateAndUpdatePostProcessDescriptorSets(cmd_buf, pp_program,
                                                    source_texture, *resource_mgr_);
    }

    // 绑定后处理 VBO（全屏四边形，6 顶点）
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &pp_vbo_, offsets);

    vkCmdDraw(cmd_buf, 6, 1, 0, 0);

    current_frame_stats_.draw_calls++;
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

    // 延迟创建粒子 Pipeline
    VkPipeline particle_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        particle_program,
        context_->swapchain_render_pass(),
        // 粒子顶点格式：vec3 pos, vec2 uv
        { VkVertexInputBindingDescription{0, 20, VK_VERTEX_INPUT_RATE_VERTEX} },
        {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},  // aPos
            {1, 0, VK_FORMAT_R32G32_SFLOAT, 12},     // aTexCoord
        },
        context_->swapchain_extent());

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
        current_frame_stats_.draw_calls++;
    }

    current_frame_stats_.particle_count += static_cast<int>(items.size());
}

// ============================================================================
// 渲染统计
// ============================================================================

void VulkanDrawExecutor::BeginFrame() {
    current_frame_stats_ = {};
    current_frame_index_ = context_->current_frame() % MAX_FRAMES;
}

void VulkanDrawExecutor::EndFrame() {
    last_frame_stats_ = current_frame_stats_;
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

    current_frame_stats_.draw_calls++;
}

} // namespace render
} // namespace dse

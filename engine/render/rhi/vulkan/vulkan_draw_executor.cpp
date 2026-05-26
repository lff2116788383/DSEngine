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
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/base/debug.h"
#include "engine/render/shaders/generated/embed/hair_vert.gen.h"
#include "engine/render/shaders/generated/embed/hair_frag.gen.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_map>

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

/// 持久映射缓存：避免每次 WriteToBuffer 都 vkMapMemory/vkUnmapMemory
/// HOST_VISIBLE + HOST_COHERENT 内存可以 map 一次后永久使用
static std::unordered_map<VkDeviceMemory, void*> g_persistent_map_cache;

/// 将数据写入 host-visible 缓冲区（使用持久映射）
void WriteToBuffer(VkDevice device, VkDeviceMemory memory,
                   VkDeviceSize offset, VkDeviceSize size, const void* data) {
    auto it = g_persistent_map_cache.find(memory);
    if (it != g_persistent_map_cache.end()) {
        memcpy(static_cast<char*>(it->second) + offset, data, static_cast<size_t>(size));
        return;
    }
    // 首次访问：全量映射并缓存指针
    void* mapped = nullptr;
    VkResult r = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (r != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] WriteToBuffer: vkMapMemory FAILED offset={} size={} result={}", offset, size, (int)r);
        return;
    }
    g_persistent_map_cache[memory] = mapped;
    memcpy(static_cast<char*>(mapped) + offset, data, static_cast<size_t>(size));
}

/// 释放持久映射（buffer 销毁前调用）
void UnmapPersistentBuffer(VkDevice device, VkDeviceMemory memory) {
    auto it = g_persistent_map_cache.find(memory);
    if (it != g_persistent_map_cache.end()) {
        vkUnmapMemory(device, memory);
        g_persistent_map_cache.erase(it);
    }
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
    const VkDeviceSize mesh_ibo_size = 16 * 1024 * 1024;  // 16 MB
    mesh_vbo_capacity_ = mesh_vbo_size;
    mesh_ibo_capacity_ = mesh_ibo_size;

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

    // --- UBO 缓冲区（双缓冲，每个缓冲区扩大到多 slot，避免 GPU 延迟执行时覆盖） ---
    // per_frame: 128 batches/frame × 256B = 32KB（含 shadow passes + GPU Driven setup）
    // per_scene/material/lights: 512 items/frame × 256B = 128KB
    constexpr size_t kPerFrameSlots  = 128;
    constexpr size_t kPerItemSlots   = 512;
    constexpr size_t kSlotAlign      = kUboSlotAlignment;
    per_frame_ubo_capacity_ = kPerFrameSlots * kSlotAlign;
    per_scene_ubo_capacity_ = kPerItemSlots * kSlotAlign;
    per_material_ubo_capacity_ = kPerItemSlots * kSlotAlign;
    terrain_params_ubo_capacity_ = kPerItemSlots * kSlotAlign;
    for (int i = 0; i < MAX_FRAMES; ++i) {
        CreateUBOBufferInternal(device, physical_device, kPerFrameSlots * kSlotAlign,
                                per_frame_ubo_[i], per_frame_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kSlotAlign,
                                per_scene_ubo_[i], per_scene_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kSlotAlign,
                                per_material_ubo_[i], per_material_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kSlotAlign,
                                terrain_params_ubo_[i], terrain_params_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kLightUboSlotAlignment,
                                per_point_lights_ubo_[i], per_point_lights_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerItemSlots * kLightUboSlotAlignment,
                                per_spot_lights_ubo_[i], per_spot_lights_ubo_mem_[i]);
    }

    // --- BoneMatrices SSBO / MorphWeights UBO ---
    constexpr size_t kBoneMatricesSize = 64 * 255 * sizeof(glm::mat4); // 64 meshes * 16320 bytes = ~1020KB
    constexpr size_t kMorphWeightsSize = 16; // 4 floats
    // BoneMatrices 使用 STORAGE_BUFFER（SSBO）+ UNIFORM_BUFFER 双用途
    CreateVulkanBuffer(device, physical_device, kBoneMatricesSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       bone_matrices_ubo_, bone_matrices_ubo_mem_);
    CreateUBOBufferInternal(device, physical_device, kMorphWeightsSize,
                            morph_weights_ubo_, morph_weights_ubo_mem_);
    // 初始化 BoneMatrices 为单位矩阵
    {
        std::vector<glm::mat4> identity_bones(64 * 255, glm::mat4(1.0f));
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
    const unsigned char* white_faces[6] = {
        white_pixel, white_pixel, white_pixel, white_pixel, white_pixel, white_pixel
    };
    white_cubemap_handle_ = resource_mgr->CreateTextureCube(1, 1, white_faces, true);

    CreateUBOBufferInternal(device, physical_device, 256,
                            dummy_ubo_buffer_, dummy_ubo_buffer_mem_);
    {
        uint8_t zeros[256] = {};
        WriteToBuffer(device, dummy_ubo_buffer_mem_, 0, sizeof(zeros), zeros);
    }

    // --- Dummy SSBO 占位 buffer ---
    // VUID-VkWriteDescriptorSet-descriptorType-00331: SSBO descriptor 写入要求
    // 对应 buffer 必须有 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT。不能复用 UBO 占位。
    CreateVulkanBuffer(device, physical_device, 64,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       dummy_ssbo_buffer_, dummy_ssbo_buffer_mem_);
    {
        uint8_t zeros[64] = {};
        WriteToBuffer(device, dummy_ssbo_buffer_mem_, 0, sizeof(zeros), zeros);
    }

    // --- Dummy 3D 纹理（1x1x1，用于 sampler3D 占位，如后处理 LUT）---
    {
        VkImageCreateInfo img_ci{};
        img_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img_ci.imageType = VK_IMAGE_TYPE_3D;
        img_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
        img_ci.extent = {1, 1, 1};
        img_ci.mipLevels = 1;
        img_ci.arrayLayers = 1;
        img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
        img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        img_ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device, &img_ci, nullptr, &dummy_3d_image_) == VK_SUCCESS) {
            VkMemoryRequirements mem_req;
            vkGetImageMemoryRequirements(device, dummy_3d_image_, &mem_req);
            VkMemoryAllocateInfo alloc_info{};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = mem_req.size;
            VkPhysicalDeviceMemoryProperties mem_props;
            vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
            for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
                if ((mem_req.memoryTypeBits & (1u << i)) &&
                    (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
                    alloc_info.memoryTypeIndex = i;
                    break;
                }
            }
            if (vkAllocateMemory(device, &alloc_info, nullptr, &dummy_3d_image_mem_) == VK_SUCCESS) {
                vkBindImageMemory(device, dummy_3d_image_, dummy_3d_image_mem_, 0);
                // 转换到 SHADER_READ_ONLY_OPTIMAL
                VkCommandBuffer cmd = resource_mgr->BeginSingleTimeCommands();
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = dummy_3d_image_;
                barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                resource_mgr->EndSingleTimeCommands(cmd);
                // 创建 3D image view
                VkImageViewCreateInfo view_ci{};
                view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                view_ci.image = dummy_3d_image_;
                view_ci.viewType = VK_IMAGE_VIEW_TYPE_3D;
                view_ci.format = VK_FORMAT_R8G8B8A8_UNORM;
                view_ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                vkCreateImageView(device, &view_ci, nullptr, &dummy_3d_image_view_);
            }
        }
    }

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
            UnmapPersistentBuffer(device, mem);
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
        destroy_buffer(terrain_params_ubo_[i], terrain_params_ubo_mem_[i]);
        destroy_buffer(per_point_lights_ubo_[i], per_point_lights_ubo_mem_[i]);
        destroy_buffer(per_spot_lights_ubo_[i], per_spot_lights_ubo_mem_[i]);
    }
    destroy_buffer(bone_matrices_ubo_, bone_matrices_ubo_mem_);
    destroy_buffer(morph_weights_ubo_, morph_weights_ubo_mem_);
    destroy_buffer(skinned_inst_ssbo_, skinned_inst_ssbo_mem_);
    skinned_inst_ssbo_capacity_ = 0;
    for (int i = 0; i < MAX_FRAMES; ++i)
        destroy_buffer(light_probe_ubo_[i], light_probe_ubo_mem_[i]);
    destroy_buffer(dummy_ubo_buffer_, dummy_ubo_buffer_mem_);
    destroy_buffer(dummy_ssbo_buffer_, dummy_ssbo_buffer_mem_);

    if (dummy_3d_image_view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, dummy_3d_image_view_, nullptr);
        dummy_3d_image_view_ = VK_NULL_HANDLE;
    }
    if (dummy_3d_image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, dummy_3d_image_, nullptr);
        dummy_3d_image_ = VK_NULL_HANDLE;
    }
    if (dummy_3d_image_mem_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, dummy_3d_image_mem_, nullptr);
        dummy_3d_image_mem_ = VK_NULL_HANDLE;
    }

    if (white_texture_handle_ != 0) {
        resource_mgr_->DeleteTexture(white_texture_handle_);
        white_texture_handle_ = 0;
    }
    if (white_cubemap_handle_ != 0) {
        resource_mgr_->DeleteTexture(white_cubemap_handle_);
        white_cubemap_handle_ = 0;
    }

    if (hair_pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, hair_pipeline_, nullptr);
        hair_pipeline_ = VK_NULL_HANDLE;
        hair_pipeline_rp_ = VK_NULL_HANDLE;
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

    if (per_frame_ubo_offset_ + sizeof(VulkanPerFrameUBO) > per_frame_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] PER_FRAME_UBO OVERFLOW: offset={} capacity={}", per_frame_ubo_offset_, per_frame_ubo_capacity_);
        return;
    }
    WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                  per_frame_ubo_offset_, sizeof(VulkanPerFrameUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerSceneUBO(const MeshDrawItem& item) {
    VulkanPerSceneUBO ubo{};
    const float light_enabled = (item.lighting_enabled && !global_state_.force_unlit) ? 1.0f : 0.0f;
    ubo.light_dir_and_enabled = glm::vec4(item.light_direction, light_enabled);
    ubo.light_color_and_ambient = glm::vec4(item.light_color, item.ambient_intensity);
    ubo.light_params = glm::vec4(item.light_intensity, item.shadow_strength,
                                  item.receive_shadow ? 1.0f : 0.0f, static_cast<float>(item.shading_mode));
    ubo.cascade_splits = glm::vec4(global_state_.cascade_splits[0], global_state_.cascade_splits[1],
                                    global_state_.cascade_splits[2], static_cast<float>(item.wboit_mode));
    for (int i = 0; i < 3; ++i) {
        ubo.light_space_matrices[i] = global_state_.light_space_matrix[i];
    }

    if (per_scene_ubo_offset_ + sizeof(VulkanPerSceneUBO) > per_scene_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] PER_SCENE_UBO OVERFLOW: offset={} capacity={}", per_scene_ubo_offset_, per_scene_ubo_capacity_);
        return;
    }
    WriteToBuffer(context_->device(), per_scene_ubo_mem_[current_frame_index_],
                  per_scene_ubo_offset_, sizeof(VulkanPerSceneUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerMaterialUBO(const MeshDrawItem& item) {
    VulkanPerMaterialUBO ubo{};
    if (global_state_.overdraw_mode) {
        ubo.albedo = glm::vec4(0.1f, 0.04f, 0.02f, 0.0f);
    } else {
        ubo.albedo = glm::vec4(item.material_albedo, item.material_metallic);
    }
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

    if (per_material_ubo_offset_ + sizeof(VulkanPerMaterialUBO) > per_material_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] PER_MATERIAL_UBO OVERFLOW: offset={} capacity={}", per_material_ubo_offset_, per_material_ubo_capacity_);
        return;
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
    bool gbuffer_mode,
    VkBuffer inst_ssbo,
    VkDeviceSize inst_ssbo_size,
    VkDeviceSize inst_ssbo_offset) {

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

    // 反射检查：仅写入 shader 实际声明的 descriptor bindings
    auto has_binding = [&](uint32_t set, uint32_t binding, VkDescriptorType type) -> bool {
        for (const auto& b : program->reflection.bindings) {
            if (b.set == set && b.binding == binding && b.type == type) return true;
        }
        return false;
    };
    // 类型无关版本：只检查 set+binding 是否存在（用于 BoneMatrices/MorphWeights 等可能存在类型微差的绑定）
    auto has_binding_any = [&](uint32_t set, uint32_t binding) -> bool {
        for (const auto& b : program->reflection.bindings) {
            if (b.set == set && b.binding == binding) return true;
        }
        return false;
    };

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
    if (has_binding(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)) {
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
    if (has_binding(1, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
        VkDescriptorBufferInfo pl_buf{};
        auto pl_it = bound_ssbos_.find(1); // binding 1 = PointLightSSBO
        const VulkanBuffer* pl_ssbo = (pl_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(pl_it->second) : nullptr;
        if (pl_ssbo && pl_ssbo->buffer != VK_NULL_HANDLE) {
            pl_buf.buffer = pl_ssbo->buffer;
            pl_buf.offset = 0;
            pl_buf.range  = pl_ssbo->size;
        } else {
            // fallback: SSBO 占位 buffer，必须有 STORAGE_BUFFER usage
            // VUID-VkWriteDescriptorSet-descriptorType-00331
            pl_buf.buffer = dummy_ssbo_buffer_;
            pl_buf.offset = 0;
            pl_buf.range  = VK_WHOLE_SIZE;
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
    if (has_binding(1, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
        VkDescriptorBufferInfo sl_buf{};
        auto sl_it = bound_ssbos_.find(2); // binding 2 = SpotLightSSBO
        const VulkanBuffer* sl_ssbo = (sl_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(sl_it->second) : nullptr;
        if (sl_ssbo && sl_ssbo->buffer != VK_NULL_HANDLE) {
            sl_buf.buffer = sl_ssbo->buffer;
            sl_buf.offset = 0;
            sl_buf.range  = sl_ssbo->size;
        } else {
            // VUID-VkWriteDescriptorSet-descriptorType-00331
            sl_buf.buffer = dummy_ssbo_buffer_;
            sl_buf.offset = 0;
            sl_buf.range  = VK_WHOLE_SIZE;
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
    if (has_binding(1, 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
        VkDescriptorBufferInfo ci_buf{};
        auto ci_it = bound_ssbos_.find(3); // binding 3 = ClusterInfoSSBO
        const VulkanBuffer* ci_ssbo = (ci_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(ci_it->second) : nullptr;
        if (ci_ssbo && ci_ssbo->buffer != VK_NULL_HANDLE) {
            ci_buf.buffer = ci_ssbo->buffer;
            ci_buf.offset = 0;
            ci_buf.range  = ci_ssbo->size;
        } else {
            // VUID-VkWriteDescriptorSet-descriptorType-00331
            ci_buf.buffer = dummy_ssbo_buffer_;
            ci_buf.offset = 0;
            ci_buf.range  = VK_WHOLE_SIZE;
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
    if (has_binding(1, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
        VkDescriptorBufferInfo li_buf{};
        auto li_it = bound_ssbos_.find(4); // binding 4 = LightIndexSSBO
        const VulkanBuffer* li_ssbo = (li_it != bound_ssbos_.end())
            ? resource_mgr.GetSSBO(li_it->second) : nullptr;
        if (li_ssbo && li_ssbo->buffer != VK_NULL_HANDLE) {
            li_buf.buffer = li_ssbo->buffer;
            li_buf.offset = 0;
            li_buf.range  = li_ssbo->size;
        } else {
            // VUID-VkWriteDescriptorSet-descriptorType-00331
            li_buf.buffer = dummy_ssbo_buffer_;
            li_buf.offset = 0;
            li_buf.range  = VK_WHOLE_SIZE;
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
    if (has_binding(1, 5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)) {
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
        if (tex && has_binding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
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
        VkWriteDescriptorSet mat_write{};
        if (has_binding(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)) {
            mat_buf_info.buffer = per_material_ubo_[fi];
            mat_buf_info.offset = per_material_offset;
            mat_buf_info.range = sizeof(VulkanPerMaterialUBO);
            mat_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            mat_write.dstSet = sets[2];
            mat_write.dstBinding = 0;
            mat_write.dstArrayElement = 0;
            mat_write.descriptorCount = 1;
            mat_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            mat_write.pBufferInfo = &mat_buf_info;
        }

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
            if (!has_binding(2, tex_bindings[i].binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) continue;
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
        if (has_binding(2, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
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
        if (has_binding(2, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
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

        // BoneMatrices SSBO (binding 8) — 整个 buffer 绑定，push constant 控制 offset
        VkDescriptorBufferInfo bone_buf_info{};
        VkWriteDescriptorSet bone_write{};
        if (has_binding_any(2, 8)) {
            bone_buf_info.buffer = bone_matrices_ubo_;
            bone_buf_info.offset = bone_offset;
            bone_buf_info.range  = VK_WHOLE_SIZE;
            bone_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            bone_write.dstSet          = sets[2];
            bone_write.dstBinding      = 8;
            bone_write.descriptorCount = 1;
            bone_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bone_write.pBufferInfo     = &bone_buf_info;
        }

        // MorphWeights UBO (binding 9)
        VkDescriptorBufferInfo morph_buf_info{};
        VkWriteDescriptorSet morph_write{};
        if (has_binding_any(2, 9)) {
            morph_buf_info.buffer = morph_weights_ubo_;
            morph_buf_info.offset = 0;
            morph_buf_info.range  = 16;
            morph_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            morph_write.dstSet          = sets[2];
            morph_write.dstBinding      = 9;
            morph_write.descriptorCount = 1;
            morph_write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            morph_write.pBufferInfo     = &morph_buf_info;
        }

        auto has_set2_binding = [&](uint32_t binding, VkDescriptorType type) {
            return has_binding(2, binding, type);
        };

        SpotLightDataUBO spot_data = PrepareSpotLightDataUBO(global_state_);
        WriteToBuffer(device, per_spot_lights_ubo_mem_[fi], 0, sizeof(spot_data), &spot_data);
        VkDescriptorBufferInfo spot_data_buf{};
        spot_data_buf.buffer = per_spot_lights_ubo_[fi];
        spot_data_buf.offset = 0;
        spot_data_buf.range = sizeof(SpotLightDataUBO);
        VkWriteDescriptorSet spot_data_write{};
        if (has_set2_binding(19, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)) {
            spot_data_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            spot_data_write.dstSet = sets[2];
            spot_data_write.dstBinding = 19;
            spot_data_write.descriptorCount = 1;
            spot_data_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            spot_data_write.pBufferInfo = &spot_data_buf;
        }

        struct TerrainParamsUBO {
            glm::vec4 flags;
            glm::vec4 tiling;
        } terrain_params{};
        terrain_params.flags.x = item.splat_enabled ? 1.0f : 0.0f;
        terrain_params.tiling = item.splat_tiling;
        VkDescriptorBufferInfo terrain_buf{};
        if (terrain_params_ubo_offset_ + sizeof(TerrainParamsUBO) <= terrain_params_ubo_capacity_) {
            WriteToBuffer(device, terrain_params_ubo_mem_[fi], terrain_params_ubo_offset_,
                          sizeof(terrain_params), &terrain_params);
            terrain_buf.buffer = terrain_params_ubo_[fi];
            terrain_buf.offset = terrain_params_ubo_offset_;
            terrain_buf.range = sizeof(TerrainParamsUBO);
            terrain_params_ubo_offset_ += kUboSlotAlignment;
        } else {
            terrain_buf.buffer = dummy_ubo_buffer_;
            terrain_buf.offset = 0;
            terrain_buf.range = sizeof(TerrainParamsUBO);
        }
        VkWriteDescriptorSet terrain_write{};
        if (has_set2_binding(16, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)) {
            terrain_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            terrain_write.dstSet = sets[2];
            terrain_write.dstBinding = 16;
            terrain_write.descriptorCount = 1;
            terrain_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            terrain_write.pBufferInfo = &terrain_buf;
        }

        VkDescriptorImageInfo splat_infos[5] = {};
        VkWriteDescriptorSet splat_writes[5] = {};
        unsigned int splat_handles[5] = {
            item.splat_weight_map_handle,
            item.splat_layer_handles[0],
            item.splat_layer_handles[1],
            item.splat_layer_handles[2],
            item.splat_layer_handles[3],
        };
        for (int i = 0; i < 5; ++i) {
            const uint32_t binding = static_cast<uint32_t>(11 + i);
            if (!has_set2_binding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) continue;
            unsigned int tex_handle = splat_handles[i] != 0 ? splat_handles[i] : white_texture_handle_;
            const VulkanTexture* tex = resource_mgr.GetTexture(tex_handle);
            if (!tex) tex = resource_mgr.GetTexture(white_texture_handle_);
            if (!tex) continue;
            splat_infos[i].sampler = resource_mgr.default_sampler();
            splat_infos[i].imageView = tex->image_view;
            splat_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            splat_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            splat_writes[i].dstSet = sets[2];
            splat_writes[i].dstBinding = binding;
            splat_writes[i].descriptorCount = 1;
            splat_writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            splat_writes[i].pImageInfo = &splat_infos[i];
        }

        VkDescriptorImageInfo ibl_infos[2] = {};
        VkWriteDescriptorSet ibl_writes[2] = {};
        const VulkanTexture* white_cube = resource_mgr.GetTexture(white_cubemap_handle_);
        const VulkanTexture* white_2d = resource_mgr.GetTexture(white_texture_handle_);
        if (has_set2_binding(17, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) && white_cube) {
            ibl_infos[0].sampler = resource_mgr.default_sampler();
            ibl_infos[0].imageView = white_cube->image_view;
            ibl_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ibl_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ibl_writes[0].dstSet = sets[2];
            ibl_writes[0].dstBinding = 17;
            ibl_writes[0].descriptorCount = 1;
            ibl_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            ibl_writes[0].pImageInfo = &ibl_infos[0];
        }
        if (has_set2_binding(18, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) && white_2d) {
            ibl_infos[1].sampler = resource_mgr.default_sampler();
            ibl_infos[1].imageView = white_2d->image_view;
            ibl_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ibl_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            ibl_writes[1].dstSet = sets[2];
            ibl_writes[1].dstBinding = 18;
            ibl_writes[1].descriptorCount = 1;
            ibl_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            ibl_writes[1].pImageInfo = &ibl_infos[1];
        }

        // SkinnedInstBuf SSBO (set=2, binding=10) — 硬件实例化路径
        VkDescriptorBufferInfo inst_ssbo_info{};
        VkWriteDescriptorSet inst_ssbo_write{};
        if (has_binding(2, 10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)) {
            inst_ssbo_info.buffer = (inst_ssbo != VK_NULL_HANDLE && inst_ssbo_size > 0)
                ? inst_ssbo : dummy_ssbo_buffer_;
            inst_ssbo_info.offset = (inst_ssbo != VK_NULL_HANDLE && inst_ssbo_size > 0)
                ? inst_ssbo_offset : 0;
            inst_ssbo_info.range = (inst_ssbo != VK_NULL_HANDLE && inst_ssbo_size > 0)
                ? inst_ssbo_size : VK_WHOLE_SIZE;
            inst_ssbo_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            inst_ssbo_write.dstSet          = sets[2];
            inst_ssbo_write.dstBinding      = 10;
            inst_ssbo_write.descriptorCount = 1;
            inst_ssbo_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            inst_ssbo_write.pBufferInfo     = &inst_ssbo_info;
        }

        std::vector<VkWriteDescriptorSet> all_writes;
        if (mat_write.sType != 0) all_writes.push_back(mat_write);
        for (int i = 0; i < 5; ++i) {
            if (tex_writes[i].sType != 0) {
                all_writes.push_back(tex_writes[i]);
            }
        }
        if (shadow_write.sType != 0) all_writes.push_back(shadow_write);
        if (spot_shadow_write.sType != 0) all_writes.push_back(spot_shadow_write);
        if (bone_write.sType != 0) all_writes.push_back(bone_write);
        if (morph_write.sType != 0) all_writes.push_back(morph_write);
        if (inst_ssbo_write.sType != 0) all_writes.push_back(inst_ssbo_write);
        if (spot_data_write.sType != 0) all_writes.push_back(spot_data_write);
        if (terrain_write.sType != 0) all_writes.push_back(terrain_write);
        for (int i = 0; i < 5; ++i) {
            if (splat_writes[i].sType != 0) all_writes.push_back(splat_writes[i]);
        }
        for (int i = 0; i < 2; ++i) {
            if (ibl_writes[i].sType != 0) all_writes.push_back(ibl_writes[i]);
        }
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(all_writes.size()), all_writes.data(), 0, nullptr);
    } // else (PBR mode Set 2)

    // --- Set 3 binding 0: 点光源立方体阴影贴图 (u_point_shadow_maps[4]) ---
    if (!gbuffer_mode && set_count >= 4 && sets[3] != VK_NULL_HANDLE
        && has_binding(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
        VkDescriptorImageInfo point_shadow_infos[4] = {};
        VkWriteDescriptorSet  point_shadow_write{};
        {
            VkSampler lin_sampler = resource_mgr.default_sampler();
            const VulkanTexture* white_tex = resource_mgr.GetTexture(white_cubemap_handle_);
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

    // VUID-VkWriteDescriptorSet-descriptorType-00331: SSBO 占位 buffer 必须有 STORAGE_BUFFER usage
    VkDescriptorBufferInfo dummy_ssbo_info{};
    dummy_ssbo_info.buffer = dummy_ssbo_buffer_;
    dummy_ssbo_info.offset = 0;
    dummy_ssbo_info.range  = VK_WHOLE_SIZE;

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
        w.pBufferInfo = &dummy_ssbo_info;
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

    // 反射辅助: 检查 (set, binding) 是否存在于着色器中
    auto has_binding = [&](uint32_t set_idx, uint32_t bind_idx) -> bool {
        for (const auto& b : program->reflection.bindings)
            if (b.set == set_idx && b.binding == bind_idx) return true;
        return false;
    };

    // Set 0 (layouts[0]): PerFrame UBO — binding 0
    if (has_binding(0, 0)) {
        push_ubo(sets[0], 0);
    }

    // Set 1 (layouts[1]): PerScene(b0) + PointLightSSBO(b1) + SpotLightSSBO(b2) + ClusterInfo(b3) + LightIndex(b4)
    if (set_count > 1) {
        if (has_binding(1, 0)) push_ubo(sets[1], 0);
        if (has_binding(1, 1)) push_ssbo(sets[1], 1);
        if (has_binding(1, 2)) push_ssbo(sets[1], 2);
        if (has_binding(1, 3)) push_ssbo(sets[1], 3);
        if (has_binding(1, 4)) push_ssbo(sets[1], 4);
    }

    // Set 2 (layouts[2]): PerMaterial(b0) + textures(b1-5) + shadow(b6,b7) + bone(b8,b9)
    if (set_count > 2) {
        if (has_binding(2, 0)) push_ubo(sets[2], 0);

        // binding 1: skybox cubemap（实际纹理）
        if (has_binding(2, 1)) {
            const VulkanTexture* cubemap_tex = resource_mgr.GetTexture(cubemap_texture_handle);
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
            if (!has_binding(2, b)) continue;
            size_t base = push_img(sets[2], b, 1);
            img_fixups.push_back({writes.size() - 1, base});
        }

        // binding 6: shadow_map[3]
        if (has_binding(2, 6)) { size_t base = push_img(sets[2], 6, 3); img_fixups.push_back({writes.size()-1, base}); }

        // binding 7: spot_shadow_map[4]
        if (has_binding(2, 7)) { size_t base = push_img(sets[2], 7, 4); img_fixups.push_back({writes.size()-1, base}); }

        // binding 8: BoneMatrices SSBO, binding 9: MorphWeights UBO
        if (has_binding(2, 8)) push_ssbo(sets[2], 8);
        if (has_binding(2, 9)) push_ubo(sets[2], 9);
    }

    // Set 3 (layouts[3]): point_shadow_maps[4] — binding 0
    if (set_count > 3 && has_binding(3, 0)) {
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

    // VUID-VkWriteDescriptorSet-descriptorType-00331: SSBO 占位 buffer 必须有 STORAGE_BUFFER usage
    VkDescriptorBufferInfo dummy_ssbo{};
    dummy_ssbo.buffer = dummy_ssbo_buffer_;
    dummy_ssbo.offset = 0;
    dummy_ssbo.range  = VK_WHOLE_SIZE;

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
        w.pBufferInfo = &dummy_ssbo;
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

    // 反射辅助: 检查 (set, binding) 是否存在于着色器中
    auto has_binding = [&](uint32_t s, uint32_t b) -> bool {
        for (const auto& rb : program->reflection.bindings)
            if (rb.set == s && rb.binding == b) return true;
        return false;
    };

    // Set 0: binding 0 (PerFrame UBO)
    if (set_count > 0 && has_binding(0, 0)) push_ubo(0, 0);

    // Set 1: binding 0 (PerScene UBO), 1-2 (Point/SpotLightSSBO), 3-4 (ClusterInfo/LightIndex SSBO), 5 (LightProbeData UBO)
    if (set_count > 1) {
        if (has_binding(1, 0)) push_ubo(1, 0);
        if (has_binding(1, 1)) push_ssbo2(1, 1);
        if (has_binding(1, 2)) push_ssbo2(1, 2);
        if (has_binding(1, 3)) push_ssbo2(1, 3);
        if (has_binding(1, 4)) push_ssbo2(1, 4);
        if (has_binding(1, 5)) push_ubo(1, 5);
    }

    // Set 2: binding 0 (PerMaterial UBO), 1-5 (textures), 6 (shadow[3]), 7 (spot_shadow[4]), 8-10 (bones/morph/instancing)
    if (set_count > 2) {
        if (has_binding(2, 0)) push_ubo(2, 0);
        for (uint32_t b = 1; b <= 5; ++b) { if (has_binding(2, b)) push_img(2, b, 1); }
        if (has_binding(2, 6)) push_img(2, 6, 3);
        if (has_binding(2, 7)) push_img(2, 7, 4);
        if (has_binding(2, 8)) push_ssbo2(2, 8);
        if (has_binding(2, 9)) push_ubo(2, 9);
        if (has_binding(2, 10)) push_ssbo2(2, 10);
        if (has_binding(2, 19)) push_ubo(2, 19);
    }

    // Set 3: binding 0 (point_shadow_maps[4])
    if (set_count > 3 && has_binding(3, 0)) push_img(3, 0, 4);

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

    auto resolve_image_info = [&](unsigned int handle, VkDescriptorImageInfo fallback) {
        VkDescriptorImageInfo info = fallback;
        const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(handle);
        if (rt) {
            if (rt->color_texture.image_view != VK_NULL_HANDLE) {
                info.imageView = rt->color_texture.image_view;
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                return info;
            }
            if (rt->depth_texture.image_view != VK_NULL_HANDLE) {
                info.imageView = rt->depth_texture.image_view;
                info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                return info;
            }
        }
        const VulkanTexture* tex = resource_mgr.GetTexture(handle);
        if (tex) {
            info.imageView = tex->image_view;
            if (tex->sampler != VK_NULL_HANDLE) info.sampler = tex->sampler;
        }
        return info;
    };

    if (source_texture != 0) {
        VkImageView before = src_img.imageView;
        src_img = resolve_image_info(source_texture, src_img);
        if (src_img.imageView == before && !resource_mgr.GetRenderTarget(source_texture)
            && !resource_mgr.GetTexture(source_texture)) {
            DEBUG_LOG_WARN("[Vulkan] PostProcess src: handle={} NOT FOUND as RT or Texture, using dummy", source_texture);
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
            // binding 5 在后处理 shader 中是 sampler3D (u_lut)，需要 3D image view
            if (binding == 5 && dummy_3d_image_view_ != VK_NULL_HANDLE) {
                ei.imageView = dummy_3d_image_view_;
            } else {
                ei.imageView = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
            }
            ei.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            if (tex_handle != 0) {
                ei = resolve_image_info(tex_handle, ei);
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
            // 根据是否需要清除选择 render pass 变体
            if (!render_pass.clear_color_enabled && rt->render_pass_load != VK_NULL_HANDLE) {
                vk_render_pass = rt->render_pass_load;
            } else {
                vk_render_pass = rt->render_pass;
            }
        }
    } else {
        // 渲染到屏幕：使用当前 swapchain framebuffer
        framebuffer = context_->current_swapchain_framebuffer();
        vk_render_pass = context_->swapchain_render_pass();

        DEBUG_LOG_INFO("[Vulkan] BeginRenderPass SWAPCHAIN: fb={} rp={} imgIdx={}",
                       (uint64_t)(uintptr_t)framebuffer,
                       (uint64_t)(uintptr_t)vk_render_pass,
                       context_->current_image_index());
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

    // 计算实际 attachment 数（兼容 MRT GBuffer 与 MSAA resolve）
    const bool is_msaa_rt = (current_msaa_samples_ != VK_SAMPLE_COUNT_1_BIT);
    int num_color = 1;
    bool rt_color_present = true;
    bool rt_depth_present = false;
    if (render_pass.render_target != 0) {
        const VulkanRenderTarget* rt_for_attachments = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt_for_attachments) {
            rt_color_present = rt_for_attachments->has_color;
            rt_depth_present = rt_for_attachments->has_depth;
            // MSAA 路径只用 1 个 color attachment（msaa_color_texture），非 MSAA MRT 才有多个
            num_color = is_msaa_rt ? 1 : (std::max)(1, rt_for_attachments->color_attachment_count);
            if (!rt_color_present) num_color = 0;
        }
    } else {
        // swapchain：只有 1 个 color，无 depth
        num_color = 1;
        rt_depth_present = false;
    }

    std::vector<VkClearValue> clear_values;
    VkClearValue color_cv{};
    color_cv.color = {{render_pass.clear_color.x,
                        render_pass.clear_color.y,
                        render_pass.clear_color.z,
                        render_pass.clear_color.w}};
    VkClearValue depth_cv{};
    depth_cv.depthStencil = {1.0f, 0};

    // 顺序必须严格匹配 CreateRenderTarget 中 attachments 的 push 顺序：
    //   [0..num_color-1] color attachments (或 MSAA 时 1 个 MSAA color)
    //   [num_color]      depth (如果有)
    //   [num_color+1]    resolve target (仅 MSAA + has_color)
    for (int i = 0; i < num_color; ++i) clear_values.push_back(color_cv);
    if (rt_depth_present) clear_values.push_back(depth_cv);
    if (rt_color_present && is_msaa_rt) clear_values.push_back(color_cv);

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

    // VUID-VkGraphicsPipelineCreateInfo-renderPass-07609: pipeline 的 colorBlend attachment 数
    // 必须等于 RP subpass 的 color attachment 数。MRT GBuffer 时 num_color>1。
    current_color_attachment_count_ = num_color;
    global_state_.current_frame_stats.render_passes += 1;
    if (!rt_color_present && rt_depth_present) {
        global_state_.current_frame_stats.shadow_passes += 1;
    }
    DEBUG_LOG_TRACE("[Vulkan] BeginRenderPass: rt={} extent={}x{} msaa={} color_count={} depth={} pass#={}",
                   render_pass.render_target,
                   render_extent.width, render_extent.height,
                   static_cast<int>(current_msaa_samples_),
                   num_color, rt_depth_present,
                   render_pass_counter_ - 1);

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

    // 对 offscreen RT 的颜色附件插入显式 image barrier，确保 layout 转换和内存可见性
    if (current_rt_handle_ != 0 && resource_mgr_) {
        const VulkanRenderTarget* rt = resource_mgr_->GetRenderTarget(current_rt_handle_);
        if (rt && rt->has_color && rt->color_texture.image != VK_NULL_HANDLE) {
            VkImageMemoryBarrier img_barrier{};
            img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            img_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            img_barrier.image = rt->color_texture.image;
            img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            img_barrier.subresourceRange.baseMipLevel = 0;
            img_barrier.subresourceRange.levelCount = 1;
            img_barrier.subresourceRange.baseArrayLayer = 0;
            img_barrier.subresourceRange.layerCount = 1;
            img_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd_buf,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &img_barrier);

            // MSAA resolve target 也需要 barrier
            if (rt->is_msaa && rt->msaa_color_texture.image != VK_NULL_HANDLE) {
                VkImageMemoryBarrier msaa_barrier = img_barrier;
                msaa_barrier.image = rt->msaa_color_texture.image;
                msaa_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                msaa_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                vkCmdPipelineBarrier(cmd_buf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &msaa_barrier);
            }
        }
        // 深度附件 barrier
        if (rt && rt->has_depth && rt->depth_texture.image != VK_NULL_HANDLE) {
            VkImageMemoryBarrier depth_barrier{};
            depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depth_barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depth_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depth_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depth_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depth_barrier.image = rt->depth_texture.image;
            // VUID-VkImageMemoryBarrier-image-03320: D24S8/D32S8 必须同时声明 DEPTH+STENCIL aspect
            const VkFormat dfmt = rt->depth_texture.format;
            const bool has_stencil = (dfmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                                      dfmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                      dfmt == VK_FORMAT_D16_UNORM_S8_UINT);
            depth_barrier.subresourceRange.aspectMask = has_stencil
                ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                : VK_IMAGE_ASPECT_DEPTH_BIT;
            depth_barrier.subresourceRange.baseMipLevel = 0;
            depth_barrier.subresourceRange.levelCount = 1;
            depth_barrier.subresourceRange.baseArrayLayer = 0;
            depth_barrier.subresourceRange.layerCount = 1;
            depth_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depth_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd_buf,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &depth_barrier);
        }
    } else {
        // swapchain 或未知 RT，使用全局 memory barrier
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
    }

    current_render_pass_ = VK_NULL_HANDLE;
    current_msaa_samples_ = VK_SAMPLE_COUNT_1_BIT;
}

// ============================================================================
// BlitRenderTargetToSwapchain — 诊断：直接 blit RT→swapchain，绕过 shader
// ============================================================================

void VulkanDrawExecutor::BlitRenderTargetToSwapchain(
    VkCommandBuffer cmd_buf,
    unsigned int source_rt,
    VulkanResourceManager& resource_mgr) {

    const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(source_rt);
    if (!rt || !rt->has_color || rt->color_texture.image == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("[Vulkan] BlitToSwapchain: invalid source RT {}", source_rt);
        return;
    }

    VkImage src_image = rt->color_texture.image;
    VkImage dst_image = context_->swapchain_images()[context_->current_image_index()];
    VkExtent2D extent = context_->swapchain_extent();

    // 1. RT color → TRANSFER_SRC
    VkImageMemoryBarrier src_barrier{};
    src_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    src_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    src_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = src_image;
    src_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    src_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                              | VK_ACCESS_SHADER_READ_BIT
                              | VK_ACCESS_TRANSFER_WRITE_BIT;
    src_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    // 2. Swapchain → TRANSFER_DST
    VkImageMemoryBarrier dst_barrier{};
    dst_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dst_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dst_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.image = dst_image;
    dst_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    dst_barrier.srcAccessMask = 0;
    dst_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkImageMemoryBarrier barriers[] = {src_barrier, dst_barrier};
    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    // 3. Blit
    VkImageBlit blit_region{};
    blit_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit_region.srcOffsets[0] = {0, 0, 0};
    blit_region.srcOffsets[1] = {static_cast<int32_t>(rt->width),
                                 static_cast<int32_t>(rt->height), 1};
    blit_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit_region.dstOffsets[0] = {0, 0, 0};
    blit_region.dstOffsets[1] = {static_cast<int32_t>(extent.width),
                                 static_cast<int32_t>(extent.height), 1};

    vkCmdBlitImage(cmd_buf,
        src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit_region, VK_FILTER_LINEAR);

    // 4. RT → SHADER_READ_ONLY, Swapchain → PRESENT_SRC
    src_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    src_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    src_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    src_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    dst_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dst_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst_barrier.dstAccessMask = 0;

    VkImageMemoryBarrier barriers2[] = {src_barrier, dst_barrier};
    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers2);

    DEBUG_LOG_INFO("[Vulkan] BlitToSwapchain: rt={} ({}x{}) → swapchain ({}x{})",
                   source_rt, rt->width, rt->height, extent.width, extent.height);
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
        current_color_attachment_count_,
        false);  // wireframe not applicable for sprites

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
    // 每次 pass 重置共享模板跟踪（避免跨 pass 误判）
    vk_last_shared_vtx_ptr_ = nullptr;
    vk_last_shared_vtx_count_ = 0;

    global_state_.current_frame_stats.mesh_count += static_cast<int>(items.size());
    dse::render::UpdateSortBatchStats(global_state_.current_frame_stats, items);

    const bool gbuffer_mode = global_state_.gbuffer_rendering_mode;
    const bool is_depth_only = (current_color_attachment_count_ == 0);
    unsigned int active_shader_handle = is_depth_only
        ? shader_mgr.shadow_shader_handle()
        : (gbuffer_mode ? shader_mgr.gbuffer_shader_handle() : shader_mgr.pbr_shader_handle());
    if (is_depth_only && active_shader_handle == 0)
        active_shader_handle = shader_mgr.pbr_shader_handle();  // fallback
    const VulkanShaderProgram* pbr_program = shader_mgr.GetProgram(active_shader_handle);
    if (!pbr_program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor: {} shader not available",
                       is_depth_only ? "Shadow" : (gbuffer_mode ? "GBuffer" : "PBR"));
        return;
    }

    // 使用当前激活的 render pass（可能是离屏 RT 的 render pass）
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // 3D Mesh 顶点格式定义
    // 注意：标准 PBR 着色器不支持 per-instance vertex attribute（location 7-10），
    // instancing 通过逐实例更新 push constant 实现，因此 pipeline 只需 binding 0。
    std::vector<VkVertexInputBindingDescription> mesh_bindings = {
        {0, sizeof(BatchVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    std::vector<VkVertexInputAttributeDescription> mesh_attrs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},       // aPos
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12},   // aColor
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 28},          // aTexCoord
        {3, 0, VK_FORMAT_R32G32B32_SFLOAT, 36},       // aNormal
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 48},       // aTangent
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 60},   // aBoneWeights
        {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 76},   // aBoneIndices
    };

    // 延迟创建 VkPipeline（按需，首次 Draw 时创建）— 有剔除版本
    VkPipeline vk_pipeline_cull = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        pbr_program, active_rp, mesh_bindings, mesh_attrs,
        context_->swapchain_extent(), current_msaa_samples_,
        current_color_attachment_count_,
        global_state_.wireframe_mode);

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
        current_color_attachment_count_,
        global_state_.wireframe_mode);

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

    // === Bone SSBO: 收集所有蒙皮实例骨骼矩阵，一次写入 ===
    // 优先使用 bone_palette（去重后数据量极小），fallback 到 per_instance_bones
    std::vector<int> bone_offsets(items.size(), 0);
    std::vector<std::vector<int>> per_inst_bone_offsets(items.size());
    std::vector<std::vector<int>> palette_base_offsets(items.size());
    VkDeviceSize vk_bone_ssbo_total_bytes = 0;
    {
        size_t total_bones = 0;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& it = items[i];
            if (it.skinned && !it.bone_palette.empty()) {
                auto& pbo = palette_base_offsets[i];
                pbo.resize(it.bone_palette.size());
                for (size_t p = 0; p < it.bone_palette.size(); ++p) {
                    pbo[p] = static_cast<int>(total_bones);
                    total_bones += (std::min)(it.bone_palette[p].size(), static_cast<size_t>(255));
                }
                auto& offsets = per_inst_bone_offsets[i];
                offsets.resize(it.instance_bone_palette_idx.size());
                for (size_t j = 0; j < it.instance_bone_palette_idx.size(); ++j) {
                    int pidx = it.instance_bone_palette_idx[j];
                    offsets[j] = (pidx >= 0 && pidx < static_cast<int>(pbo.size())) ? pbo[pidx] : 0;
                }
            } else if (it.skinned && !it.per_instance_bones.empty()) {
                auto& offsets = per_inst_bone_offsets[i];
                offsets.resize(it.per_instance_bones.size());
                for (size_t j = 0; j < it.per_instance_bones.size(); ++j) {
                    offsets[j] = static_cast<int>(total_bones);
                    total_bones += (std::min)(it.per_instance_bones[j].size(), static_cast<size_t>(255));
                }
            } else if (it.skinned && !it.bone_matrices.empty()) {
                bone_offsets[i] = static_cast<int>(total_bones);
                total_bones += (std::min)(it.bone_matrices.size(), static_cast<size_t>(255));
            }
        }
        if (total_bones > 0) {
            vk_bone_ssbo_total_bytes = total_bones * sizeof(glm::mat4);
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& it = items[i];
                if (it.skinned && !it.bone_palette.empty()) {
                    for (size_t p = 0; p < it.bone_palette.size(); ++p) {
                        size_t count = (std::min)(it.bone_palette[p].size(), static_cast<size_t>(255));
                        VkDeviceSize offset = palette_base_offsets[i][p] * sizeof(glm::mat4);
                        WriteToBuffer(context_->device(), bone_matrices_ubo_mem_, offset,
                                      count * sizeof(glm::mat4), it.bone_palette[p].data());
                    }
                } else if (it.skinned && !it.per_instance_bones.empty()) {
                    for (size_t j = 0; j < it.per_instance_bones.size(); ++j) {
                        size_t count = (std::min)(it.per_instance_bones[j].size(), static_cast<size_t>(255));
                        VkDeviceSize offset = per_inst_bone_offsets[i][j] * sizeof(glm::mat4);
                        WriteToBuffer(context_->device(), bone_matrices_ubo_mem_, offset,
                                      count * sizeof(glm::mat4), it.per_instance_bones[j].data());
                    }
                } else if (it.skinned && !it.bone_matrices.empty()) {
                    size_t count = (std::min)(it.bone_matrices.size(), static_cast<size_t>(255));
                    VkDeviceSize offset = bone_offsets[i] * sizeof(glm::mat4);
                    WriteToBuffer(context_->device(), bone_matrices_ubo_mem_, offset,
                                  count * sizeof(glm::mat4), it.bone_matrices.data());
                }
            }
        }
    }

    // --- 预计算实例 SSBO 总需求（skinned + non-skinned），一次性分配 ---
    {
        constexpr VkDeviceSize kSkinSSBOAlign = 256; // VK minStorageBufferOffsetAlignment 上限
        struct SkinnedInstGPUSz { glm::mat4 m; int b; int p[3]; };
        VkDeviceSize total_needed = 0;
        for (const auto& it : items) {
            if (it.instance_transforms.size() <= 1) continue;
            VkDeviceSize sz = it.instance_transforms.size() * sizeof(SkinnedInstGPUSz);
            total_needed += (sz + kSkinSSBOAlign - 1) & ~(kSkinSSBOAlign - 1);
        }
        if (total_needed > skinned_inst_ssbo_capacity_) {
            VkDevice dev2 = context_->device();
            VkPhysicalDevice phys2 = context_->physical_device();
            if (skinned_inst_ssbo_ != VK_NULL_HANDLE) {
                UnmapPersistentBuffer(dev2, skinned_inst_ssbo_mem_);
                vkDestroyBuffer(dev2, skinned_inst_ssbo_, nullptr);
                vkFreeMemory(dev2, skinned_inst_ssbo_mem_, nullptr);
                skinned_inst_ssbo_ = VK_NULL_HANDLE;
            }
            size_t new_cap = static_cast<size_t>(total_needed) * 2;
            CreateVulkanBuffer(dev2, phys2, new_cap,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                skinned_inst_ssbo_, skinned_inst_ssbo_mem_);
            skinned_inst_ssbo_capacity_ = new_cap;
        }
    }
    VkDeviceSize skinned_ssbo_write_offset = 0; // 当前帧在共享 SSBO 中的写入游标

    // 逐 mesh 绘制
    unsigned int last_material_tex = (std::numeric_limits<unsigned int>::max)();
    for (size_t item_idx = 0; item_idx < items.size(); ++item_idx) {
        const auto& item = items[item_idx];
        if (item.texture_handle != last_material_tex) {
            if (last_material_tex != (std::numeric_limits<unsigned int>::max)())
                global_state_.current_frame_stats.material_switches++;
            last_material_tex = item.texture_handle;
        }
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
        VkDeviceSize cur_pl_offset       = 0;
        VkDeviceSize cur_sl_offset       = 0;
        UpdatePerSceneUBO(item);
        UpdatePerMaterialUBO(item);
        per_scene_ubo_offset_        += kUboSlotAlignment;
        per_material_ubo_offset_     += kUboSlotAlignment;

        // Bone SSBO: offset=0, range=total（push constant u_bone_offset 控制索引）
        VkDeviceSize cur_bone_offset = 0;

        // 解析顶点/索引数据源：优先使用 shared_vertex_ptr
        const BatchVertex* vtx_data = item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
        const uint32_t* idx_data = item.shared_index_ptr ? item.shared_index_ptr : item.indices.data();
        const size_t vtx_count = item.shared_vertex_ptr ? item.shared_vertex_count : item.vertices.size();
        const size_t idx_count = item.shared_index_ptr ? item.shared_index_count : item.indices.size();

        // 同 pass 内连续相同 shared_vertex_ptr → 复用已上传的 VBO/IBO 偏移
        const bool reuse_upload = item.shared_vertex_ptr
            && item.shared_vertex_ptr == vk_last_shared_vtx_ptr_
            && vtx_count == vk_last_shared_vtx_count_;

        VkDeviceSize cur_vbo_offset;
        VkDeviceSize cur_ibo_offset;
        if (reuse_upload) {
            cur_vbo_offset = vk_last_shared_vbo_offset_;
            cur_ibo_offset = vk_last_shared_ibo_offset_;
        } else {
            cur_vbo_offset = mesh_vbo_offset_;
            if (vtx_count > 0) {
                size_t vdata_size = vtx_count * sizeof(BatchVertex);
                if (mesh_vbo_offset_ + vdata_size > mesh_vbo_capacity_) {
                    DEBUG_LOG_ERROR("[Vulkan] VBO OVERFLOW: offset={} + size={} > capacity={}", mesh_vbo_offset_, vdata_size, mesh_vbo_capacity_);
                } else {
                    WriteToBuffer(context_->device(), mesh_vbo_mem_, mesh_vbo_offset_, vdata_size, vtx_data);
                }
                mesh_vbo_offset_ += vdata_size;
            }

            cur_ibo_offset = mesh_ibo_offset_;
            if (idx_count > 0) {
                size_t idata_size = idx_count * sizeof(uint32_t);
                if (mesh_ibo_offset_ + idata_size > mesh_ibo_capacity_) {
                    DEBUG_LOG_ERROR("[Vulkan] IBO OVERFLOW: offset={} + size={} > capacity={}", mesh_ibo_offset_, idata_size, mesh_ibo_capacity_);
                } else {
                    WriteToBuffer(context_->device(), mesh_ibo_mem_, mesh_ibo_offset_, idata_size, idx_data);
                }
                mesh_ibo_offset_ += idata_size;
            }

            if (item.shared_vertex_ptr) {
                vk_last_shared_vtx_ptr_ = item.shared_vertex_ptr;
                vk_last_shared_vtx_count_ = vtx_count;
                vk_last_shared_vbo_offset_ = cur_vbo_offset;
                vk_last_shared_ibo_offset_ = cur_ibo_offset;
            } else {
                vk_last_shared_vtx_ptr_ = nullptr;
                vk_last_shared_vtx_count_ = 0;
            }
        }

        // Push constant 结构
        const bool is_instanced = item.instance_transforms.size() > 1;
        struct {
            glm::mat4 model;
            int skinned;
            int morph_enabled;
            int bone_offset;
        } pc_data;
        pc_data.model = item.model;
        pc_data.skinned = item.skinned ? 1 : 0;
        pc_data.morph_enabled = item.morph_enabled ? 1 : 0;
        pc_data.bone_offset = item.skinned ? bone_offsets[item_idx] : 0;

        // 绑定 VBO binding 0
        VkBuffer vbo_buffers[] = {mesh_vbo_};
        VkDeviceSize vbo_offsets[] = {cur_vbo_offset};
        vkCmdBindVertexBuffers(cmd_buf, 0, 1, vbo_buffers, vbo_offsets);
        vkCmdBindIndexBuffer(cmd_buf, mesh_ibo_, cur_ibo_offset, VK_INDEX_TYPE_UINT32);

        // 绘制
        if (is_instanced) {
            // --- Shadow instance culling (与 OpenGL/DX11 一致) ---
            constexpr float kShadowCullMargin     = 150.0f;
            constexpr float kBudgetOrthoThreshold = 2000.0f;
            constexpr float kBudgetBaseInstances  = 800.0f;
            constexpr float kBudgetMinInstances   = 64.0f;
            constexpr float kSkinnedShadowSkipOrtho = 1500.0f;
            constexpr float kSkinnedBudgetOrtho     = 400.0f;
            constexpr float kSkinnedBudgetBase      = 200.0f;

            // Vulkan clip correction 将 ortho 的 [3][3] 从 1.0 变为 ~0.5
            // 改用 [2][3]==0（透视投影 [2][3]==-1，正交投影 [2][3]==0）
            const bool is_ortho = std::abs(projection[2][3]) < 0.01f;
            const bool shadow_cull_active = is_depth_only && is_ortho;
            float shadow_cull_limit = 0.0f;
            size_t shadow_instance_budget = SIZE_MAX;
            size_t skinned_shadow_budget  = SIZE_MAX;
            if (shadow_cull_active && std::abs(projection[0][0]) > 1e-6f) {
                float ortho_size = 1.0f / projection[0][0];
                shadow_cull_limit = ortho_size + kShadowCullMargin;
                if (ortho_size > kBudgetOrthoThreshold) {
                    shadow_instance_budget = static_cast<size_t>(
                        (std::max)(kBudgetBaseInstances * kBudgetOrthoThreshold / ortho_size, kBudgetMinInstances));
                }
                if (ortho_size > kSkinnedShadowSkipOrtho) {
                    skinned_shadow_budget = 0;
                } else if (ortho_size > kSkinnedBudgetOrtho) {
                    skinned_shadow_budget = static_cast<size_t>(
                        (std::max)(kSkinnedBudgetBase * kSkinnedBudgetOrtho / ortho_size, 0.0f));
                }
            }

            const bool skinned_instanced = item.skinned && (!item.per_instance_bones.empty() || !item.bone_palette.empty());
            const auto& inst_bo = per_inst_bone_offsets[item_idx];

            // PreZ 跳过：蒙皮实例在深度预填充 pass（透视深度）收益极小，直接跳过节省 VS 时间
            // shadow(ortho depth-only) 仍正常绘制；forward pass 不受影响
            const bool is_prez = is_depth_only && !is_ortho;
            if (is_prez && skinned_instanced) continue;

            // 蒙皮实例: 硬件实例化（SSBO + skinned=2 + instanceCount>1）
            // 非蒙皮实例: pseudo-instancing（push constant 逐实例更新）
            if (skinned_instanced) {
                struct SkinnedInstGPU {
                    glm::mat4 model;
                    int bone_offset;
                    int _pad0, _pad1, _pad2;
                };
                thread_local std::vector<SkinnedInstGPU> visible_instances;
                visible_instances.clear();
                visible_instances.reserve(item.instance_transforms.size());
                const size_t effective_budget = (skinned_shadow_budget < shadow_instance_budget)
                    ? skinned_shadow_budget : shadow_instance_budget;
                for (size_t inst = 0; inst < item.instance_transforms.size(); ++inst) {
                    if (shadow_cull_active) {
                        if (visible_instances.size() >= effective_budget) break;
                        if (shadow_cull_limit > 0.0f) {
                            const glm::vec3 wp(item.instance_transforms[inst][3]);
                            const glm::vec4 ls = view * glm::vec4(wp, 1.0f);
                            if (std::abs(ls.x) > shadow_cull_limit || std::abs(ls.y) > shadow_cull_limit)
                                continue;
                        }
                    }
                    SkinnedInstGPU gpu_inst{};
                    gpu_inst.model = item.instance_transforms[inst];
                    gpu_inst.bone_offset = (inst < inst_bo.size()) ? inst_bo[inst] : 0;
                    visible_instances.push_back(gpu_inst);
                }

                const size_t inst_count = visible_instances.size();
                if (inst_count > 0) {
                    constexpr VkDeviceSize kSkinSSBOAlign = 256;
                    const size_t inst_data_size = inst_count * sizeof(SkinnedInstGPU);
                    const VkDeviceSize item_ssbo_offset = skinned_ssbo_write_offset;
                    WriteToBuffer(context_->device(), skinned_inst_ssbo_mem_,
                        item_ssbo_offset, inst_data_size, visible_instances.data());
                    skinned_ssbo_write_offset += (inst_data_size + kSkinSSBOAlign - 1) & ~(kSkinSSBOAlign - 1);

                    AllocateAndUpdateMeshDescriptorSets(cmd_buf, pbr_program, item, resource_mgr,
                        cur_bone_offset, cur_per_frame_offset, cur_scene_offset,
                        cur_material_offset, cur_pl_offset, cur_sl_offset, gbuffer_mode,
                        skinned_inst_ssbo_, static_cast<VkDeviceSize>(inst_data_size), item_ssbo_offset);

                    pc_data.skinned = 2;
                    pc_data.bone_offset = 0;
                    vkCmdPushConstants(cmd_buf, pbr_program->pipeline_layout,
                                       VK_SHADER_STAGE_VERTEX_BIT,
                                       0, sizeof(pc_data), &pc_data);
                    vkCmdDrawIndexed(cmd_buf,
                                     static_cast<uint32_t>(idx_count),
                                     static_cast<uint32_t>(inst_count), 0, 0, 0);
                }
                global_state_.current_frame_stats.instanced_draw_calls++;
                global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(inst_count);
                global_state_.current_frame_stats.draw_calls += (inst_count > 0) ? 1 : 0;
            } else {
                // 非蒙皮: 硬件实例化（SSBO + skinned=3 + instanceCount>1）
                // skinned==3 在 GLSL 中跳过骨骼蒙皮，从 SSBO 读 model
                struct SkinnedInstGPU {
                    glm::mat4 model;
                    int bone_offset;
                    int _pad0, _pad1, _pad2;
                };
                thread_local std::vector<SkinnedInstGPU> visible_instances_ns;
                visible_instances_ns.clear();
                visible_instances_ns.reserve(item.instance_transforms.size());
                for (size_t inst = 0; inst < item.instance_transforms.size(); ++inst) {
                    if (shadow_cull_active) {
                        if (visible_instances_ns.size() >= shadow_instance_budget) break;
                        if (shadow_cull_limit > 0.0f) {
                            const glm::vec3 wp(item.instance_transforms[inst][3]);
                            const glm::vec4 ls = view * glm::vec4(wp, 1.0f);
                            if (std::abs(ls.x) > shadow_cull_limit || std::abs(ls.y) > shadow_cull_limit)
                                continue;
                        }
                    }
                    SkinnedInstGPU gpu_inst{};
                    gpu_inst.model = item.instance_transforms[inst];
                    gpu_inst.bone_offset = 0;
                    visible_instances_ns.push_back(gpu_inst);
                }

                const size_t inst_count = visible_instances_ns.size();
                if (inst_count > 0) {
                    constexpr VkDeviceSize kSkinSSBOAlign = 256;
                    const size_t inst_data_size = inst_count * sizeof(SkinnedInstGPU);
                    const VkDeviceSize item_ssbo_offset = skinned_ssbo_write_offset;
                    WriteToBuffer(context_->device(), skinned_inst_ssbo_mem_,
                        item_ssbo_offset, inst_data_size, visible_instances_ns.data());
                    skinned_ssbo_write_offset += (inst_data_size + kSkinSSBOAlign - 1) & ~(kSkinSSBOAlign - 1);

                    AllocateAndUpdateMeshDescriptorSets(cmd_buf, pbr_program, item, resource_mgr,
                        cur_bone_offset, cur_per_frame_offset, cur_scene_offset,
                        cur_material_offset, cur_pl_offset, cur_sl_offset, gbuffer_mode,
                        skinned_inst_ssbo_, static_cast<VkDeviceSize>(inst_data_size), item_ssbo_offset);

                    pc_data.skinned = 3;
                    pc_data.bone_offset = 0;
                    vkCmdPushConstants(cmd_buf, pbr_program->pipeline_layout,
                                       VK_SHADER_STAGE_VERTEX_BIT,
                                       0, sizeof(pc_data), &pc_data);
                    vkCmdDrawIndexed(cmd_buf,
                                     static_cast<uint32_t>(idx_count),
                                     static_cast<uint32_t>(inst_count), 0, 0, 0);
                }
                global_state_.current_frame_stats.instanced_draw_calls++;
                global_state_.current_frame_stats.instanced_mesh_count += static_cast<int>(inst_count);
                global_state_.current_frame_stats.draw_calls += (inst_count > 0) ? 1 : 0;
            }
        } else {
            // 非实例化: 单个 descriptor set，不绑定 instance SSBO
            AllocateAndUpdateMeshDescriptorSets(cmd_buf, pbr_program, item, resource_mgr,
                cur_bone_offset, cur_per_frame_offset, cur_scene_offset,
                cur_material_offset, cur_pl_offset, cur_sl_offset, gbuffer_mode);

            vkCmdPushConstants(cmd_buf, pbr_program->pipeline_layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0, sizeof(pc_data), &pc_data);
            vkCmdDrawIndexed(cmd_buf,
                             static_cast<uint32_t>(idx_count),
                             1, 0, 0, 0);
            global_state_.current_frame_stats.draw_calls++;
        }
        global_state_.current_frame_stats.triangle_count += static_cast<int>(idx_count / 3) * static_cast<int>(is_instanced ? item.instance_transforms.size() : 1);
    }
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
        current_color_attachment_count_,
        false);  // wireframe not applicable for skybox

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
    const PostProcessRequest& request,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr) {

    const unsigned int source_texture = request.source_texture;
    const std::string& effect_name = request.effect_name;
    const std::vector<float>& params = request.params;

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

    // 根据 effect 名称选择专用着色器（表驱动）
    using ShaderAccessor = unsigned int (ShaderManagerBase::*)() const;
    static const std::unordered_map<std::string, ShaderAccessor> kShaderMap = {
        {"bloom_extract",     &ShaderManagerBase::bloom_extract_shader_handle},
        {"fxaa",              &ShaderManagerBase::fxaa_shader_handle},
        {"ssao",              &ShaderManagerBase::ssao_shader_handle},
        {"ssao_blur",         &ShaderManagerBase::ssao_blur_shader_handle},
        {"ssao_apply",        &ShaderManagerBase::ssao_apply_shader_handle},
        {"lum_compute",       &ShaderManagerBase::lum_compute_shader_handle},
        {"lum_adapt",         &ShaderManagerBase::lum_adapt_shader_handle},
        {"tonemapping",       &ShaderManagerBase::tonemapping_shader_handle},
        {"bloom_composite",   &ShaderManagerBase::bloom_composite_ssao_ae_shader_handle},
        {"color_grading",     &ShaderManagerBase::color_grading_shader_handle},
        {"contact_shadow",    &ShaderManagerBase::contact_shadow_shader_handle},
        {"taa_resolve",       &ShaderManagerBase::taa_resolve_shader_handle},
        {"dof",               &ShaderManagerBase::dof_shader_handle},
        {"motion_blur",       &ShaderManagerBase::motion_blur_shader_handle},
        {"motion_vector",     &ShaderManagerBase::motion_vector_shader_handle},
        {"ssr",               &ShaderManagerBase::ssr_shader_handle},
        {"deferred_lighting", &ShaderManagerBase::deferred_lighting_shader_handle},
        {"edge_detect",       &ShaderManagerBase::edge_detect_shader_handle},
        {"volumetric_fog",    &ShaderManagerBase::volumetric_fog_shader_handle},
        {"decal",             &ShaderManagerBase::decal_shader_handle},
        {"wboit_composite",   &ShaderManagerBase::wboit_composite_shader_handle},
        {"water",             &ShaderManagerBase::water_shader_handle},
        {"light_shaft",       &ShaderManagerBase::light_shaft_shader_handle},
    };
    unsigned int selected_shader_handle = shader_mgr.postprocess_shader_handle();
    {
        auto it = kShaderMap.find(effect_name);
        if (it != kShaderMap.end()) {
            unsigned int h = (shader_mgr.*(it->second))();
            if (h != 0) selected_shader_handle = h;
        }
    }

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
            current_color_attachment_count_,
            false);  // wireframe not applicable for postprocess
    }

    if (pp_pipeline != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pp_pipeline);
    }

    // 构建额外纹理绑定列表 {set2_binding, texture_handle}（表驱动）
    struct ExtraTexSlot { uint32_t binding; int findtex_slot; };
    static const std::unordered_map<std::string, std::vector<ExtraTexSlot>> kExtraTexTable = {
        {"tonemapping",       {{2, 2}, {5, 5}}},
        {"ssao_apply",        {{2, 2}, {3, 3}, {5, 5}}},
        {"lum_adapt",         {{2, 2}}},
        {"color_grading",     {{5, 5}}},
        {"taa_resolve",       {{2, 2}, {5, 5}}},
        {"dof",               {{2, 2}}},
        {"motion_blur",       {{2, 2}}},
        {"ssr",               {{2, 2}}},
        {"deferred_lighting", {{2, 2}, {3, 3}}},
        {"volumetric_fog",    {{2, 2}}},
        {"decal",             {{2, 2}, {3, 3}}},
        {"wboit_composite",   {{2, 2}}},
        {"water",             {{2, 2}}},
        {"light_shaft",       {{2, 2}}},
    };
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
    } else {
        auto tex_it = kExtraTexTable.find(effect_name);
        if (tex_it != kExtraTexTable.end()) {
            for (const auto& s : tex_it->second)
                extra_bindings.push_back({s.binding, request.FindTex(s.findtex_slot)});
        }
    }

    // 分配并绑定后处理 DescriptorSet
    if (pp_program) {
        AllocateAndUpdatePostProcessDescriptorSets(cmd_buf, pp_program,
                                                    source_texture, *resource_mgr_,
                                                    extra_bindings);
    }

    // 传递 push constants
    if (pp_program && pp_program->pipeline_layout != VK_NULL_HANDLE) {
        if (effect_name == "bloom_extract" && params.size() >= 1) {
            float pc = params[0];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "fxaa" && params.size() >= 2) {
            float pc[2] = {params[0], params[1]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "ssao" && params.size() >= 6) {
            float pc[6] = {params[0], params[1], params[2], params[3], params[4], params[5]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "ssao_apply" && params.size() >= 2) {
            struct { float exposure; int ae_enabled; int lut_enabled; float lut_intensity; } pc{};
            pc.exposure    = params[0];
            pc.ae_enabled  = request.FindTex(3) != 0 ? 1 : 0;
            pc.lut_enabled = request.FindTex(5) != 0 ? 1 : 0;
            pc.lut_intensity = params[1];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "lum_adapt" && params.size() >= 6) {
            // params: [dt, speed_up, speed_down, min, max, compensation]
            float pc[6] = {params[0], params[1], params[2], params[3], params[4], params[5]};
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "tonemapping") {
            struct { float manual_exposure; int ae_enabled; int lut_enabled; float lut_intensity; } pc{};
            pc.manual_exposure = (params.size() >= 1) ? params[0] : 1.0f;
            pc.ae_enabled = request.FindTex(2) != 0 ? 1 : 0;
            pc.lut_enabled = request.FindTex(5) != 0 ? 1 : 0;
            pc.lut_intensity = (params.size() >= 2) ? params[1] : 0.0f;
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
        } else if (effect_name == "color_grading" && params.size() >= 1) {
            float pc = params[0];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        } else if (effect_name == "taa_resolve" && params.size() >= 6) {
            struct { float blend_factor, jitter_x, jitter_y; int frame_index; float screen_w, screen_h; } pc{};
            pc.blend_factor = params[0]; pc.jitter_x = params[1]; pc.jitter_y = params[2];
            pc.frame_index = static_cast<int>(params[3]);
            pc.screen_w = params[4]; pc.screen_h = params[5];
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
        } else if (effect_name == "deferred_lighting" && params.size() >= 8) {
            // params: [light_dir.xyz, light_color.xyz, intensity, ambient]
            struct { float lx, ly, lz; float intensity; float cx, cy, cz; float ambient; } pc{};
            pc.lx = params[0]; pc.ly = params[1]; pc.lz = params[2];
            pc.cx = params[3]; pc.cy = params[4]; pc.cz = params[5];
            pc.intensity = params[6];
            pc.ambient = params[7];
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
        } else if (effect_name == "volumetric_fog" && params.size() >= 29) {
            float pc[30];
            pc[0] = static_cast<float>(request.FindTex(2));
            for (int i = 0; i < 29; ++i) pc[i + 1] = params[i];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "decal" && params.size() >= 24) {
            float pc[26];
            pc[0] = static_cast<float>(request.FindTex(2));
            pc[1] = static_cast<float>(request.FindTex(3));
            for (int i = 0; i < 24; ++i) pc[i + 2] = params[i];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "wboit_composite") {
            float pc[1] = { static_cast<float>(request.FindTex(2)) };
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "water" && params.size() >= 39) {
            float pc[40];
            pc[0] = static_cast<float>(request.FindTex(2));
            for (int i = 0; i < 39; ++i) pc[i + 1] = params[i];
            vkCmdPushConstants(cmd_buf, pp_program->pipeline_layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
        } else if (effect_name == "light_shaft" && params.size() >= 11) {
            float pc[16] = {};
            pc[0] = static_cast<float>(request.FindTex(2));
            for (int i = 0; i < 11; ++i) pc[i + 1] = params[i];
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
        current_color_attachment_count_,
        false);  // wireframe not applicable for particles

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
// Hair Strand 渲染 (Vulkan)
// ============================================================================

void VulkanDrawExecutor::DrawHairStrands(
    VkCommandBuffer cmd_buf,
    const std::vector<HairDrawItem>& items,
    const glm::mat4& view,
    const glm::mat4& projection,
    VulkanPipelineStateManager& pipeline_mgr,
    VulkanShaderManager& shader_mgr) {

    if (items.empty()) return;

    // 懒初始化 hair shader program（预编译 SPIR-V）
    if (hair_shader_handle_ == 0) {
        using namespace dse::render::generated_shaders;
        hair_shader_handle_ = shader_mgr.CreateProgramFromSpirv(
            khair_vert_spv, khair_vert_spv_size,
            khair_frag_spv, khair_frag_spv_size);
        if (hair_shader_handle_ == 0) {
            DEBUG_LOG_ERROR("[VulkanDrawExecutor] Failed to compile hair shader for Vulkan");
            return;
        }
    }

    const VulkanShaderProgram* hair_program = shader_mgr.GetProgram(hair_shader_handle_);
    if (!hair_program) return;

    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // 懒创建 LINE_STRIP + alpha blend + depth readonly pipeline
    if (hair_pipeline_ == VK_NULL_HANDLE || hair_pipeline_rp_ != active_rp) {
        // 销毁旧管线
        if (hair_pipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(context_->device(), hair_pipeline_, nullptr);
            hair_pipeline_ = VK_NULL_HANDLE;
        }

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = hair_program->vert_module;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = hair_program->frag_module;
        stages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertex_input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

        VkPipelineInputAssemblyStateCreateInfo input_assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        input_assembly.primitiveRestartEnable = VK_FALSE;

        VkExtent2D extent = context_->swapchain_extent();
        VkViewport viewport{0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
        VkRect2D scissor{{0,0}, extent};
        VkPipelineViewportStateCreateInfo viewport_state{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport_state.viewportCount = 1; viewport_state.pViewports = &viewport;
        viewport_state.scissorCount = 1; viewport_state.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VulkanPipelineStateManager::ToVkFrontFace();
        rasterizer.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = current_msaa_samples_;

        VkPipelineDepthStencilStateCreateInfo depth_stencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth_stencil.depthTestEnable = VK_TRUE;
        depth_stencil.depthWriteEnable = VK_FALSE; // depth readonly
        depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

        VkPipelineColorBlendAttachmentState blend_att{};
        blend_att.colorWriteMask = 0xF;
        blend_att.blendEnable = VK_TRUE;
        blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_att.colorBlendOp = VK_BLEND_OP_ADD;
        blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_att.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1; blend.pAttachments = &blend_att;

        VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic_state{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic_state.dynamicStateCount = 2; dynamic_state.pDynamicStates = dyn_states;

        VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        ci.stageCount = 2; ci.pStages = stages;
        ci.pVertexInputState = &vertex_input;
        ci.pInputAssemblyState = &input_assembly;
        ci.pViewportState = &viewport_state;
        ci.pRasterizationState = &rasterizer;
        ci.pMultisampleState = &multisample;
        ci.pDepthStencilState = &depth_stencil;
        ci.pColorBlendState = &blend;
        ci.pDynamicState = &dynamic_state;
        ci.layout = hair_program->pipeline_layout;
        ci.renderPass = active_rp;
        ci.subpass = 0;

        VkResult result = vkCreateGraphicsPipelines(context_->device(), VK_NULL_HANDLE, 1, &ci, nullptr, &hair_pipeline_);
        if (result != VK_SUCCESS) {
            DEBUG_LOG_ERROR("[VulkanDrawExecutor] Failed to create hair pipeline: {}", static_cast<int>(result));
            hair_pipeline_ = VK_NULL_HANDLE;
            return;
        }
        hair_pipeline_rp_ = active_rp;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, hair_pipeline_);

    // HairUBO 布局 (std140, 10 x vec4/mat4 = 256B)
    struct HairUBO {
        glm::mat4 model;           // 0
        glm::mat4 view;            // 64
        glm::mat4 projection;      // 128
        glm::vec4 camera_pos;      // 192
        glm::vec4 light_dir_int;   // 208
        glm::vec4 light_color_amb; // 224
        glm::vec4 root_color;      // 240
        glm::vec4 tip_color;       // 256
        glm::vec4 spec_params;     // 272
        glm::vec4 spec_color_opa;  // 288
    }; // = 304 bytes

    glm::mat4 inv_view = glm::inverse(view);
    glm::vec3 cam_pos(inv_view[3]);

    for (const auto& item : items) {
        if (item.strand_count == 0 || item.total_vertex_count == 0) continue;
        if (!item.strand_firsts || !item.strand_counts) continue;

        HairUBO ubo{};
        ubo.model = item.world_transform;
        ubo.view = view;
        ubo.projection = projection;
        ubo.camera_pos = glm::vec4(cam_pos, 0.0f);
        ubo.light_dir_int = glm::vec4(item.light_direction, item.light_intensity);
        ubo.light_color_amb = glm::vec4(item.light_color, item.ambient_intensity);
        ubo.root_color = item.root_color;
        ubo.tip_color = item.tip_color;
        ubo.spec_params = glm::vec4(item.specular_primary, item.specular_secondary,
                                     item.specular_strength_primary, item.specular_strength_secondary);
        ubo.spec_color_opa = glm::vec4(item.specular_color, item.opacity);

        // 写入 per-frame UBO slot（复用 VulkanDrawExecutor 的 per_frame_ubo_）
        uint32_t fi = context_->current_frame();
        VkDeviceSize slot_offset = per_frame_ubo_offset_;
        VkDeviceSize aligned_size = (sizeof(HairUBO) + kUboSlotAlignment - 1) & ~(kUboSlotAlignment - 1);

        void* mapped = nullptr;
        vkMapMemory(context_->device(), per_frame_ubo_mem_[fi], slot_offset, sizeof(HairUBO), 0, &mapped);
        memcpy(mapped, &ubo, sizeof(HairUBO));
        vkUnmapMemory(context_->device(), per_frame_ubo_mem_[fi]);
        per_frame_ubo_offset_ += aligned_size;

        // 分配 descriptor set (set 0 = UBO, set 1 = 2 SSBO)
        std::vector<VkDescriptorSet> sets;

        // Set 0: HairUBO
        {
            VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            alloc_info.descriptorPool = resource_mgr_->descriptor_pool();
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &hair_program->descriptor_set_layouts[0];
            VkDescriptorSet ds;
            if (vkAllocateDescriptorSets(context_->device(), &alloc_info, &ds) != VK_SUCCESS) continue;

            VkDescriptorBufferInfo buf_info{};
            buf_info.buffer = per_frame_ubo_[fi];
            buf_info.offset = slot_offset;
            buf_info.range = sizeof(HairUBO);

            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = ds;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.pBufferInfo = &buf_info;
            vkUpdateDescriptorSets(context_->device(), 1, &write, 0, nullptr);
            sets.push_back(ds);
        }

        // Set 1: Position SSBO (binding 0) + Tangent SSBO (binding 1)
        if (hair_program->descriptor_set_layouts.size() > 1) {
            VkDescriptorSetAllocateInfo alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            alloc_info.descriptorPool = resource_mgr_->descriptor_pool();
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &hair_program->descriptor_set_layouts[1];
            VkDescriptorSet ds;
            if (vkAllocateDescriptorSets(context_->device(), &alloc_info, &ds) != VK_SUCCESS) continue;

            const VulkanBuffer* pos_buf = resource_mgr_->GetSSBO(item.position_ssbo.raw());
            const VulkanBuffer* tan_buf = resource_mgr_->GetSSBO(item.tangent_ssbo.raw());
            if (!pos_buf || !tan_buf || !pos_buf->buffer || !tan_buf->buffer) continue;

            VkDescriptorBufferInfo ssbo_infos[2]{};
            ssbo_infos[0].buffer = pos_buf->buffer;
            ssbo_infos[0].offset = 0;
            ssbo_infos[0].range = VK_WHOLE_SIZE;
            ssbo_infos[1].buffer = tan_buf->buffer;
            ssbo_infos[1].offset = 0;
            ssbo_infos[1].range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet writes[2]{};
            for (int i = 0; i < 2; ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = ds;
                writes[i].dstBinding = static_cast<uint32_t>(i);
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                writes[i].pBufferInfo = &ssbo_infos[i];
            }
            vkUpdateDescriptorSets(context_->device(), 2, writes, 0, nullptr);
            sets.push_back(ds);
        }

        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                hair_program->pipeline_layout,
                                0, static_cast<uint32_t>(sets.size()), sets.data(),
                                0, nullptr);

        // 逐 strand 绘制（每个 strand 是一个 LINE_STRIP）
        for (uint32_t s = 0; s < item.strand_count; ++s) {
            int first = item.strand_firsts[s];
            int count = item.strand_counts[s];
            if (count <= 0) continue;
            vkCmdDraw(cmd_buf, static_cast<uint32_t>(count), 1,
                      static_cast<uint32_t>(first), 0);
        }

        global_state_.current_frame_stats.draw_calls += 1;
    }
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
    terrain_params_ubo_offset_ = 0;
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

// ============================================================
// GPU-Driven PBR 渲染设置
// ============================================================

void VulkanDrawExecutor::SetupGPUDrivenPBR(VkCommandBuffer cmd_buf,
                                            const glm::mat4& view, const glm::mat4& proj,
                                            const glm::vec3& camera_pos,
                                            const glm::vec3& light_dir, const glm::vec3& light_color,
                                            float light_intensity, float ambient_intensity,
                                            float shadow_strength,
                                            VulkanPipelineStateManager& pipeline_mgr,
                                            VulkanShaderManager& shader_mgr) {
    if (cmd_buf == VK_NULL_HANDLE || !context_) return;

    // 优先使用 GPU-driven shader（VS 从 SSBO 读 model, FS 从 Material SSBO 读材质）
    unsigned int shader_handle = shader_mgr.gpu_driven_pbr_shader_handle();
    const VulkanShaderProgram* pbr_program = shader_mgr.GetProgram(shader_handle);
    if (!pbr_program) {
        // 回退到标准 PBR（不支持 glslang 时）
        shader_handle = shader_mgr.pbr_shader_handle();
        pbr_program = shader_mgr.GetProgram(shader_handle);
        if (!pbr_program) return;
    }

    // 获取当前 render pass（可能是离屏 RT 的 render pass）
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // BatchVertex 顶点格式（与 DrawMeshBatch 一致）
    std::vector<VkVertexInputBindingDescription> mesh_bindings = {
        {0, sizeof(BatchVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    std::vector<VkVertexInputAttributeDescription> mesh_attrs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},       // aPos
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12},   // aColor
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 28},          // aTexCoord
        {3, 0, VK_FORMAT_R32G32B32_SFLOAT, 36},       // aNormal
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 48},       // aTangent
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 60},   // aBoneWeights
        {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 76},   // aBoneIndices
    };

    VkPipeline vk_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        pbr_program, active_rp, mesh_bindings, mesh_attrs,
        context_->swapchain_extent(), current_msaa_samples_,
        current_color_attachment_count_,
        global_state_.wireframe_mode);
    if (vk_pipeline == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
    gpu_driven_pipeline_layout_ = pbr_program->pipeline_layout;

    // 满足 pipeline layout 中的 push constant range 要求，防止 validation warning
    if (pbr_program->reflection.has_push_constant) {
        static const uint8_t kZeroPushConstants[256] = {};
        uint32_t pc_size = std::min(pbr_program->reflection.push_constant_range.size, uint32_t(256));
        vkCmdPushConstants(cmd_buf, pbr_program->pipeline_layout,
                           pbr_program->reflection.push_constant_range.stageFlags,
                           0, pc_size, kZeroPushConstants);
    }

    // PerFrame UBO
    VkDeviceSize cur_per_frame_offset = per_frame_ubo_offset_;
    VulkanPerFrameUBO frame_ubo{};
    frame_ubo.vp = proj * view;
    frame_ubo.view = view;
    frame_ubo.camera_pos = glm::vec4(camera_pos, 0.0f);
    if (per_frame_ubo_offset_ + sizeof(VulkanPerFrameUBO) > per_frame_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] GPU_DRIVEN PER_FRAME_UBO OVERFLOW: offset={} capacity={}", per_frame_ubo_offset_, per_frame_ubo_capacity_);
    } else {
        WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                      per_frame_ubo_offset_, sizeof(VulkanPerFrameUBO), &frame_ubo);
    }
    per_frame_ubo_offset_ += kUboSlotAlignment;

    // PerScene UBO
    VkDeviceSize cur_per_scene_offset = per_scene_ubo_offset_;
    VulkanPerSceneUBO scene_ubo{};
    const float gpu_driven_light = global_state_.force_unlit ? 0.0f : 1.0f;
    scene_ubo.light_dir_and_enabled   = glm::vec4(light_dir, gpu_driven_light);
    scene_ubo.light_color_and_ambient = glm::vec4(light_color, ambient_intensity);
    const float receive_shadow = (shadow_strength > 0.0f) ? 1.0f : 0.0f;
    scene_ubo.light_params            = glm::vec4(light_intensity, shadow_strength, receive_shadow, 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene_ubo.light_space_matrices[i] = global_state_.light_space_matrix[i];
    }
    if (per_scene_ubo_offset_ + sizeof(VulkanPerSceneUBO) > per_scene_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] GPU_DRIVEN PER_SCENE_UBO OVERFLOW: offset={} capacity={}", per_scene_ubo_offset_, per_scene_ubo_capacity_);
    } else {
        WriteToBuffer(context_->device(), per_scene_ubo_mem_[current_frame_index_],
                      per_scene_ubo_offset_, sizeof(VulkanPerSceneUBO), &scene_ubo);
    }
    per_scene_ubo_offset_ += kUboSlotAlignment;

    if (resource_mgr_) {
        cached_gpu_driven_program_ = pbr_program;
        gpu_driven_instance_set_bound_ = false;

        if (pbr_program->descriptor_set_layouts.size() < 4) {
            DEBUG_LOG_WARN("GPU-driven PBR program has insufficient descriptor set layouts ({})",
                           pbr_program->descriptor_set_layouts.size());
            return;
        }

        auto device = context_->device();
        VkDescriptorSet set0 = resource_mgr_->AllocateDescriptorSet(pbr_program->descriptor_set_layouts[0]);
        VkDescriptorSet set1 = resource_mgr_->AllocateDescriptorSet(pbr_program->descriptor_set_layouts[1]);
        VkDescriptorSet set3 = resource_mgr_->AllocateDescriptorSet(pbr_program->descriptor_set_layouts[3]);
        if (set0 == VK_NULL_HANDLE || set1 == VK_NULL_HANDLE || set3 == VK_NULL_HANDLE) return;

        VkDescriptorBufferInfo frame_buf{};
        frame_buf.buffer = per_frame_ubo_[current_frame_index_];
        frame_buf.offset = cur_per_frame_offset;
        frame_buf.range = sizeof(VulkanPerFrameUBO);
        VkWriteDescriptorSet frame_write{};
        frame_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        frame_write.dstSet = set0;
        frame_write.dstBinding = 0;
        frame_write.descriptorCount = 1;
        frame_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        frame_write.pBufferInfo = &frame_buf;
        vkUpdateDescriptorSets(device, 1, &frame_write, 0, nullptr);

        VkDescriptorBufferInfo scene_buf{};
        scene_buf.buffer = per_scene_ubo_[current_frame_index_];
        scene_buf.offset = cur_per_scene_offset;
        scene_buf.range = sizeof(VulkanPerSceneUBO);
        VkWriteDescriptorSet scene_write{};
        scene_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        scene_write.dstSet = set1;
        scene_write.dstBinding = 0;
        scene_write.descriptorCount = 1;
        scene_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        scene_write.pBufferInfo = &scene_buf;
        vkUpdateDescriptorSets(device, 1, &scene_write, 0, nullptr);

        auto write_set1_ssbo = [&](uint32_t binding) {
            VkDescriptorBufferInfo ssbo_buf{};
            auto it = bound_ssbos_.find(binding);
            const VulkanBuffer* ssbo = (it != bound_ssbos_.end()) ? resource_mgr_->GetSSBO(it->second) : nullptr;
            if (ssbo && ssbo->buffer != VK_NULL_HANDLE) {
                ssbo_buf.buffer = ssbo->buffer;
                ssbo_buf.offset = 0;
                ssbo_buf.range = ssbo->size;
            } else {
                ssbo_buf.buffer = dummy_ssbo_buffer_;
                ssbo_buf.offset = 0;
                ssbo_buf.range = VK_WHOLE_SIZE;
            }
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set1;
            write.dstBinding = binding;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &ssbo_buf;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        };
        write_set1_ssbo(1);
        write_set1_ssbo(2);
        write_set1_ssbo(3);
        write_set1_ssbo(4);

        struct LightProbeGPU {
            glm::vec4 sh[9];
            glm::vec4 params;
        } lp_data{};
        for (int i = 0; i < 9; ++i) lp_data.sh[i] = global_state_.light_probe_sh[i];
        lp_data.params = glm::vec4(global_state_.light_probe_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
        WriteToBuffer(device, light_probe_ubo_mem_[current_frame_index_], 0, sizeof(lp_data), &lp_data);
        VkDescriptorBufferInfo lp_buf{};
        lp_buf.buffer = light_probe_ubo_[current_frame_index_];
        lp_buf.offset = 0;
        lp_buf.range = sizeof(lp_data);
        VkWriteDescriptorSet lp_write{};
        lp_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lp_write.dstSet = set1;
        lp_write.dstBinding = 5;
        lp_write.descriptorCount = 1;
        lp_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lp_write.pBufferInfo = &lp_buf;
        vkUpdateDescriptorSets(device, 1, &lp_write, 0, nullptr);

        VkDescriptorImageInfo point_shadow_infos[4] = {};
        const VulkanTexture* white_tex = resource_mgr_->GetTexture(white_cubemap_handle_);
        for (int i = 0; i < 4; ++i) {
            unsigned int ps_handle = global_state_.point_shadow_map[i];
            const VulkanTexture* ps_tex = (ps_handle != 0) ? resource_mgr_->GetTexture(ps_handle) : nullptr;
            const VulkanTexture* tex = ps_tex ? ps_tex : white_tex;
            if (tex) {
                point_shadow_infos[i].sampler = resource_mgr_->default_sampler();
                point_shadow_infos[i].imageView = tex->image_view;
                point_shadow_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
        }
        VkWriteDescriptorSet point_shadow_write{};
        point_shadow_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        point_shadow_write.dstSet = set3;
        point_shadow_write.dstBinding = 0;
        point_shadow_write.descriptorCount = 4;
        point_shadow_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        point_shadow_write.pImageInfo = point_shadow_infos;
        vkUpdateDescriptorSets(device, 1, &point_shadow_write, 0, nullptr);

        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pbr_program->pipeline_layout, 0, 1, &set0, 0, nullptr);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pbr_program->pipeline_layout, 1, 1, &set1, 0, nullptr);
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pbr_program->pipeline_layout, 3, 1, &set3, 0, nullptr);
    }
}

// ============================================================
// GPU-Driven Shadow 渲染设置
// ============================================================

void VulkanDrawExecutor::SetupGPUDrivenShadow(VkCommandBuffer cmd_buf,
                                                const glm::mat4& light_view, const glm::mat4& light_proj,
                                                VulkanPipelineStateManager& pipeline_mgr,
                                                VulkanShaderManager& shader_mgr) {
    if (cmd_buf == VK_NULL_HANDLE || !context_) return;

    // 优先使用 GPU-driven shadow shader（VS 从 SSBO 读 model）
    unsigned int shader_handle = shader_mgr.gpu_driven_shadow_shader_handle();
    const VulkanShaderProgram* shadow_program = shader_mgr.GetProgram(shader_handle);
    if (!shadow_program) {
        shader_handle = shader_mgr.shadow_shader_handle();
        shadow_program = shader_mgr.GetProgram(shader_handle);
        if (!shadow_program) return;
    }

    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    std::vector<VkVertexInputBindingDescription> mesh_bindings = {
        {0, sizeof(BatchVertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };
    std::vector<VkVertexInputAttributeDescription> mesh_attrs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},       // aPos
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 12},   // aColor
        {2, 0, VK_FORMAT_R32G32_SFLOAT, 28},          // aTexCoord
        {3, 0, VK_FORMAT_R32G32B32_SFLOAT, 36},       // aNormal
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, 48},       // aTangent
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 60},   // aBoneWeights
        {6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 76},   // aBoneIndices
    };

    VkPipeline vk_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(),
        shadow_program, active_rp, mesh_bindings, mesh_attrs,
        context_->swapchain_extent(), current_msaa_samples_,
        current_color_attachment_count_);
    if (vk_pipeline == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
    gpu_driven_pipeline_layout_ = shadow_program->pipeline_layout;
    cached_gpu_driven_program_ = shadow_program;
    gpu_driven_instance_set_bound_ = false;

    // 满足 pipeline layout 中的 push constant range 要求，防止 validation warning
    if (shadow_program->reflection.has_push_constant) {
        static const uint8_t kZeroPushConstants[256] = {};
        uint32_t pc_size = std::min(shadow_program->reflection.push_constant_range.size, uint32_t(256));
        vkCmdPushConstants(cmd_buf, shadow_program->pipeline_layout,
                           shadow_program->reflection.push_constant_range.stageFlags,
                           0, pc_size, kZeroPushConstants);
    }

    VkDeviceSize cur_per_frame_offset = per_frame_ubo_offset_;
    VulkanPerFrameUBO frame_ubo{};
    frame_ubo.vp = light_proj * light_view;
    frame_ubo.view = light_view;
    frame_ubo.camera_pos = glm::vec4(0.0f);
    if (per_frame_ubo_offset_ + sizeof(VulkanPerFrameUBO) > per_frame_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] GPU_DRIVEN_SHADOW PER_FRAME_UBO OVERFLOW: offset={} capacity={}", per_frame_ubo_offset_, per_frame_ubo_capacity_);
    } else {
        WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                      per_frame_ubo_offset_, sizeof(VulkanPerFrameUBO), &frame_ubo);
    }
    per_frame_ubo_offset_ += kUboSlotAlignment;

    if (resource_mgr_ && !shadow_program->descriptor_set_layouts.empty()) {
        VkDescriptorSet set0 = resource_mgr_->AllocateDescriptorSet(shadow_program->descriptor_set_layouts[0]);
        if (set0 != VK_NULL_HANDLE) {
            VkDescriptorBufferInfo frame_buf{};
            frame_buf.buffer = per_frame_ubo_[current_frame_index_];
            frame_buf.offset = cur_per_frame_offset;
            frame_buf.range = sizeof(VulkanPerFrameUBO);
            VkWriteDescriptorSet frame_write{};
            frame_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            frame_write.dstSet = set0;
            frame_write.dstBinding = 0;
            frame_write.descriptorCount = 1;
            frame_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            frame_write.pBufferInfo = &frame_buf;
            vkUpdateDescriptorSets(context_->device(), 1, &frame_write, 0, nullptr);
            vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    shadow_program->pipeline_layout, 0, 1, &set0, 0, nullptr);
        }

        // 为 sets 1-3 绑定当前帧空 descriptor sets，防止与前一个 pipeline layout 不兼容导致 TDR
        for (uint32_t si = 1; si < shadow_program->descriptor_set_layouts.size() && si < 4; ++si) {
            VkDescriptorSet empty_set = resource_mgr_->AllocateDescriptorSet(
                shadow_program->descriptor_set_layouts[si]);
            if (empty_set != VK_NULL_HANDLE) {
                vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        shadow_program->pipeline_layout, si, 1,
                                        &empty_set, 0, nullptr);
            }
        }
    }
}

// ============================================================
// GPU-Driven: 按纹理桶重新绑定 Set 2
// ============================================================

void VulkanDrawExecutor::UpdateGPUDrivenMaterial(const void* mat_data) {
    // VK GPU-driven FS 从 MaterialSSBO 逐 instance 读材质（v_material_id 索引），
    // 不再需要 per-bucket UBO 更新。保留接口供 DX11 使用。
    (void)mat_data;
}

void VulkanDrawExecutor::BindGPUDrivenInstanceSet(VkCommandBuffer cmd_buf,
                                                   VulkanResourceManager& resource_mgr) {
    if (cmd_buf == VK_NULL_HANDLE || !cached_gpu_driven_program_) return;
    if (gpu_driven_pipeline_layout_ == VK_NULL_HANDLE) return;
    if (gpu_driven_instance_set_bound_) return;
    if (cached_gpu_driven_program_->descriptor_set_layouts.size() <= 4) return;

    auto inst_it = bound_ssbos_.find(gpu_driven::kSSBOBindingInstances);
    if (inst_it == bound_ssbos_.end()) return;
    const VulkanBuffer* inst_ssbo = resource_mgr.GetSSBO(inst_it->second);
    if (!inst_ssbo || inst_ssbo->buffer == VK_NULL_HANDLE) return;

    VkDescriptorSet set4 = resource_mgr.AllocateDescriptorSet(
        cached_gpu_driven_program_->descriptor_set_layouts[4]);
    if (set4 == VK_NULL_HANDLE) return;

    VkDescriptorBufferInfo inst_buf{};
    inst_buf.buffer = inst_ssbo->buffer;
    inst_buf.offset = 0;
    inst_buf.range  = inst_ssbo->size;
    VkWriteDescriptorSet inst_write{};
    inst_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inst_write.dstSet          = set4;
    inst_write.dstBinding      = 0;
    inst_write.descriptorCount = 1;
    inst_write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inst_write.pBufferInfo     = &inst_buf;
    vkUpdateDescriptorSets(context_->device(), 1, &inst_write, 0, nullptr);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            gpu_driven_pipeline_layout_, 4, 1, &set4, 0, nullptr);
    gpu_driven_instance_set_bound_ = true;
}

void VulkanDrawExecutor::BindGPUDrivenTextures(VkCommandBuffer cmd_buf,
                                                unsigned int albedo, unsigned int normal,
                                                unsigned int metallic_roughness,
                                                unsigned int emissive, unsigned int occlusion,
                                                VulkanResourceManager& resource_mgr) {
    if (cmd_buf == VK_NULL_HANDLE || !cached_gpu_driven_program_) return;
    if (cached_gpu_driven_program_->descriptor_set_layouts.size() < 3) return;

    auto device = context_->device();
    VkDescriptorSet set2 = resource_mgr.AllocateDescriptorSet(
        cached_gpu_driven_program_->descriptor_set_layouts[2]);
    if (set2 == VK_NULL_HANDLE) return;

    auto has_binding = [&](uint32_t binding, VkDescriptorType type) {
        for (const auto& b : cached_gpu_driven_program_->reflection.bindings) {
            if (b.set == 2 && b.binding == binding && b.type == type) return true;
        }
        return false;
    };

    const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
    const VulkanTexture* white_cube = resource_mgr.GetTexture(white_cubemap_handle_);
    VkWriteDescriptorSet writes[24] = {};
    VkDescriptorImageInfo image_infos[24] = {};
    VkDescriptorBufferInfo buffer_infos[8] = {};
    int wc = 0;
    int ic = 0;
    int bc = 0;

    auto push_image = [&](uint32_t binding, const VulkanTexture* tex, VkSampler sampler,
                          VkImageLayout layout, uint32_t count = 1) {
        if (!has_binding(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) || !tex) return;
        image_infos[ic].sampler = sampler;
        image_infos[ic].imageView = tex->image_view;
        image_infos[ic].imageLayout = layout;
        writes[wc].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[wc].dstSet = set2;
        writes[wc].dstBinding = binding;
        writes[wc].descriptorCount = count;
        writes[wc].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[wc].pImageInfo = &image_infos[ic];
        ++wc;
        ic += static_cast<int>(count);
    };
    auto push_buffer = [&](uint32_t binding, VkDescriptorType type,
                           VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range) {
        if (!has_binding(binding, type) || buffer == VK_NULL_HANDLE) return;
        buffer_infos[bc].buffer = buffer;
        buffer_infos[bc].offset = offset;
        buffer_infos[bc].range = range;
        writes[wc].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[wc].dstSet = set2;
        writes[wc].dstBinding = binding;
        writes[wc].descriptorCount = 1;
        writes[wc].descriptorType = type;
        writes[wc].pBufferInfo = &buffer_infos[bc];
        ++wc;
        ++bc;
    };

    struct TexBinding { unsigned int handle; uint32_t binding; };
    TexBinding tex_bindings[] = {
        {albedo, 1}, {normal, 2}, {metallic_roughness, 3},
        {emissive, 4}, {occlusion, 5},
    };
    for (const auto& tb : tex_bindings) {
        unsigned int h = tb.handle != 0 ? tb.handle : white_texture_handle_;
        const VulkanTexture* tex = resource_mgr.GetTexture(h);
        push_image(tb.binding, tex ? tex : white_tex, resource_mgr.default_sampler(),
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    VkDescriptorImageInfo csm_infos[3] = {};
    if (has_binding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
        for (int i = 0; i < 3; ++i) {
            unsigned int sm_handle = global_state_.shadow_map[i];
            VkImageView depth_view = sm_handle != 0 ? resource_mgr.GetRenderTargetDepthImageView(sm_handle) : VK_NULL_HANDLE;
            csm_infos[i].sampler = depth_view != VK_NULL_HANDLE ? resource_mgr.shadow_comparison_sampler() : resource_mgr.default_sampler();
            csm_infos[i].imageView = depth_view != VK_NULL_HANDLE ? depth_view : (white_tex ? white_tex->image_view : VK_NULL_HANDLE);
            csm_infos[i].imageLayout = depth_view != VK_NULL_HANDLE ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        writes[wc].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[wc].dstSet = set2;
        writes[wc].dstBinding = 6;
        writes[wc].descriptorCount = 3;
        writes[wc].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[wc].pImageInfo = csm_infos;
        ++wc;
    }

    VkDescriptorImageInfo spot_infos[4] = {};
    if (has_binding(7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
        for (int i = 0; i < 4; ++i) {
            unsigned int ss_handle = global_state_.spot_shadow_map[i];
            VkImageView depth_view = ss_handle != 0 ? resource_mgr.GetRenderTargetDepthImageView(ss_handle) : VK_NULL_HANDLE;
            spot_infos[i].sampler = depth_view != VK_NULL_HANDLE ? resource_mgr.shadow_comparison_sampler() : resource_mgr.default_sampler();
            spot_infos[i].imageView = depth_view != VK_NULL_HANDLE ? depth_view : (white_tex ? white_tex->image_view : VK_NULL_HANDLE);
            spot_infos[i].imageLayout = depth_view != VK_NULL_HANDLE ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        writes[wc].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[wc].dstSet = set2;
        writes[wc].dstBinding = 7;
        writes[wc].descriptorCount = 4;
        writes[wc].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[wc].pImageInfo = spot_infos;
        ++wc;
    }

    push_buffer(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bone_matrices_ubo_, 0, VK_WHOLE_SIZE);

    VkBuffer mat_buffer = dummy_ssbo_buffer_;
    VkDeviceSize mat_range = VK_WHOLE_SIZE;
    auto mat_it = bound_ssbos_.find(gpu_driven::kSSBOBindingMaterials);
    if (mat_it != bound_ssbos_.end()) {
        const VulkanBuffer* mat_ssbo = resource_mgr.GetSSBO(mat_it->second);
        if (mat_ssbo && mat_ssbo->buffer != VK_NULL_HANDLE) {
            mat_buffer = mat_ssbo->buffer;
            mat_range = mat_ssbo->size;
        }
    }
    push_buffer(9, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, mat_buffer, 0, mat_range);

    SpotLightDataUBO spot_data = PrepareSpotLightDataUBO(global_state_);
    WriteToBuffer(device, per_spot_lights_ubo_mem_[current_frame_index_], 0, sizeof(spot_data), &spot_data);
    push_buffer(19, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, per_spot_lights_ubo_[current_frame_index_],
                0, sizeof(SpotLightDataUBO));

    for (uint32_t binding = 11; binding <= 15; ++binding) {
        push_image(binding, white_tex, resource_mgr.default_sampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    push_buffer(16, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, dummy_ubo_buffer_, 0, 32);
    push_image(17, white_cube, resource_mgr.default_sampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    push_image(18, white_tex, resource_mgr.default_sampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (wc > 0) {
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(wc), writes, 0, nullptr);
    }
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                             gpu_driven_pipeline_layout_, 2, 1, &set2, 0, nullptr);
    BindGPUDrivenInstanceSet(cmd_buf, resource_mgr);
}

} // namespace render
} // namespace dse

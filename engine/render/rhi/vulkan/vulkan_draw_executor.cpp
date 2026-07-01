/**
 * @file vulkan_draw_executor.cpp
 * @brief Vulkan ç»˜åˆ¶æ‰§è¡Œå™¨å®žçŽ°
 *
 * å®žçŽ°å®Œæ•´çš„ Vulkan å‘½ä»¤ç¼“å†²å½•åˆ¶é€»è¾‘ï¼š
 * - å‡ ä½•ç¼“å†²åŒºåˆå§‹åŒ–ï¼ˆVBO/IBO åˆ›å»ºä¸Žæ•°æ®ä¸Šä¼ ï¼‰
 * - UBO ç¼“å†²åŒºç®¡ç†ï¼ˆPerFrame/PerScene/PerMaterial åŒç¼“å†²ï¼‰
 * - DescriptorSet åˆ†é…/æ›´æ–°/ç»‘å®š
 * - DrawMeshBatchï¼š3D PBR ç½‘æ ¼ç»˜åˆ¶ï¼ˆpush constant + descriptor setï¼‰
 * - DrawParticles3Dï¼š3D ç²’å­ billboard
 */

#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace dse {
namespace render {

// å‡ ä½•ç¼“å†²åŒºå®¹é‡å¸¸é‡ï¼ˆä¸Ž GL ç‰ˆæœ¬å¯¹é½ï¼‰
constexpr size_t MAX_SPRITES = 10000;
constexpr size_t MAX_SPRITE_VERTICES = MAX_SPRITES * 4;
constexpr size_t MAX_SPRITE_INDICES = MAX_SPRITES * 6;
constexpr size_t MAX_MESH_VERTICES = 131072;
constexpr size_t MAX_MESH_INDICES = 262144;
static const unsigned int kSdfVariantKey = static_cast<unsigned int>(std::hash<std::string>{}("TEXT_SDF"));

// ============================================================================
// è¾…åŠ©ï¼šåˆ›å»º VkBuffer + VkDeviceMemory
// ============================================================================

namespace {

/// åˆ›å»º Vulkan ç¼“å†²åŒºï¼ˆhost-visibleï¼Œç”¨äºŽåŠ¨æ€æ›´æ–°ï¼‰
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

    // æŸ¥æ‰¾åˆé€‚çš„å†…å­˜ç±»åž‹
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

/// æŒä¹…æ˜ å°„ç¼“å­˜ï¼šé¿å…æ¯æ¬¡ WriteToBuffer éƒ½ vkMapMemory/vkUnmapMemory
/// HOST_VISIBLE + HOST_COHERENT å†…å­˜å¯ä»¥ map ä¸€æ¬¡åŽæ°¸ä¹…ä½¿ç”¨
} // anonymous namespace

static std::unordered_map<VkDeviceMemory, void*> g_persistent_map_cache;

/// å°†æ•°æ®å†™å…¥ host-visible ç¼“å†²åŒºï¼ˆä½¿ç”¨æŒä¹…æ˜ å°„ï¼‰
void WriteToBuffer(VkDevice device, VkDeviceMemory memory,
                   VkDeviceSize offset, VkDeviceSize size, const void* data) {
    auto it = g_persistent_map_cache.find(memory);
    if (it != g_persistent_map_cache.end()) {
        memcpy(static_cast<char*>(it->second) + offset, data, static_cast<size_t>(size));
        return;
    }
    // é¦–æ¬¡è®¿é—®ï¼šå…¨é‡æ˜ å°„å¹¶ç¼“å­˜æŒ‡é’ˆ
    void* mapped = nullptr;
    VkResult r = vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (r != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] WriteToBuffer: vkMapMemory FAILED offset={} size={} result={}", offset, size, (int)r);
        return;
    }
    g_persistent_map_cache[memory] = mapped;
    memcpy(static_cast<char*>(mapped) + offset, data, static_cast<size_t>(size));
}

/// é‡Šæ”¾æŒä¹…æ˜ å°„ï¼ˆbuffer é”€æ¯å‰è°ƒç”¨ï¼‰
void UnmapPersistentBuffer(VkDevice device, VkDeviceMemory memory) {
    auto it = g_persistent_map_cache.find(memory);
    if (it != g_persistent_map_cache.end()) {
        vkUnmapMemory(device, memory);
        g_persistent_map_cache.erase(it);
    }
}

namespace {

/// åˆ›å»º UBO ç¼“å†²åŒºè¾…åŠ©ï¼ˆUBO ç”¨é€” + host-visible + coherentï¼‰
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

    // --- ç²¾çµæ‰¹å¤„ç† VBO/IBO ---
    // é¡¶ç‚¹æ ¼å¼ï¼švec2 pos, vec2 texcoord, vec4 color = 8 floats * 4 = 32 bytes
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

    // é¢„å¡«å…… sprite IBOï¼ˆquad ç´¢å¼•ï¼‰
    {
        std::vector<uint16_t> indices(MAX_SPRITE_INDICES);
        for (size_t i = 0; i < MAX_SPRITES; ++i) {
            uint16_t base = static_cast<uint16_t>(i * 4);
            indices[i * 6 + 0] = base + 0;
            indices[i * 6 + 1] = base + 1;
            indices[i * 6 + 2] = base + 2;
            indices[i * 6 + 3] = base + 0;
            indices[i * 6 + 4] = base + 2;
            indices[i * 6 + 5] = base + 3;
        }
        WriteToBuffer(device, sprite_ibo_mem_, 0,
                      indices.size() * sizeof(uint16_t), indices.data());
    }

    // --- VFX UBO ring buffer (256B aligned Ã— 64 slots = 16KB, for ui_effects PS params) ---
    CreateVulkanBuffer(device, physical_device, 256 * 64,
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       vfx_ubo_, vfx_ubo_mem_);

    // --- 3D ç½‘æ ¼ VBO/IBO ---
    // å¤š pass æ¯å¸§ç´¯ç§¯å†™å…¥ï¼Œéœ€è¦è¶³å¤Ÿå¤§çš„ç¼“å†²åŒº
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

    // --- å¤©ç©ºç›’ VBO ---
    // 36 é¡¶ç‚¹ * vec3 = 36 * 12 = 432 bytes
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

    // --- åŽå¤„ç†å…¨å±å››è¾¹å½¢ VBO ---
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

    // --- UBO ç¼“å†²åŒºï¼ˆåŒç¼“å†²ï¼Œæ¯ä¸ªç¼“å†²åŒºæ‰©å¤§åˆ°å¤š slotï¼Œé¿å… GPU å»¶è¿Ÿæ‰§è¡Œæ—¶è¦†ç›–ï¼‰ ---
    // per_frame:  128 batches/frame Ã— 256B = 32KBï¼ˆå« shadow passes + GPU Driven setupï¼‰
    // per_object: æ¯ä¸ª draw item å  1 slotï¼›é‡å®žä¾‹åŒ–/å¤š shadow pass åœºæ™¯ä¸‹å•å¸§å¯è¾¾æ•°åƒ draw
    //             ï¼ˆå®žæµ‹ 3d_instancing å³°å€¼ ~2165 slotï¼‰ï¼Œæ•… per_scene/material/terrain å– 4096 slot Ã—256B = 1MBã€‚
    // lights:     512 slotï¼Œä½†æ¯ slot ä¸º kLightUboSlotAlignment(4352B)ï¼Œå•ç‹¬ä¿ç•™ä»¥å…æµªè´¹æ˜¾å­˜ã€‚
    constexpr size_t kPerFrameSlots  = 128;
    constexpr size_t kPerObjectSlots = 4096;   // per-scene / per-material / terrainï¼ˆæ¯ draw 1 slotï¼‰
    constexpr size_t kLightSlots     = 512;    // ç‚¹/èšå…‰ç¯ UBOï¼ˆå¤§å¯¹é½ï¼Œå•ç‹¬é™é¢ï¼‰
    constexpr size_t kSlotAlign      = kUboSlotAlignment;
    per_frame_ubo_capacity_ = kPerFrameSlots * kSlotAlign;
    per_scene_ubo_capacity_ = kPerObjectSlots * kSlotAlign;
    per_material_ubo_capacity_ = kPerObjectSlots * kSlotAlign;
    terrain_params_ubo_capacity_ = kPerObjectSlots * kSlotAlign;
    for (int i = 0; i < MAX_FRAMES; ++i) {
        CreateUBOBufferInternal(device, physical_device, kPerFrameSlots * kSlotAlign,
                                per_frame_ubo_[i], per_frame_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerObjectSlots * kSlotAlign,
                                per_scene_ubo_[i], per_scene_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerObjectSlots * kSlotAlign,
                                per_material_ubo_[i], per_material_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kPerObjectSlots * kSlotAlign,
                                terrain_params_ubo_[i], terrain_params_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kLightSlots * kLightUboSlotAlignment,
                                per_point_lights_ubo_[i], per_point_lights_ubo_mem_[i]);
        CreateUBOBufferInternal(device, physical_device, kLightSlots * kLightUboSlotAlignment,
                                per_spot_lights_ubo_[i], per_spot_lights_ubo_mem_[i]);
    }

    // --- BoneMatrices SSBO / MorphWeights UBO ---
    constexpr size_t kBoneMatricesSize = 64 * 255 * sizeof(glm::mat4); // 64 meshes * 16320 bytes = ~1020KB
    constexpr size_t kMorphWeightsSize = 16; // 4 floats
    // BoneMatrices ä½¿ç”¨ STORAGE_BUFFERï¼ˆSSBOï¼‰+ UNIFORM_BUFFER åŒç”¨é€”
    CreateVulkanBuffer(device, physical_device, kBoneMatricesSize,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       bone_matrices_ubo_, bone_matrices_ubo_mem_);
    CreateUBOBufferInternal(device, physical_device, kMorphWeightsSize,
                            morph_weights_ubo_, morph_weights_ubo_mem_);
    // åˆå§‹åŒ– BoneMatrices ä¸ºå•ä½çŸ©é˜µ
    {
        std::vector<glm::mat4> identity_bones(64 * 255, glm::mat4(1.0f));
        WriteToBuffer(device, bone_matrices_ubo_mem_, 0, kBoneMatricesSize, identity_bones.data());
    }
    // MorphWeights åˆå§‹åŒ–ä¸º 0
    {
        float zero_weights[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        WriteToBuffer(device, morph_weights_ubo_mem_, 0, kMorphWeightsSize, zero_weights);
    }

    // --- LightProbeData UBOï¼ˆåŒç¼“å†²ï¼Œæ¯å¸§ä¸€ä»½ï¼‰ ---
    constexpr size_t kLightProbeSize = sizeof(glm::vec4) * 10; // 9 SH + 1 params = 160B
    for (int i = 0; i < MAX_FRAMES; ++i) {
        CreateUBOBufferInternal(device, physical_device, kLightProbeSize,
                                light_probe_ubo_[i], light_probe_ubo_mem_[i]);
        // åˆå§‹åŒ–ä¸ºé›¶ï¼ˆprobe_params.x = 0 = disabledï¼‰
        glm::vec4 zero_lp[10] = {};
        WriteToBuffer(device, light_probe_ubo_mem_[i], 0, kLightProbeSize, zero_lp);
    }

    // --- GPU Instancing VBOï¼ˆåˆå§‹ 256 å®žä¾‹ = 16KBï¼‰---
    {
        constexpr size_t kInitialInstanceCapacity = 256;
        instance_vbo_capacity_ = kInitialInstanceCapacity * sizeof(glm::mat4);
        CreateVulkanBuffer(device, physical_device, instance_vbo_capacity_,
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           instance_vbo_, instance_vbo_mem_);
    }

    // --- ç™½è‰²çº¹ç† ---
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

    // --- Dummy SSBO å ä½ buffer ---
    // VUID-VkWriteDescriptorSet-descriptorType-00331: SSBO descriptor å†™å…¥è¦æ±‚
    // å¯¹åº” buffer å¿…é¡»æœ‰ VK_BUFFER_USAGE_STORAGE_BUFFER_BITã€‚ä¸èƒ½å¤ç”¨ UBO å ä½ã€‚
    CreateVulkanBuffer(device, physical_device, 64,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       dummy_ssbo_buffer_, dummy_ssbo_buffer_mem_);
    {
        uint8_t zeros[64] = {};
        WriteToBuffer(device, dummy_ssbo_buffer_mem_, 0, sizeof(zeros), zeros);
    }

    // --- Dummy 3D çº¹ç†ï¼ˆ1x1x1ï¼Œç”¨äºŽ sampler3D å ä½ï¼Œå¦‚åŽå¤„ç† LUTï¼‰---
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
                // è½¬æ¢åˆ° SHADER_READ_ONLY_OPTIMAL
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
                // åˆ›å»º 3D image view
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
    destroy_buffer(vfx_ubo_, vfx_ubo_mem_);
    destroy_buffer(mesh_vbo_, mesh_vbo_mem_);
    destroy_buffer(mesh_ibo_, mesh_ibo_mem_);
    destroy_buffer(instance_vbo_, instance_vbo_mem_);
    instance_vbo_capacity_ = 0;
    destroy_buffer(skybox_vbo_, skybox_vbo_mem_);
    destroy_buffer(pp_vbo_, pp_vbo_mem_);

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

    DEBUG_LOG_INFO("VulkanDrawExecutor geometry buffers destroyed");
}

// ============================================================================
// UBO æ›´æ–°
// ============================================================================

void VulkanDrawExecutor::UpdatePerFrameUBO(
    const glm::mat4& view, const glm::mat4& projection,
    const std::unordered_map<std::string, glm::mat4>& pending_mat4) {

    VulkanPerFrameUBO ubo{};
    ubo.vp = projection * view;
    ubo.view = view;

    // ä»Ž view çŸ©é˜µçš„é€†çŸ©é˜µæå–ç›¸æœºä¸–ç•Œä½ç½®ï¼ˆä¸Ž OpenGL ä¸€è‡´ï¼‰
    glm::mat4 inv_view = glm::inverse(view);
    ubo.camera_pos = glm::vec4(inv_view[3][0], inv_view[3][1], inv_view[3][2], global_state_.global_wetness);
    ubo.foliage_wind = global_state_.foliage_wind;
    ubo.foliage_push = global_state_.foliage_push;

    if (per_frame_ubo_offset_ + sizeof(VulkanPerFrameUBO) > per_frame_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] PER_FRAME_UBO OVERFLOW: offset={} capacity={}", per_frame_ubo_offset_, per_frame_ubo_capacity_);
        return;
    }
    WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                  per_frame_ubo_offset_, sizeof(VulkanPerFrameUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerSceneUBO(const MeshDrawItem& item) {
    VulkanPerSceneUBO ubo = PreparePerSceneUBO(item, global_state_);

    if (per_scene_ubo_offset_ + sizeof(VulkanPerSceneUBO) > per_scene_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] PER_SCENE_UBO OVERFLOW: offset={} capacity={}", per_scene_ubo_offset_, per_scene_ubo_capacity_);
        return;
    }
    WriteToBuffer(context_->device(), per_scene_ubo_mem_[current_frame_index_],
                  per_scene_ubo_offset_, sizeof(VulkanPerSceneUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePerMaterialUBO(const MeshDrawItem& item) {
    VulkanPerMaterialUBO ubo = PreparePerMaterialUBO(item, global_state_);

    if (per_material_ubo_offset_ + sizeof(VulkanPerMaterialUBO) > per_material_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] PER_MATERIAL_UBO OVERFLOW: offset={} capacity={}", per_material_ubo_offset_, per_material_ubo_capacity_);
        return;
    }
    WriteToBuffer(context_->device(), per_material_ubo_mem_[current_frame_index_],
                  per_material_ubo_offset_, sizeof(VulkanPerMaterialUBO), &ubo);
}

void VulkanDrawExecutor::UpdatePointSpotLightUBOs(const MeshDrawItem& item) {
    uint32_t fi = current_frame_index_;

    VulkanPointLightsUBO pl_ubo = PreparePointLightsUBO(item);
    WriteToBuffer(context_->device(), per_point_lights_ubo_mem_[fi],
                  per_point_lights_ubo_offset_, sizeof(pl_ubo), &pl_ubo);

    VulkanSpotLightsUBO sl_ubo = PrepareSpotLightsUBO(item);
    WriteToBuffer(context_->device(), per_spot_lights_ubo_mem_[fi],
                  per_spot_lights_ubo_offset_, sizeof(sl_ubo), &sl_ubo);
}

// ============================================================================
// DescriptorSet åˆ†é…ä¸Žæ›´æ–°
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

    // ç¨‹åºè‡³å°‘éœ€è¦ 3 ä¸ª set layoutï¼ˆSet 0=PerFrame, Set 1=PerScene+Lights, Set 2=PerMaterial+Samplersï¼Œ Set 3=ç‚¹å…‰é˜´å½±ï¼‰
    if (program->descriptor_set_layouts.size() < 3) {
        DEBUG_LOG_WARN("Mesh shader program has insufficient descriptor set layouts ({})",
                       program->descriptor_set_layouts.size());
        return VK_NULL_HANDLE;
    }

    // ä¸ºæ¯ä¸ª set åˆ†é…ä¸€ä¸ª DescriptorSetï¼ˆæœ€å¤š 4 ä¸ªï¼‰
    VkDescriptorSet sets[4] = {};
    const int set_count = static_cast<int>(program->descriptor_set_layouts.size());
    for (int s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) {
            DEBUG_LOG_WARN("Failed to allocate descriptor set for set {}", s);
            return VK_NULL_HANDLE;
        }
    }

    // åå°„æ£€æŸ¥ï¼šä»…å†™å…¥ shader å®žé™…å£°æ˜Žçš„ descriptor bindings
    auto has_binding = [&](uint32_t set, uint32_t binding, VkDescriptorType type) -> bool {
        for (const auto& b : program->reflection.bindings) {
            if (b.set == set && b.binding == binding && b.type == type) return true;
        }
        return false;
    };
    // ç±»åž‹æ— å…³ç‰ˆæœ¬ï¼šåªæ£€æŸ¥ set+binding æ˜¯å¦å­˜åœ¨ï¼ˆç”¨äºŽ BoneMatrices/MorphWeights ç­‰å¯èƒ½å­˜åœ¨ç±»åž‹å¾®å·®çš„ç»‘å®šï¼‰
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

    // --- Set 1 binding 1+: PBR ä¸“ç”¨ SSBO/UBOï¼ˆGBuffer æ¨¡å¼è·³è¿‡ï¼‰---
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
            // fallback: SSBO å ä½ bufferï¼Œå¿…é¡»æœ‰ STORAGE_BUFFER usage
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
        // å†™å…¥ SH æ•°æ®åˆ°å½“å‰å¸§çš„ UBO
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

    } // !gbuffer_mode â€” Set 1 PBR ä¸“ç”¨ç»‘å®šç»“æŸ

    // --- Set 2: PerMaterial UBO + é‡‡æ ·å™¨ ---
    if (gbuffer_mode) {
        // GBuffer æ¨¡å¼åªç»‘å®š albedo çº¹ç†åˆ° binding 1
        unsigned int tex_handle = item.texture_handle;
        if (tex_handle == 0) tex_handle = white_texture_handle_;
        const VulkanTexture* tex = resource_mgr.GetTexture(tex_handle);
        if (!tex) tex = resource_mgr.GetTexture(white_texture_handle_);
        if (tex && has_binding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
            VkDescriptorImageInfo img_info{};
            img_info.sampler = resource_mgr.material_sampler();
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

        // çº¹ç†é‡‡æ ·å™¨ (binding 1-5)
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

        int write_count = 1; // mat_write ç®—ç¬¬ 0 ä¸ª
        for (int i = 0; i < 5; ++i) {
            if (!has_binding(2, tex_bindings[i].binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) continue;
            unsigned int tex_handle = tex_bindings[i].handle;
            if (tex_handle == 0) tex_handle = white_texture_handle_;

            const VulkanTexture* tex = resource_mgr.GetTexture(tex_handle);
            if (!tex) tex = resource_mgr.GetTexture(white_texture_handle_);
            if (!tex) continue;

            image_infos[i].sampler = resource_mgr.material_sampler();
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

        // ç»‘å®š CSM é˜´å½±è´´å›¾åˆ° binding 6ï¼ˆsampler2DShadowï¼Œä½¿ç”¨æ¯”è¾ƒé‡‡æ ·å™¨ï¼‰
        VkDescriptorImageInfo shadow_image_infos[3] = {};
        VkWriteDescriptorSet shadow_write{};
        if (has_binding(2, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
            VkSampler cmp_sampler = resource_mgr.shadow_comparison_sampler();
            const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
            for (int i = 0; i < 3; ++i) {
                unsigned int sm_handle = global_state_.shadow_map[i];
                // shadow map handle æ˜¯ RT handleï¼Œéœ€ä»Ž RT èŽ·å– depth image view
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

        // ç»‘å®š Spot é˜´å½±è´´å›¾åˆ° binding 7
        VkDescriptorImageInfo spot_shadow_image_infos[4] = {};
        VkWriteDescriptorSet spot_shadow_write{};
        if (has_binding(2, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)) {
            const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
            for (int i = 0; i < 4; ++i) {
                unsigned int ss_handle = global_state_.spot_shadow_map[i];
                // spot shadow map handle æ˜¯ RT handleï¼Œéœ€ä»Ž RT èŽ·å– depth image view
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

        // BoneMatrices SSBO (binding 8) â€” æ•´ä¸ª buffer ç»‘å®šï¼Œpush constant æŽ§åˆ¶ offset
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
            glm::vec4 snow_params;
        } terrain_params{};
        terrain_params.flags.x = item.splat_enabled ? 1.0f : 0.0f;
        terrain_params.flags.y = item.snow_coverage;
        terrain_params.flags.z = item.snow_normal_threshold;
        terrain_params.flags.w = item.snow_edge_sharpness;
        terrain_params.tiling = item.splat_tiling;
        terrain_params.snow_params = glm::vec4(item.snow_albedo, item.snow_roughness);
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
            splat_infos[i].sampler = resource_mgr.material_sampler();
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

        // SkinnedInstBuf SSBO (set=2, binding=10) â€” ç¡¬ä»¶å®žä¾‹åŒ–è·¯å¾„
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

    // --- Set 3 binding 0: ç‚¹å…‰æºç«‹æ–¹ä½“é˜´å½±è´´å›¾ (u_point_shadow_maps[4]) ---
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

    // ç»‘å®šæ‰€æœ‰ DescriptorSet
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

    // å¤©ç©ºç›’ç€è‰²å™¨ä½¿ç”¨ Set 0 (PerFrame) + Set 2 (skybox sampler)
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

    // èŽ·å– dummy èµ„æºç”¨äºŽå¡«å……æœªä½¿ç”¨çš„ç»‘å®š
    const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
    VkSampler default_samp = resource_mgr.default_sampler();

    VkDescriptorBufferInfo dummy_ubo_info{};
    dummy_ubo_info.buffer = per_frame_ubo_[fi];
    dummy_ubo_info.offset = 0;
    dummy_ubo_info.range = sizeof(VulkanPerFrameUBO);

    // VUID-VkWriteDescriptorSet-descriptorType-00331: SSBO å ä½ buffer å¿…é¡»æœ‰ STORAGE_BUFFER usage
    VkDescriptorBufferInfo dummy_ssbo_info{};
    dummy_ssbo_info.buffer = dummy_ssbo_buffer_;
    dummy_ssbo_info.offset = 0;
    dummy_ssbo_info.range  = VK_WHOLE_SIZE;

    VkDescriptorImageInfo dummy_img_info{};
    dummy_img_info.sampler = default_samp;
    dummy_img_info.imageView = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
    dummy_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // ä¿æŒæ‰€æœ‰ image info çš„ç”Ÿå‘½å‘¨æœŸç›´åˆ° vkUpdateDescriptorSets
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
        // pImageInfo ç¨åŽé€šè¿‡ base ç´¢å¼•è®¾ç½®ï¼ˆimg_pool å¯èƒ½ reallocï¼‰
        writes.push_back(w);
        return base;
    };

    // è®°å½• <writes_index, img_pool_base> ä»¥ä¾¿åŽç»­ä¿®æ­£æŒ‡é’ˆ
    std::vector<std::pair<size_t, size_t>> img_fixups;

    // åå°„è¾…åŠ©: æ£€æŸ¥ (set, binding) æ˜¯å¦å­˜åœ¨äºŽç€è‰²å™¨ä¸­
    auto has_binding = [&](uint32_t set_idx, uint32_t bind_idx) -> bool {
        for (const auto& b : program->reflection.bindings)
            if (b.set == set_idx && b.binding == bind_idx) return true;
        return false;
    };

    // Set 0 (layouts[0]): PerFrame UBO â€” binding 0
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

        // binding 1: skybox cubemapï¼ˆå®žé™…çº¹ç†ï¼‰
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

    // Set 3 (layouts[3]): point_shadow_maps[4] â€” binding 0
    if (set_count > 3 && has_binding(3, 0)) {
        size_t base = push_img(sets[3], 0, 4);
        img_fixups.push_back({writes.size() - 1, base});
    }

    // ä¿®æ­£æ‰€æœ‰ image æè¿°ç¬¦çš„æŒ‡é’ˆï¼ˆimg_pool ä¸å† reallocï¼‰
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
// AllocateAllSetsWithDummies â€” åˆ†é…å…¨éƒ¨ descriptor sets å¹¶ç”¨ dummy æ•°æ®å¡«æ»¡
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

    // VUID-VkWriteDescriptorSet-descriptorType-00331: SSBO å ä½ buffer å¿…é¡»æœ‰ STORAGE_BUFFER usage
    VkDescriptorBufferInfo dummy_ssbo{};
    dummy_ssbo.buffer = dummy_ssbo_buffer_;
    dummy_ssbo.offset = 0;
    dummy_ssbo.range  = VK_WHOLE_SIZE;

    VkDescriptorImageInfo dummy_img{};
    dummy_img.sampler     = samp;
    dummy_img.imageView   = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
    dummy_img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // é¢„åˆ†é… image info æ± ï¼ˆé¿å…æŒ‡é’ˆæ‚¬ç©ºï¼‰
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

    // åå°„è¾…åŠ©: æ£€æŸ¥ (set, binding) æ˜¯å¦å­˜åœ¨äºŽç€è‰²å™¨ä¸­
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

    // ä¿®æ­£ image æŒ‡é’ˆ
    for (auto& [wi, base] : fixups) writes[wi].pImageInfo = &img_pool[base];

    if (!writes.empty())
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return sets;
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

    // åˆ†é…æ‰€æœ‰ setï¼ˆåŒ…æ‹¬ç©º layout çš„ setï¼‰
    std::vector<VkDescriptorSet> sets(set_count, VK_NULL_HANDLE);
    for (uint32_t s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    }

    // åŽå¤„ç† shader ä»…ä½¿ç”¨ set 2, binding 1 (screenTexture)
    // åªå†™å®žé™…å­˜åœ¨çš„ bindingsï¼Œé¿å…å†™å…¥ç©º layout æˆ–ä¸å­˜åœ¨çš„ binding
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

        // extra bindings (bloom, ssao, ae, lut ç­‰) â€” ä»…å†™ shader åå°„ä¸­å­˜åœ¨çš„ binding
        for (size_t i = 0; i < extra_bindings.size(); ++i) {
            auto [binding, tex_handle] = extra_bindings[i];
            bool binding_exists = false;
            for (const auto& b : program->reflection.bindings) {
                if (b.set == 2 && b.binding == binding) { binding_exists = true; break; }
            }
            if (!binding_exists) continue;
            VkDescriptorImageInfo& ei = extra_imgs[i];
            ei.sampler     = resource_mgr.default_sampler();
            // binding 5 åœ¨åŽå¤„ç† shader ä¸­æ˜¯ sampler3D (u_lut)ï¼Œéœ€è¦ 3D image view
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


} // namespace render
} // namespace dse


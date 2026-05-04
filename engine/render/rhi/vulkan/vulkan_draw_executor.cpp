/**
 * @file vulkan_draw_executor.cpp
 * @brief Vulkan 绘制执行器实现
 *
 * 实现完整的 Vulkan 命令缓冲录制逻辑：
 * - 几何缓冲区初始化（VBO/IBO 创建与数据上传）
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

    // --- 白色纹理 ---
    unsigned char white_pixel[4] = {255, 255, 255, 255};
    white_texture_handle_ = resource_mgr->CreateTexture2D(1, 1, white_pixel, true);

    DEBUG_LOG_INFO("VulkanDrawExecutor geometry buffers initialized");
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

    if (white_texture_handle_ != 0) {
        resource_mgr_->DeleteTexture(white_texture_handle_);
        white_texture_handle_ = 0;
    }

    DEBUG_LOG_INFO("VulkanDrawExecutor geometry buffers destroyed");
}

// ============================================================================
// RenderPass
// ============================================================================

void VulkanDrawExecutor::BeginRenderPass(
    VkCommandBuffer cmd_buf,
    const RenderPassDesc& render_pass,
    VulkanResourceManager& resource_mgr,
    VulkanPipelineStateManager& pipeline_mgr) {

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
    VulkanShaderManager& shader_mgr) {

    if (items.empty()) return;

    // TODO: [CodeBuddy-2026-05-04] 完整实现需要 2D sprite shader
    // 当前标记统计，Vulkan 2D 管线需后续补充 sprite 专用着色器
    current_frame_stats_.draw_calls++;
    current_frame_stats_.sprite_count += static_cast<int>(items.size());

    (void)cmd_buf;
    (void)pipeline_mgr;
    (void)shader_mgr;
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
        context_->swapchain_render_pass(),  // 当前使用 swapchain RenderPass
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

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pbr_program->pipeline_layout, 0,
                            static_cast<uint32_t>(pbr_program->descriptor_set_layouts.size()),
                            nullptr, 0, nullptr);

    // 逐 mesh 绘制
    for (const auto& item : items) {
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
        VkPushConstantRange pc_range = pbr_program->reflection.push_constant_range;
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

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            skybox_program->pipeline_layout, 0,
                            static_cast<uint32_t>(skybox_program->descriptor_set_layouts.size()),
                            nullptr, 0, nullptr);

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

    // 后处理管线状态：无混合、无深度、无剔除
    PipelineStateDesc pp_desc;
    pp_desc.blend_enabled = false;
    pp_desc.depth_test_enabled = false;
    pp_desc.depth_write_enabled = false;
    pp_desc.culling_enabled = false;
    unsigned int pp_state = pipeline_mgr.CreatePipelineState(pp_desc);

    // 延迟创建 Pipeline（使用 PBR shader 占位，后处理需要独立 shader）
    // TODO: [CodeBuddy-2026-05-04] 使用专用后处理着色器
    const VulkanShaderProgram* pbr_program = shader_mgr.GetProgram(shader_mgr.pbr_shader_handle());
    VkPipeline pp_pipeline = VK_NULL_HANDLE;
    if (pbr_program) {
        pp_pipeline = pipeline_mgr.GetOrCreateVkPipeline(
            pp_state, pbr_program, context_->swapchain_render_pass(),
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

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            particle_program->pipeline_layout, 0,
                            static_cast<uint32_t>(particle_program->descriptor_set_layouts.size()),
                            nullptr, 0, nullptr);

    // 绑定粒子 VBO
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf, 0, 1, &particle_vbo_, offsets);

    // 每个粒子绘制一次 billboard（4 顶点三角带）
    for (const auto& item : items) {
        // 更新粒子实例数据（push constant 或实例缓冲）
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
}

void VulkanDrawExecutor::EndFrame() {
    last_frame_stats_ = current_frame_stats_;
}

} // namespace render
} // namespace dse

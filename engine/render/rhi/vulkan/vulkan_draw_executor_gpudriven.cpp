/**
 * @file vulkan_draw_executor_gpudriven.cpp
 * @brief VulkanDrawExecutor frame management, compute dispatch, and GPU-driven rendering.
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

namespace dse { namespace render {
void WriteToBuffer(VkDevice device, VkDeviceMemory memory,
                   VkDeviceSize offset, VkDeviceSize size, const void* data);
} }

namespace dse {
namespace render {
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

void VulkanDrawExecutor::DispatchComputePass(
    VkCommandBuffer cmd_buf,
    const ComputeDispatch& dispatch,
    VulkanShaderManager& shader_mgr) {
    if (dispatch.shader == 0 || current_rt_handle_ == 0) return;
    DispatchBloomCompute(cmd_buf, dispatch.shader, dispatch.source_texture,
                         current_rt_handle_, dispatch.blend_weight, shader_mgr);
}

void VulkanDrawExecutor::DispatchBloomCompute(
    VkCommandBuffer cmd_buf,
    unsigned int cs_handle,
    unsigned int src_texture_handle,
    unsigned int dst_rt_handle,
    float blend_weight,
    VulkanShaderManager& shader_mgr) {

    const VulkanComputeProgram* cs = shader_mgr.GetComputeProgram(cs_handle);
    if (!cs || cs->pipeline == VK_NULL_HANDLE) return;

    const VulkanTexture*      src_tex = resource_mgr_->GetTexture(src_texture_handle);
    const VulkanRenderTarget* dst_rt  = resource_mgr_->GetRenderTarget(dst_rt_handle);
    if (!src_tex || !dst_rt || !dst_rt->allow_uav) return;

    VkDevice device = context_->device();

    // 1. å°† dst image è¿‡æ¸¡åˆ° GENERALï¼ˆä»¥æ”¯æŒ Storage å†™å…¥ï¼‰
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

    // 2. ç»‘å®š Compute Pipeline
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, cs->pipeline);

    // 3. åˆ†é…å¹¶æ›´æ–° DescriptorSet
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

    // 4. Push constants ï¼ˆtexel å¤§å° + upsample æ··åˆæƒé‡ï¼‰
    struct BloomParams { float src_w, src_h, dst_w, dst_h, blend_weight; };
    const BloomParams bp {
        src_tex->width  > 0 ? 1.0f / static_cast<float>(src_tex->width)  : 1.0f,
        src_tex->height > 0 ? 1.0f / static_cast<float>(src_tex->height) : 1.0f,
        dst_rt->width   > 0 ? 1.0f / static_cast<float>(dst_rt->width)   : 1.0f,
        dst_rt->height  > 0 ? 1.0f / static_cast<float>(dst_rt->height)  : 1.0f,
        blend_weight,
    };
    vkCmdPushConstants(cmd_buf, cs->pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bp), &bp);

    // 5. Dispatch
    const uint32_t gx = (static_cast<uint32_t>(dst_rt->width)  + 7) / 8;
    const uint32_t gy = (static_cast<uint32_t>(dst_rt->height) + 7) / 8;
    vkCmdDispatch(cmd_buf, gx, gy, 1);

    // 6. è¿‡æ¸¡å›ž SHADER_READ_ONLY
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
// GPU-Driven PBR æ¸²æŸ“è®¾ç½®
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

    // ä¼˜å…ˆä½¿ç”¨ GPU-driven shaderï¼ˆVS ä»Ž SSBO è¯» model, FS ä»Ž Material SSBO è¯»æè´¨ï¼‰
    unsigned int shader_handle = shader_mgr.gpu_driven_pbr_shader_handle();
    const VulkanShaderProgram* pbr_program = shader_mgr.GetProgram(shader_handle);
    if (!pbr_program) {
        // å›žé€€åˆ°æ ‡å‡† PBRï¼ˆä¸æ”¯æŒ glslang æ—¶ï¼‰
        shader_handle = shader_mgr.pbr_shader_handle();
        pbr_program = shader_mgr.GetProgram(shader_handle);
        if (!pbr_program) return;
    }

    // èŽ·å–å½“å‰ render passï¼ˆå¯èƒ½æ˜¯ç¦»å± RT çš„ render passï¼‰
    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // BatchVertex é¡¶ç‚¹æ ¼å¼ï¼ˆä¸Ž DrawMeshBatch ä¸€è‡´ï¼‰
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

    // æ»¡è¶³ pipeline layout ä¸­çš„ push constant range è¦æ±‚ï¼Œé˜²æ­¢ validation warning
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
        return;
    }
    WriteToBuffer(context_->device(), per_frame_ubo_mem_[current_frame_index_],
                  per_frame_ubo_offset_, sizeof(VulkanPerFrameUBO), &frame_ubo);
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
    for (int i = 0; i < 3; ++i) {
        scene_ubo.shadow_atlas_regions[i] = global_state_.shadow_atlas_region[i];
    }
    if (per_scene_ubo_offset_ + sizeof(VulkanPerSceneUBO) > per_scene_ubo_capacity_) {
        DEBUG_LOG_ERROR("[Vulkan] GPU_DRIVEN PER_SCENE_UBO OVERFLOW: offset={} capacity={}", per_scene_ubo_offset_, per_scene_ubo_capacity_);
        return;
    }
    WriteToBuffer(context_->device(), per_scene_ubo_mem_[current_frame_index_],
                  per_scene_ubo_offset_, sizeof(VulkanPerSceneUBO), &scene_ubo);
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
// GPU-Driven Shadow æ¸²æŸ“è®¾ç½®
// ============================================================

void VulkanDrawExecutor::SetupGPUDrivenShadow(VkCommandBuffer cmd_buf,
                                                const glm::mat4& light_view, const glm::mat4& light_proj,
                                                VulkanPipelineStateManager& pipeline_mgr,
                                                VulkanShaderManager& shader_mgr) {
    if (cmd_buf == VK_NULL_HANDLE || !context_) return;

    // ä¼˜å…ˆä½¿ç”¨ GPU-driven shadow shaderï¼ˆVS ä»Ž SSBO è¯» modelï¼‰
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

    // æ»¡è¶³ pipeline layout ä¸­çš„ push constant range è¦æ±‚ï¼Œé˜²æ­¢ validation warning
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

        // ä¸º sets 1-3 ç»‘å®šå½“å‰å¸§ç©º descriptor setsï¼Œé˜²æ­¢ä¸Žå‰ä¸€ä¸ª pipeline layout ä¸å…¼å®¹å¯¼è‡´ TDR
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
// GPU-Driven: æŒ‰çº¹ç†æ¡¶é‡æ–°ç»‘å®š Set 2
// ============================================================

void VulkanDrawExecutor::UpdateGPUDrivenMaterial(const void* mat_data) {
    // VK GPU-driven FS ä»Ž MaterialSSBO é€ instance è¯»æè´¨ï¼ˆv_material_id ç´¢å¼•ï¼‰ï¼Œ
    // ä¸å†éœ€è¦ per-bucket UBO æ›´æ–°ã€‚ä¿ç•™æŽ¥å£ä¾› DX11 ä½¿ç”¨ã€‚
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
        push_image(tb.binding, tex ? tex : white_tex, resource_mgr.material_sampler(),
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

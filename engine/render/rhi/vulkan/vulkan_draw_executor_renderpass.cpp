/**
 * @file vulkan_draw_executor_renderpass.cpp
 * @brief VulkanDrawExecutor render pass management and primitive drawing API.
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
void VulkanDrawExecutor::BeginRenderPass(
    VkCommandBuffer cmd_buf,
    const RenderPassDesc& render_pass,
    VulkanResourceManager& resource_mgr,
    VulkanPipelineStateManager& pipeline_mgr) {

    // æ›´æ–°å¸§ç´¢å¼•å’Œå½“å‰ RT å¥æŸ„
    current_frame_index_ = context_->current_frame() % MAX_FRAMES;
    current_rt_handle_ = render_pass.render_target;

    // ç¡®å®š Framebuffer å’Œ RenderPass
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkRenderPass vk_render_pass = VK_NULL_HANDLE;
    if (render_pass.render_target != 0) {
        const VulkanRenderTarget* rt = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt) {
            framebuffer = rt->framebuffer;
            // æ ¹æ®æ˜¯å¦éœ€è¦æ¸…é™¤é€‰æ‹© render pass å˜ä½“
            if (!render_pass.clear_color_enabled && rt->render_pass_load != VK_NULL_HANDLE) {
                vk_render_pass = rt->render_pass_load;
            } else {
                vk_render_pass = rt->render_pass;
            }
        }
    } else {
        // æ¸²æŸ“åˆ°å±å¹•ï¼šä½¿ç”¨å½“å‰ swapchain framebuffer
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
        // fallback: ä½¿ç”¨ pipeline_mgr ç¼“å­˜çš„ RenderPass
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

    // è®°å½•å½“å‰æ¿€æ´»çš„ render passï¼Œä¾›åŽç»­ Draw å‡½æ•°åˆ›å»º pipeline æ—¶ä½¿ç”¨
    current_render_pass_ = vk_render_pass;

    // ç¡®å®šæ¸²æŸ“åŒºåŸŸå¤§å°å’Œ MSAA é‡‡æ ·æ•°
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

    // è®¡ç®—å®žé™… attachment æ•°ï¼ˆå…¼å®¹ MRT GBuffer ä¸Ž MSAA resolveï¼‰
    const bool is_msaa_rt = (current_msaa_samples_ != VK_SAMPLE_COUNT_1_BIT);
    int num_color = 1;
    bool rt_color_present = true;
    bool rt_depth_present = false;
    if (render_pass.render_target != 0) {
        const VulkanRenderTarget* rt_for_attachments = resource_mgr.GetRenderTarget(render_pass.render_target);
        if (rt_for_attachments) {
            rt_color_present = rt_for_attachments->has_color;
            rt_depth_present = rt_for_attachments->has_depth;
            // MSAA è·¯å¾„åªç”¨ 1 ä¸ª color attachmentï¼ˆmsaa_color_textureï¼‰ï¼Œéž MSAA MRT æ‰æœ‰å¤šä¸ª
            num_color = is_msaa_rt ? 1 : (std::max)(1, rt_for_attachments->color_attachment_count);
            if (!rt_color_present) num_color = 0;
        }
    } else {
        // swapchainï¼šåªæœ‰ 1 ä¸ª colorï¼Œæ—  depth
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

    // é¡ºåºå¿…é¡»ä¸¥æ ¼åŒ¹é… CreateRenderTarget ä¸­ attachments çš„ push é¡ºåºï¼š
    //   [0..num_color-1] color attachments (æˆ– MSAA æ—¶ 1 ä¸ª MSAA color)
    //   [num_color]      depth (å¦‚æžœæœ‰)
    //   [num_color+1]    resolve target (ä»… MSAA + has_color)
    for (int i = 0; i < num_color; ++i) clear_values.push_back(color_cv);
    if (rt_depth_present) clear_values.push_back(depth_cv);
    if (rt_color_present && is_msaa_rt) clear_values.push_back(color_cv);

    begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    begin_info.pClearValues = clear_values.data();

    // äºŒåˆ†æ³•è¯Šæ–­ï¼šè·³è¿‡è¶…é™çš„ render passï¼ˆåœ¨ vkCmdBeginRenderPass ä¹‹å‰æ£€æŸ¥ï¼‰
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

    // VUID-VkGraphicsPipelineCreateInfo-renderPass-07609: pipeline çš„ colorBlend attachment æ•°
    // å¿…é¡»ç­‰äºŽ RP subpass çš„ color attachment æ•°ã€‚MRT GBuffer æ—¶ num_color>1ã€‚
    current_color_attachment_count_ = num_color;
    global_state_.current_frame_stats.render_passes += 1;
    const bool vk_depth_only = (!rt_color_present && rt_depth_present);
    global_state_.current_pass_depth_only = vk_depth_only;
    if (vk_depth_only) {
        global_state_.current_frame_stats.shadow_passes += 1;
    }
    DEBUG_LOG_TRACE("[Vulkan] BeginRenderPass: rt={} extent={}x{} msaa={} color_count={} depth={} pass#={}",
                   render_pass.render_target,
                   render_extent.width, render_extent.height,
                   static_cast<int>(current_msaa_samples_),
                   num_color, rt_depth_present,
                   render_pass_counter_ - 1);

    // è®¾ç½®åŠ¨æ€ viewport å’Œ scissorï¼ˆpipeline ä½¿ç”¨ VK_DYNAMIC_STATE_VIEWPORT/SCISSORï¼‰
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
    global_state_.current_pass_depth_only = false;
    if (skip_current_pass_) {
        DEBUG_LOG_TRACE("[Vulkan] EndRenderPass: SKIPPED rt={}", current_rt_handle_);
        skip_current_pass_ = false;
        return;
    }
    DEBUG_LOG_TRACE("[Vulkan] EndRenderPass: rt={}", current_rt_handle_);
    vkCmdEndRenderPass(cmd_buf);

    // å¯¹ offscreen RT çš„é¢œè‰²é™„ä»¶æ’å…¥æ˜¾å¼ image barrierï¼Œç¡®ä¿ layout è½¬æ¢å’Œå†…å­˜å¯è§æ€§
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

            // MSAA resolve target ä¹Ÿéœ€è¦ barrier
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
        // æ·±åº¦é™„ä»¶ barrier
        if (rt && rt->has_depth && rt->depth_texture.image != VK_NULL_HANDLE) {
            VkImageMemoryBarrier depth_barrier{};
            depth_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            depth_barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depth_barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depth_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depth_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            depth_barrier.image = rt->depth_texture.image;
            // VUID-VkImageMemoryBarrier-image-03320: D24S8/D32S8 å¿…é¡»åŒæ—¶å£°æ˜Ž DEPTH+STENCIL aspect
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
        // swapchain æˆ–æœªçŸ¥ RTï¼Œä½¿ç”¨å…¨å±€ memory barrier
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
// BlitRenderTargetToSwapchain â€” è¯Šæ–­ï¼šç›´æŽ¥ blit RTâ†’swapchainï¼Œç»•è¿‡ shader
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

    // 1. RT color â†’ TRANSFER_SRC
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

    // 2. Swapchain â†’ TRANSFER_DST
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

    // 4. RT â†’ SHADER_READ_ONLY, Swapchain â†’ PRESENT_SRC
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

    DEBUG_LOG_INFO("[Vulkan] BlitToSwapchain: rt={} ({}x{}) â†’ swapchain ({}x{})",
                   source_rt, rt->width, rt->height, extent.width, extent.height);
}

// ============================================================================
// é€šç”¨ç»˜åˆ¶åŽŸè¯­ (A1)
// ============================================================================

void VulkanDrawExecutor::PrimBindShaderProgram(unsigned int program_handle) {
    prim_program_handle_ = program_handle;
}

void VulkanDrawExecutor::PrimBindVertexBuffer(uint32_t slot, VkBuffer buffer, uint32_t stride,
                                              const std::vector<VertexAttr>& attrs,
                                              VertexInputRate rate) {
    prim_vbs_[slot] = PrimVbBinding{buffer, stride, attrs, rate};
}

void VulkanDrawExecutor::BuildPrimVertexInput(
        std::vector<VkVertexInputBindingDescription>& bindings,
        std::vector<VkVertexInputAttributeDescription>& vk_attrs) const {
    for (const auto& [slot, b] : prim_vbs_) {
        bindings.push_back(VkVertexInputBindingDescription{
            slot, b.stride,
            b.rate == VertexInputRate::PerInstance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                   : VK_VERTEX_INPUT_RATE_VERTEX});
        for (const auto& a : b.attrs) {
            VkFormat fmt = VK_FORMAT_R32G32B32_SFLOAT;
            switch (a.components) {
                case 1: fmt = VK_FORMAT_R32_SFLOAT;          break;
                case 2: fmt = VK_FORMAT_R32G32_SFLOAT;       break;
                case 3: fmt = VK_FORMAT_R32G32B32_SFLOAT;    break;
                case 4: fmt = VK_FORMAT_R32G32B32A32_SFLOAT; break;
                default: break;
            }
            vk_attrs.push_back(VkVertexInputAttributeDescription{a.location, slot, fmt, a.offset});
        }
    }
}

void VulkanDrawExecutor::BindPrimVertexBuffers(VkCommandBuffer cmd_buf) const {
    for (const auto& [slot, b] : prim_vbs_) {
        VkDeviceSize offset = 0;
        VkBuffer buf = b.buffer;
        vkCmdBindVertexBuffers(cmd_buf, slot, 1, &buf, &offset);
    }
}

void VulkanDrawExecutor::PrimPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size) {
    (void)stage; // Vulkan æŒ‰ç¨‹åºåå°„çš„ push_constant_range.stageFlags æŽ¨é€ï¼Œæ— éœ€é€æ¬¡ stage
    if (!data || size == 0) return;
    if (offset + size > kPrimPushMaxBytes) return;
    std::memcpy(prim_push_data_ + offset, data, size);
    if (offset + size > prim_push_size_) prim_push_size_ = offset + size;
    prim_has_push_ = true;
}

void VulkanDrawExecutor::PrimDraw(VkCommandBuffer cmd_buf, uint32_t vertex_count, uint32_t first_vertex,
                                  VulkanPipelineStateManager& pipeline_mgr,
                                  VulkanShaderManager& shader_mgr,
                                  VulkanResourceManager& resource_mgr) {
    if (skip_current_pass_) return;
    const VulkanShaderProgram* program = shader_mgr.GetProgram(prim_program_handle_);
    if (!program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDraw: shader program not available");
        return;
    }
    // VB å¯ç¼ºçœï¼ˆvertexlessï¼šgl_VertexIndex å– SSBOï¼Œæ¯›å‘é€ strand ç”¨ï¼‰ã€‚
    const bool has_vbo = HasPrimVbo();

    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    // é¡¶ç‚¹è¾“å…¥ï¼šç”±å„ slot çš„ BindVertexBufferï¼ˆVertexAttr + rateï¼‰ç¿»è¯‘ä¸º Vulkan é¡¶ç‚¹è¾“å…¥æè¿°
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> vk_attrs;
    BuildPrimVertexInput(bindings, vk_attrs);

    VkPipeline pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(), program, active_rp,
        bindings, vk_attrs,
        context_->swapchain_extent(), current_msaa_samples_, current_color_attachment_count_,
        false);
    if (pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDraw: failed to create pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (prim_has_push_ && program->reflection.push_constant_range.size > 0) {
        // stageFlags å–ç¨‹åºåå°„çš„ push rangeï¼ˆskybox=VERTEX / PP=FRAGMENTï¼‰ï¼Œ
        // æŽ¨é€å·²å†™å…¥çš„å­—èŠ‚èŒƒå›´ [0, prim_push_size_)ã€‚
        uint32_t pc_size = std::min(prim_push_size_, program->reflection.push_constant_range.size);
        vkCmdPushConstants(cmd_buf, program->pipeline_layout,
                           program->reflection.push_constant_range.stageFlags,
                           0, pc_size, prim_push_data_);
    }
    prim_has_push_ = false;
    prim_push_size_ = 0;

    if (prim_cubemap_ != 0) {
        AllocateAndUpdateSkyboxDescriptorSets(cmd_buf, program, prim_cubemap_, resource_mgr);
    } else {
        // é€šç”¨ UBO/SSBO/çº¹ç†ç»‘å®šï¼ˆæ¯›å‘ï¼šç»„åˆ HairUniforms UBO\@set0.b0 + position/tangent SSBO\@set7.b0/b1ï¼‰ã€‚
        AllocateAndUpdateGenericDescriptorSets(cmd_buf, program, resource_mgr);
    }

    if (has_vbo) {
        BindPrimVertexBuffers(cmd_buf);
    }
    vkCmdDraw(cmd_buf, vertex_count, 1, first_vertex, 0);
    ClearExtraVertexSlots();

    global_state_.current_frame_stats.draw_calls++;
}

// ============================================================================
// é€šç”¨ç»˜åˆ¶åŽŸè¯­ (B0): ç´¢å¼• / 2D çº¹ç† / UBO / ç´¢å¼•ç»˜åˆ¶
// ============================================================================

void VulkanDrawExecutor::PrimBindIndexBuffer(VkBuffer buffer, IndexType type) {
    prim_index_buffer_ = buffer;
    prim_index_type_ = (type == IndexType::UInt32) ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
}

void VulkanDrawExecutor::PrimBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
    // Vulkan çš„ image view åœ¨çº¹ç†åˆ›å»ºæ—¶å·²æŒ‰ç»´åº¦å®šåž‹ï¼›å¥‘çº¦ slot æš‚å­˜ï¼ŒPrimDrawIndexed æ—¶æ˜ å°„åˆ°å…·ä½“ bindingã€‚
    // cubemapï¼ˆTexCubeï¼‰èµ°å¤©ç©ºç›’ä¸“ç”¨ descriptor set è·¯å¾„ï¼ˆset0.b0 samplerCubeï¼‰ï¼Œä¸Žé€šç”¨ 2D çº¹ç†ç»‘å®šåˆ†ç¦»ã€‚
    if (dim == TextureDim::TexCube) {
        prim_cubemap_ = texture_handle;  // slot å›ºå®š set0.b0ï¼ˆspike ä»…å• cubemap bindingï¼‰
        return;
    }
    prim_textures_[slot] = texture_handle;
}

void VulkanDrawExecutor::PrimBindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                               uint32_t /*offset*/, uint32_t /*size*/) {
    // å¥‘çº¦ slot æš‚å­˜ï¼ˆoffset/size å­åŒºé—´ v1 æš‚ä¸æ”¯æŒï¼Œæ•´å—ç»‘å®šï¼‰ã€‚
    prim_ubos_[slot] = buffer_handle;
}

void VulkanDrawExecutor::PrimBindStorageBuffer(uint32_t slot, unsigned int buffer_handle,
                                               uint32_t offset, uint32_t size) {
    // å¥‘çº¦ slot æš‚å­˜ï¼ŒPrimDrawIndexed* æ—¶æ˜ å°„åˆ°ç¬¬ N ä¸ª STORAGE_BUFFER bindingï¼ˆoffset/size èµ° rangeï¼‰ã€‚
    prim_ssbos_[slot] = PrimSSBOBinding{buffer_handle, offset, size};
}

void VulkanDrawExecutor::AllocateAndUpdateGenericDescriptorSets(
    VkCommandBuffer cmd_buf,
    const VulkanShaderProgram* program,
    VulkanResourceManager& resource_mgr) {

    auto device = context_->device();
    uint32_t fi = current_frame_index_;
    const uint32_t set_count = static_cast<uint32_t>(program->descriptor_set_layouts.size());
    if (set_count == 0) return;

    std::vector<VkDescriptorSet> sets(set_count, VK_NULL_HANDLE);
    for (uint32_t s = 0; s < set_count; ++s) {
        sets[s] = resource_mgr.AllocateDescriptorSet(program->descriptor_set_layouts[s]);
        if (sets[s] == VK_NULL_HANDLE) return;
    }

    const VulkanTexture* white_tex = resource_mgr.GetTexture(white_texture_handle_);
    VkSampler default_samp = resource_mgr.default_sampler();

    VkDescriptorBufferInfo dummy_ubo_info{};
    dummy_ubo_info.buffer = per_frame_ubo_[fi];
    dummy_ubo_info.offset = 0;
    dummy_ubo_info.range = sizeof(VulkanPerFrameUBO);

    VkDescriptorBufferInfo dummy_ssbo_info{};
    dummy_ssbo_info.buffer = dummy_ssbo_buffer_;
    dummy_ssbo_info.offset = 0;
    dummy_ssbo_info.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo dummy_img_info{};
    dummy_img_info.sampler = default_samp;
    dummy_img_info.imageView = white_tex ? white_tex->image_view : VK_NULL_HANDLE;
    dummy_img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // åå°„ binding æŒ‰ (set,binding) å‡åºï¼šæŠŠå¥‘çº¦ slot é¡ºåºæ˜ å°„åˆ°ç¬¬ N ä¸ªåŒç±» bindingã€‚
    std::vector<DescriptorBindingInfo> sorted = program->reflection.bindings;
    std::sort(sorted.begin(), sorted.end(), [](const DescriptorBindingInfo& a, const DescriptorBindingInfo& b) {
        return a.set != b.set ? a.set < b.set : a.binding < b.binding;
    });

    // info æ± åœ¨æ‰€æœ‰å†™å…¥æ”¶é›†å®Œæ¯•åŽå† realloc å®šç¨¿ï¼Œæ•…å…ˆè®° base ç´¢å¼•ã€æœ€åŽä¿®æŒ‡é’ˆã€‚
    std::vector<VkDescriptorBufferInfo> buf_pool;
    std::vector<VkDescriptorImageInfo> img_pool;
    buf_pool.reserve(sorted.size());
    img_pool.reserve(sorted.size() * 4 + 1);
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(sorted.size());
    std::vector<std::tuple<size_t, bool, size_t>> fixups;  // <write_idx, is_image, pool_base>

    uint32_t ubo_slot = 0;   // å¥‘çº¦ BindUniformBuffer slot è®¡æ•°ï¼ˆæŒ‰ binding å‡åºï¼‰
    uint32_t tex_slot = 0;   // å¥‘çº¦ BindTexture slot è®¡æ•°ï¼ˆæŒ‰ binding å‡åºï¼‰
    uint32_t ssbo_slot = 0;  // å¥‘çº¦ BindStorageBuffer slot è®¡æ•°ï¼ˆæŒ‰ binding å‡åºï¼‰

    for (const auto& b : sorted) {
        if (b.set >= set_count) continue;
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = sets[b.set];
        w.dstBinding = b.binding;
        w.descriptorCount = b.count;
        w.descriptorType = b.type;

        if (b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            b.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) {
            VkDescriptorBufferInfo info = dummy_ubo_info;
            auto it = prim_ubos_.find(ubo_slot);
            if (it != prim_ubos_.end()) {
                const VulkanBuffer* ub = resource_mgr.GetBuffer(it->second);
                if (ub && ub->buffer) { info.buffer = ub->buffer; info.offset = 0; info.range = ub->size; }
            }
            size_t base = buf_pool.size();
            buf_pool.push_back(info);
            fixups.push_back({writes.size(), false, base});
            writes.push_back(w);
            ++ubo_slot;
        } else if (b.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            VkDescriptorImageInfo info = dummy_img_info;
            auto it = prim_textures_.find(tex_slot);
            if (it != prim_textures_.end()) {
                const VulkanTexture* t = resource_mgr.GetTexture(it->second);
                if (t && t->image_view) {
                    info.imageView = t->image_view;
                    if (t->sampler) info.sampler = t->sampler;
                }
            }
            size_t base = img_pool.size();
            for (uint32_t j = 0; j < b.count; ++j) img_pool.push_back(info);
            fixups.push_back({writes.size(), true, base});
            writes.push_back(w);
            ++tex_slot;
        } else if (b.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
            VkDescriptorBufferInfo info = dummy_ssbo_info;
            auto it = prim_ssbos_.find(ssbo_slot);
            if (it != prim_ssbos_.end()) {
                const VulkanBuffer* sb = resource_mgr.GetSSBO(it->second.handle);
                if (sb && sb->buffer) {
                    info.buffer = sb->buffer;
                    info.offset = it->second.offset;
                    info.range = (it->second.size != 0)
                        ? it->second.size
                        : (sb->size - it->second.offset);
                }
            }
            size_t base = buf_pool.size();
            buf_pool.push_back(info);
            fixups.push_back({writes.size(), false, base});
            writes.push_back(w);
            ++ssbo_slot;
        } else {
            // å…¶å®ƒç±»åž‹ (storage image / sampled image ç­‰) v1 æš‚ç”¨ dummy ubo å ä½ï¼Œé¿å…æœªåˆå§‹åŒ–æè¿°ç¬¦ã€‚
            size_t base = buf_pool.size();
            buf_pool.push_back(dummy_ubo_info);
            fixups.push_back({writes.size(), false, base});
            writes.push_back(w);
        }
    }

    for (auto& [wi, is_img, base] : fixups) {
        if (is_img) writes[wi].pImageInfo = &img_pool[base];
        else        writes[wi].pBufferInfo = &buf_pool[base];
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
    }
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            program->pipeline_layout, 0,
                            set_count, sets.data(), 0, nullptr);
}

void VulkanDrawExecutor::PrimDrawIndexed(VkCommandBuffer cmd_buf, uint32_t index_count,
                                         uint32_t first_index, int32_t base_vertex,
                                         VulkanPipelineStateManager& pipeline_mgr,
                                         VulkanShaderManager& shader_mgr,
                                         VulkanResourceManager& resource_mgr) {
    PrimDrawIndexedInstanced(cmd_buf, index_count, 1, first_index, base_vertex, 0,
                             pipeline_mgr, shader_mgr, resource_mgr);
}

void VulkanDrawExecutor::PrimDrawIndexedInstanced(VkCommandBuffer cmd_buf, uint32_t index_count,
                                                  uint32_t instance_count, uint32_t first_index,
                                                  int32_t base_vertex, uint32_t first_instance,
                                                  VulkanPipelineStateManager& pipeline_mgr,
                                                  VulkanShaderManager& shader_mgr,
                                                  VulkanResourceManager& resource_mgr) {
    if (skip_current_pass_) return;
    const VulkanShaderProgram* program = shader_mgr.GetProgram(prim_program_handle_);
    if (!program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexedInstanced: shader program not available");
        return;
    }
    // VB å¯ç¼ºçœï¼ˆvertexlessï¼šgl_VertexIndex å–ç´¢å¼•å€¼â†’SSBOï¼Œæ¯›å‘ç”¨ï¼‰ï¼›IB å¿…é¡»å­˜åœ¨ã€‚
    if (prim_index_buffer_ == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexedInstanced: index buffer not bound");
        return;
    }

    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    const bool has_vbo = HasPrimVbo();
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> vk_attrs;
    BuildPrimVertexInput(bindings, vk_attrs);

    VkPipeline pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(), program, active_rp,
        bindings, vk_attrs,
        context_->swapchain_extent(), current_msaa_samples_, current_color_attachment_count_,
        false);
    if (pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexed: failed to create pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (prim_has_push_ && program->reflection.push_constant_range.size > 0) {
        // åŽå¤„ç†å‚æ•°èµ°çœŸ push constantï¼ˆstageFlags=FRAGMENTï¼Œå–ç¨‹åºåå°„ rangeï¼‰ã€‚
        uint32_t pc_size = std::min(prim_push_size_, program->reflection.push_constant_range.size);
        vkCmdPushConstants(cmd_buf, program->pipeline_layout,
                           program->reflection.push_constant_range.stageFlags,
                           0, pc_size, prim_push_data_);
    }
    prim_has_push_ = false;
    prim_push_size_ = 0;

    AllocateAndUpdateGenericDescriptorSets(cmd_buf, program, resource_mgr);

    if (has_vbo) {
        BindPrimVertexBuffers(cmd_buf);
    }
    vkCmdBindIndexBuffer(cmd_buf, prim_index_buffer_, 0, prim_index_type_);
    vkCmdDrawIndexed(cmd_buf, index_count, instance_count, first_index, base_vertex, first_instance);
    ClearExtraVertexSlots();

    global_state_.current_frame_stats.draw_calls++;
    if (instance_count != 1 || first_instance != 0) {
        global_state_.current_frame_stats.instanced_draw_calls++;
    }
}

void VulkanDrawExecutor::PrimDrawIndexedIndirect(VkCommandBuffer cmd_buf, unsigned int indirect_buffer,
                                                 uint32_t byte_offset,
                                                 VulkanPipelineStateManager& pipeline_mgr,
                                                 VulkanShaderManager& shader_mgr,
                                                 VulkanResourceManager& resource_mgr) {
    if (skip_current_pass_) return;
    const VulkanShaderProgram* program = shader_mgr.GetProgram(prim_program_handle_);
    if (!program) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexedIndirect: shader program not available");
        return;
    }
    if (!HasPrimVbo() || prim_index_buffer_ == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexedIndirect: vertex/index buffer not bound");
        return;
    }

    // è§£æž indirect VkBufferï¼šå…ˆæŸ¥ indirect mapï¼Œå†é€€å›ž SSBO mapï¼ˆdraw cmd å­˜äºŽå¸¦ INDIRECT_BIT çš„ SSBOï¼‰ã€‚
    const VulkanBuffer* arg_buf = resource_mgr.GetIndirectBuffer(indirect_buffer);
    if (!arg_buf || arg_buf->buffer == VK_NULL_HANDLE) {
        arg_buf = resource_mgr.GetSSBO(indirect_buffer);
    }
    if (!arg_buf || arg_buf->buffer == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexedIndirect: indirect buffer not found");
        return;
    }

    VkRenderPass active_rp = current_render_pass_ != VK_NULL_HANDLE
        ? current_render_pass_ : context_->swapchain_render_pass();

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> vk_attrs;
    BuildPrimVertexInput(bindings, vk_attrs);

    VkPipeline pipeline = pipeline_mgr.GetOrCreateVkPipeline(
        pipeline_mgr.active_pipeline_state(), program, active_rp,
        bindings, vk_attrs,
        context_->swapchain_extent(), current_msaa_samples_, current_color_attachment_count_,
        false);
    if (pipeline == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("VulkanDrawExecutor::PrimDrawIndexedIndirect: failed to create pipeline");
        return;
    }

    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    AllocateAndUpdateGenericDescriptorSets(cmd_buf, program, resource_mgr);

    BindPrimVertexBuffers(cmd_buf);
    vkCmdBindIndexBuffer(cmd_buf, prim_index_buffer_, 0, prim_index_type_);
    // draw_count=1ï¼šä»Ž byte_offset å¤„è¯»å–ä¸€æ¡ VkDrawIndexedIndirectCommandï¼ˆ5Ã—uint32ï¼Œä¸‰ç«¯å¸ƒå±€ä¸€è‡´ï¼‰ã€‚
    // å¥‘çº¦ï¼šbase_instance åç§»é¡»ç» SSBO åç§»è¡¨è¾¾ï¼ˆÂ§6ï¼‰ã€‚
    vkCmdDrawIndexedIndirect(cmd_buf, arg_buf->buffer, static_cast<VkDeviceSize>(byte_offset),
                             1, static_cast<uint32_t>(sizeof(DrawElementsIndirectCommand)));
    ClearExtraVertexSlots();

    global_state_.current_frame_stats.draw_calls++;
    global_state_.current_frame_stats.indirect_draw_calls++;
}

// ============================================================================
// æ¸²æŸ“ç»Ÿè®¡
// ============================================================================

} // namespace render
} // namespace dse

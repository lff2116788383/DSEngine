/**
 * @file vulkan_rhi_device_compute.cpp
 * @brief VulkanRhiDevice compute, HiZ, and uniform management.
 */

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include "engine/render/rhi/vulkan/vulkan_rhi_device_internal.h"
#include "engine/render/rhi/gpu_scene_types.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/base/debug.h"
#include "engine/platform/screen.h"

#include <algorithm>
#include <cstring>

namespace dse {
namespace render {

unsigned int VulkanRhiDevice::CreateComputeShader(const std::string& source) {
    if (!initialized_) return 0u;
    return shader_mgr_.CreateComputeProgram(source);
}

void VulkanRhiDevice::DeleteComputeShader(unsigned int handle) {
    shader_mgr_.DeleteComputeProgram(handle);
}

void VulkanRhiDevice::BeginComputePass() {
    if (!initialized_ || in_compute_pass_) return;

    compute_cmd_buffer_ = resource_mgr_.BeginSingleTimeCommands();
    in_compute_pass_ = true;
    pending_compute_images_.clear();
    pending_compute_samplers_.clear();
}

void VulkanRhiDevice::EndComputePass() {
    if (!in_compute_pass_ || compute_cmd_buffer_ == VK_NULL_HANDLE) return;

    resource_mgr_.EndSingleTimeCommands(compute_cmd_buffer_);
    compute_cmd_buffer_ = VK_NULL_HANDLE;
    in_compute_pass_ = false;
    pending_compute_images_.clear();
    pending_compute_samplers_.clear();
}

void VulkanRhiDevice::DispatchCompute(unsigned int shader_handle,
                                       unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) {
    if (!initialized_ || shader_handle == 0) return;

    const auto* prog = shader_mgr_.GetComputeProgram(shader_handle);
    if (!prog || prog->pipeline == VK_NULL_HANDLE) return;

    // ç¡®å®šå½•åˆ¶ç›®æ ‡ cmd buffer
    const bool batched = in_compute_pass_ && compute_cmd_buffer_ != VK_NULL_HANDLE;
    VkCommandBuffer cmd = batched ? compute_cmd_buffer_ : resource_mgr_.BeginSingleTimeCommands();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, prog->pipeline);

    // ç»‘å®š descriptor setï¼ˆSSBO + storage image + samplerï¼‰
    if (prog->descriptor_set_layout != VK_NULL_HANDLE) {
        VkDescriptorSet ds = resource_mgr_.AllocateDescriptorSet(prog->descriptor_set_layout);
        if (ds != VK_NULL_HANDLE) {
            std::vector<VkWriteDescriptorSet> writes;
            std::vector<VkDescriptorBufferInfo> buffer_infos;
            std::vector<VkDescriptorImageInfo>  image_infos;
            buffer_infos.reserve(bound_ssbos_.size());
            image_infos.reserve(pending_compute_images_.size() + pending_compute_samplers_.size());

            // SSBO ç»‘å®šï¼ˆbinding 0..ssbo_binding_count-1ï¼‰
            for (auto& [binding, ssbo_handle] : bound_ssbos_) {
                if (binding >= prog->ssbo_binding_count) continue;
                const auto* ssbo = resource_mgr_.GetSSBO(ssbo_handle);
                if (!ssbo) continue;
                VkDescriptorBufferInfo buf_info{};
                buf_info.buffer = ssbo->buffer;
                buf_info.offset = 0;
                buf_info.range  = ssbo->size;
                buffer_infos.push_back(buf_info);
                VkWriteDescriptorSet w{};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = ds;
                w.dstBinding      = binding;
                w.descriptorCount = 1;
                w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                w.pBufferInfo     = &buffer_infos.back();
                writes.push_back(w);
            }

            // Storage image ç»‘å®šï¼ˆlayout binding = ssbo_count + user_bindingï¼‰
            uint32_t img_base = prog->ssbo_binding_count;
            uint32_t total_bindings = prog->ssbo_binding_count + prog->storage_image_count + prog->sampler_count;
            for (auto& [binding, img_bind] : pending_compute_images_) {
                if (img_base + binding >= total_bindings) continue;
                VkImageView view = VK_NULL_HANDLE;
                // æ£€æŸ¥ Hi-Z çº¹ç†
                if (hiz_impl_) {
                    auto hit = hiz_impl_->textures.find(img_bind.texture_handle);
                    if (hit != hiz_impl_->textures.end()) {
                        auto& hiz = hit->second;
                        int mip = img_bind.mip_level >= 0 ? img_bind.mip_level : 0;
                        if (mip < static_cast<int>(hiz.mip_views.size()))
                            view = hiz.mip_views[mip];
                    }
                }
                // æ™®é€šçº¹ç†
                if (view == VK_NULL_HANDLE) {
                    const auto* tex = resource_mgr_.GetTexture(img_bind.texture_handle);
                    if (tex) view = tex->image_view;
                }
                if (view == VK_NULL_HANDLE) continue;
                VkDescriptorImageInfo ii{};
                ii.imageView   = view;
                ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                image_infos.push_back(ii);
                VkWriteDescriptorSet w{};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = ds;
                w.dstBinding      = img_base + binding;
                w.descriptorCount = 1;
                w.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                w.pImageInfo      = &image_infos.back();
                writes.push_back(w);
            }

            // Sampler ç»‘å®šï¼ˆlayout binding = ssbo_count + storage_image_count + user_unitï¼‰
            uint32_t smp_base = prog->ssbo_binding_count + prog->storage_image_count;
            for (auto& [unit, tex_handle] : pending_compute_samplers_) {
                if (smp_base + unit >= total_bindings) continue;
                VkImageView view = VK_NULL_HANDLE;
                VkSampler sampler = VK_NULL_HANDLE;
                // æ£€æŸ¥ Hi-Z çº¹ç†ï¼ˆstorage image ä½¿ç”¨åŽä¿æŒ GENERAL layoutï¼‰
                bool is_hiz_texture = false;
                if (hiz_impl_) {
                    auto hit = hiz_impl_->textures.find(tex_handle);
                    if (hit != hiz_impl_->textures.end()) {
                        view = hit->second.full_view;
                        sampler = resource_mgr_.default_sampler();
                        is_hiz_texture = true;
                    }
                }
                // æ™®é€šçº¹ç†
                if (view == VK_NULL_HANDLE) {
                    const auto* tex = resource_mgr_.GetTexture(tex_handle);
                    if (tex && tex->image_view != VK_NULL_HANDLE) {
                        view = tex->image_view;
                        sampler = tex->sampler != VK_NULL_HANDLE ? tex->sampler : resource_mgr_.default_sampler();
                    }
                }
                // Render target depth attachmentï¼ˆHi-Z ä½¿ç”¨ PreZ depthï¼‰
                bool is_depth_attachment = false;
                if (view == VK_NULL_HANDLE) {
                    VkImageView depth_view = resource_mgr_.GetRenderTargetDepthImageView(tex_handle);
                    if (depth_view != VK_NULL_HANDLE) {
                        view = depth_view;
                        sampler = resource_mgr_.default_sampler();
                        is_depth_attachment = true;
                    }
                }
                if (view == VK_NULL_HANDLE) continue;
                VkDescriptorImageInfo ii{};
                ii.sampler     = sampler;
                ii.imageView   = view;
                ii.imageLayout = is_depth_attachment
                    ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                    : (is_hiz_texture ? VK_IMAGE_LAYOUT_GENERAL
                                      : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                image_infos.push_back(ii);
                VkWriteDescriptorSet w{};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = ds;
                w.dstBinding      = smp_base + unit;
                w.descriptorCount = 1;
                w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w.pImageInfo      = &image_infos.back();
                writes.push_back(w);
            }

            if (!writes.empty()) {
                vkUpdateDescriptorSets(context_.device(),
                    static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                    prog->pipeline_layout, 0, 1, &ds, 0, nullptr);
            }
        }
    }

    // Push constants
    if (prog->push_constant_size > 0 && !compute_push_constants_.empty()) {
        uint32_t size = std::min(prog->push_constant_size,
                                 static_cast<uint32_t>(compute_push_constants_.size()));
        vkCmdPushConstants(cmd, prog->pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, size,
                           compute_push_constants_.data());
    }
    // Dispatch åŽæ¸…ç©ºçŠ¶æ€ç¼“å­˜ï¼Œé¿å…è·¨ dispatch ç´¯ç§¯
    compute_push_constants_.clear();
    compute_uniform_layouts_.clear();
    compute_uniform_next_offset_ = 0;
    pending_compute_images_.clear();
    pending_compute_samplers_.clear();

    vkCmdDispatch(cmd, groups_x, groups_y, groups_z);

    if (!batched) {
        // å•æ¬¡æ¨¡å¼ï¼šæ’å…¥ barrier + æäº¤
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
        resource_mgr_.EndSingleTimeCommands(cmd);
    }
}

// ============================================================
// RenderGraph è‡ªåŠ¨å±éšœï¼šç²¾ç¡® VkImageMemoryBarrier
// ============================================================

namespace {

struct VkBarrierMapping {
    VkImageLayout layout;
    VkAccessFlags access;
    VkPipelineStageFlags stage;
};

VkBarrierMapping MapResourceState(ResourceState state) {
    switch (state) {
    case ResourceState::RenderTarget:
        return { VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    case ResourceState::DepthWrite:
        return { VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT };
    case ResourceState::DepthRead:
        return { VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                 VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT };
    case ResourceState::ShaderRead:
        return { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
    case ResourceState::UnorderedAccess:
        return { VK_IMAGE_LAYOUT_GENERAL,
                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
    case ResourceState::CopySource:
        return { VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_ACCESS_TRANSFER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT };
    case ResourceState::CopyDest:
        return { VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT };
    case ResourceState::Present:
        return { VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                 0,
                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };
    case ResourceState::Undefined:
    default:
        return { VK_IMAGE_LAYOUT_UNDEFINED,
                 0,
                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
    }
}

} // anonymous namespace

// ============================================================
// å³æ—¶ç»˜åˆ¶ / RT blit åŽŸè¯­ï¼ˆç¼–è¾‘å™¨æž¶æž„ Â§5.A / Â§5.Bï¼‰
// ============================================================

namespace {

/// é¡¶ç‚¹å±žæ€§åˆ†é‡æ•° â†’ VkFormatï¼ˆfloat åˆ†é‡ï¼‰ã€‚
VkFormat ImmAttrVkFormat(int components) {
    switch (components) {
    case 1:  return VK_FORMAT_R32_SFLOAT;
    case 2:  return VK_FORMAT_R32G32_SFLOAT;
    case 3:  return VK_FORMAT_R32G32B32_SFLOAT;
    case 4:  return VK_FORMAT_R32G32B32A32_SFLOAT;
    default: return VK_FORMAT_R32G32B32A32_SFLOAT;
    }
}

void AppendBytes(std::string& key, const void* data, size_t size) {
    key.append(reinterpret_cast<const char*>(data), size);
}

} // anonymous namespace

VkPipeline VulkanRhiDevice::GetOrCreateImmediatePipeline(
    const ImmediateDrawDesc& desc,
    const VulkanShaderProgram* program,
    VkRenderPass render_pass,
    uint32_t color_attachment_count) {

    // --- å¤åˆé”®ï¼šVS/FS module + render_pass + topology + blend/depth + é¡¶ç‚¹å±žæ€§å¸ƒå±€ ---
    std::string key;
    AppendBytes(key, &program->vert_module, sizeof(program->vert_module));
    AppendBytes(key, &program->frag_module, sizeof(program->frag_module));
    AppendBytes(key, &render_pass, sizeof(render_pass));
    uint8_t topo = static_cast<uint8_t>(desc.topology);
    AppendBytes(key, &topo, sizeof(topo));
    uint8_t flags = (desc.blend ? 1u : 0u) | (desc.depth_test ? 2u : 0u);
    AppendBytes(key, &flags, sizeof(flags));
    int32_t stride = desc.stride_bytes;
    AppendBytes(key, &stride, sizeof(stride));
    AppendBytes(key, &color_attachment_count, sizeof(color_attachment_count));
    for (const auto& a : desc.attribs) {
        int32_t loc = a.location, comp = a.components, off = a.offset_bytes;
        AppendBytes(key, &loc, sizeof(loc));
        AppendBytes(key, &comp, sizeof(comp));
        AppendBytes(key, &off, sizeof(off));
    }

    auto it = immediate_pipelines_.find(key);
    if (it != immediate_pipelines_.end()) return it->second;

    VkDevice device = context_.device();

    // --- Shader Stages ---
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = program->vert_module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = program->frag_module;
    stages[1].pName = "main";

    // --- Vertex Inputï¼šå• bindingï¼Œper-vertex ---
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = static_cast<uint32_t>(desc.stride_bytes);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrs;
    attrs.reserve(desc.attribs.size());
    for (const auto& a : desc.attribs) {
        VkVertexInputAttributeDescription va{};
        va.location = static_cast<uint32_t>(a.location);
        va.binding = 0;
        va.format = ImmAttrVkFormat(a.components);
        va.offset = static_cast<uint32_t>(a.offset_bytes);
        attrs.push_back(va);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = (desc.stride_bytes > 0) ? 1 : 0;
    vertex_input.pVertexBindingDescriptions = (desc.stride_bytes > 0) ? &binding : nullptr;
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertex_input.pVertexAttributeDescriptions = attrs.empty() ? nullptr : attrs.data();

    // --- Input Assembly ---
    VkPrimitiveTopology vk_topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    switch (desc.topology) {
    case ImmediateTopology::Lines:     vk_topo = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;  break;
    case ImmediateTopology::LineStrip: vk_topo = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
    case ImmediateTopology::Triangles:
    default:                           vk_topo = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
    }
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = vk_topo;

    // --- Viewport/Scissor èµ° dynamic stateï¼ˆå®žé™…å€¼åœ¨å½•åˆ¶æ—¶è®¾ç½®ï¼‰---
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // å³æ—¶ç»˜åˆ¶ä¸å‰”é™¤ï¼ˆæ‹¾å–/å…¨å± quadï¼‰
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = desc.depth_test ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = desc.blend ? VK_TRUE : VK_FALSE;
    if (desc.blend) {
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    const uint32_t blend_count = (color_attachment_count > 0) ? color_attachment_count : 1;
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(blend_count, blend_attachment);

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = blend_count;
    blend.pAttachments = blend_attachments.data();

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vertex_input;
    ci.pInputAssemblyState = &input_assembly;
    ci.pViewportState = &viewport_state;
    ci.pRasterizationState = &rasterizer;
    ci.pMultisampleState = &multisample;
    ci.pDepthStencilState = &depth_stencil;
    ci.pColorBlendState = &blend;
    ci.pDynamicState = &dynamic_state;
    ci.layout = program->pipeline_layout;
    ci.renderPass = render_pass;
    ci.subpass = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(device, context_.pipeline_cache(), 1, &ci, nullptr, &pipeline) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] ImmediateDraw: failed to create pipeline");
        return VK_NULL_HANDLE;
    }
    immediate_pipelines_[key] = pipeline;
    return pipeline;
}

void VulkanRhiDevice::ImmediateDraw(const ImmediateDrawDesc& desc) {
    EnsureInitialized();
    if (!initialized_ || desc.shader_program == 0) return;
    if (desc.render_target == 0) {
        // å³æ—¶ç»˜åˆ¶ç›®æ ‡é¡»ä¸ºç¦»å± RTï¼›swapchain ç›´ç»˜èµ°å‘ˆçŽ°å±‚ï¼ˆÂ§5.3ï¼‰ï¼Œæ­¤å¤„ä¸æ”¯æŒã€‚
        DEBUG_LOG_WARN("[Vulkan] ImmediateDraw: default framebuffer target unsupported");
        return;
    }

    const VulkanShaderProgram* program = shader_mgr_.GetProgram(desc.shader_program);
    const VulkanRenderTarget* rt = resource_mgr_.GetRenderTarget(desc.render_target);
    if (!program || !rt || !rt->has_color ||
        rt->framebuffer == VK_NULL_HANDLE || rt->color_texture.image == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("[Vulkan] ImmediateDraw: invalid program/RT {}", desc.render_target);
        return;
    }

    VkDevice device = context_.device();
    const uint32_t color_count = static_cast<uint32_t>(rt->color_attachment_count > 0 ? rt->color_attachment_count : 1);

    // clear â†’ loadOp=CLEAR render passï¼›å¦åˆ™ loadOp=LOAD ä¿ç•™æ—¢æœ‰å†…å®¹ã€‚
    VkRenderPass render_pass = desc.clear ? rt->render_pass : rt->render_pass_load;
    if (render_pass == VK_NULL_HANDLE) render_pass = rt->render_pass;

    VkPipeline pipeline = GetOrCreateImmediatePipeline(desc, program, render_pass, color_count);
    if (pipeline == VK_NULL_HANDLE) return;

    // é¡¶ç‚¹æ•°æ®ä¸Šä¼ åˆ°ä¸´æ—¶ GPU é¡¶ç‚¹ç¼“å†²ï¼ˆåŒæ­¥æäº¤åŽåˆ é™¤ï¼‰ã€‚
    unsigned int vbo_handle = 0;
    const VulkanBuffer* vbuf = nullptr;
    if (desc.vertices && desc.vertex_bytes > 0) {
        vbo_handle = resource_mgr_.CreateBuffer(desc.vertex_bytes, desc.vertices, true, false);
        vbuf = resource_mgr_.GetBuffer(vbo_handle);
        if (!vbuf || vbuf->buffer == VK_NULL_HANDLE) {
            if (vbo_handle) resource_mgr_.DeleteBuffer(vbo_handle);
            return;
        }
    }

    VkCommandBuffer cmd = resource_mgr_.BeginSingleTimeCommands();

    // RT é¢œè‰²å›¾åƒé™æ¯æ€ä¸º SHADER_READ_ONLYï¼ˆä¸Žå›žè¯»çº¦å®šä¸€è‡´ï¼‰â†’ è½¬ COLOR_ATTACHMENT_OPTIMALï¼Œ
    // åŒ¹é… render pass çš„ initialLayoutã€‚
    VkImageMemoryBarrier to_color{};
    to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_color.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_color.image = rt->color_texture.image;
    to_color.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    to_color.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_color);

    VkClearValue clear_values[2]{};
    clear_values[0].color = {{desc.clear_color.r, desc.clear_color.g, desc.clear_color.b, desc.clear_color.a}};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpbi{};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = render_pass;
    rpbi.framebuffer = rt->framebuffer;
    rpbi.renderArea.offset = {0, 0};
    rpbi.renderArea.extent = {static_cast<uint32_t>(rt->width), static_cast<uint32_t>(rt->height)};
    if (desc.clear) {
        rpbi.clearValueCount = rt->has_depth ? 2u : 1u;
        rpbi.pClearValues = clear_values;
    }
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // viewportï¼š0,0,0,0 â†’ RT å…¨å°ºå¯¸
    const bool full_vp = (desc.viewport.z == 0 || desc.viewport.w == 0);
    VkViewport vp{};
    vp.x = full_vp ? 0.0f : static_cast<float>(desc.viewport.x);
    vp.y = full_vp ? 0.0f : static_cast<float>(desc.viewport.y);
    vp.width = full_vp ? static_cast<float>(rt->width) : static_cast<float>(desc.viewport.z);
    vp.height = full_vp ? static_cast<float>(rt->height) : static_cast<float>(desc.viewport.w);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = { static_cast<int32_t>(vp.x), static_cast<int32_t>(vp.y) };
    scissor.extent = { static_cast<uint32_t>(vp.width), static_cast<uint32_t>(vp.height) };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // uniform â†’ push constantï¼šæŒ‰åå°„çš„æˆå‘˜ååç§»æ‰“åŒ…ï¼ˆä¸Ž DX11 cbuffer åå°„å¯¹ä½ï¼‰ã€‚
    if (program->reflection.has_push_constant && !program->push_constant_member_offsets.empty()) {
        std::vector<uint8_t> pc(program->reflection.push_constant_range.size, 0);
        auto write_uniform = [&](const std::string& name, const void* src, size_t size) {
            auto off_it = program->push_constant_member_offsets.find(name);
            if (off_it == program->push_constant_member_offsets.end()) return;
            uint32_t off = off_it->second;
            if (off + size <= pc.size()) std::memcpy(pc.data() + off, src, size);
        };
        for (const auto& u : desc.uniforms_f)    write_uniform(u.first, &u.second, sizeof(float));
        for (const auto& u : desc.uniforms_vec2)  write_uniform(u.first, &u.second, sizeof(glm::vec2));
        for (const auto& u : desc.uniforms_vec4)  write_uniform(u.first, &u.second, sizeof(glm::vec4));
        vkCmdPushConstants(cmd, program->pipeline_layout,
                           program->reflection.push_constant_range.stageFlags,
                           0, static_cast<uint32_t>(pc.size()), pc.data());
    }

    if (vbuf) {
        VkBuffer vbs[] = { vbuf->buffer };
        VkDeviceSize offs[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offs);
    }
    vkCmdDraw(cmd, static_cast<uint32_t>(desc.vertex_count), 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    // render pass finalLayout=COLOR_ATTACHMENT_OPTIMAL â†’ è½¬å›ž SHADER_READ_ONLYï¼ˆä¾›å›žè¯» / é‡‡æ ·ï¼‰ã€‚
    VkImageMemoryBarrier to_read{};
    to_read.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_read.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    to_read.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_read.image = rt->color_texture.image;
    to_read.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    to_read.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    to_read.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &to_read);

    resource_mgr_.EndSingleTimeCommands(cmd);  // æäº¤ + ç­‰å¾…å®Œæˆ

    if (vbo_handle) resource_mgr_.DeleteBuffer(vbo_handle);

    current_frame_stats_.draw_calls++;
}

void VulkanRhiDevice::BlitRenderTarget(unsigned int src_rt, unsigned int dst_rt) {
    EnsureInitialized();
    if (!initialized_ || src_rt == dst_rt) return;

    const VulkanRenderTarget* src = resource_mgr_.GetRenderTarget(src_rt);
    const VulkanRenderTarget* dst = resource_mgr_.GetRenderTarget(dst_rt);
    if (!src || !dst || !src->has_color || !dst->has_color ||
        src->color_texture.image == VK_NULL_HANDLE || dst->color_texture.image == VK_NULL_HANDLE) {
        DEBUG_LOG_WARN("[Vulkan] BlitRenderTarget: invalid src {} / dst {}", src_rt, dst_rt);
        return;
    }

    VkImage src_image = src->color_texture.image;
    VkImage dst_image = dst->color_texture.image;

    VkCommandBuffer cmd = resource_mgr_.BeginSingleTimeCommands();

    // src/dst é™æ¯æ€å‡ä¸º SHADER_READ_ONLY â†’ TRANSFER_SRC / TRANSFER_DSTã€‚
    VkImageMemoryBarrier barriers[2]{};
    barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barriers[0].image = src_image;
    barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    barriers[1] = barriers[0];
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].image = dst_image;
    barriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[0] = {0, 0, 0};
    region.srcOffsets[1] = {src->width, src->height, 1};
    region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.dstOffsets[0] = {0, 0, 0};
    region.dstOffsets[1] = {dst->width, dst->height, 1};
    // ç­‰å°ºå¯¸ä¼˜å…ˆ NEARESTï¼ˆÂ§5.Bï¼‰ï¼›å°ºå¯¸ä¸åŒæ—¶ä»æ­£ç¡®ç¼©æ”¾ã€‚
    VkFilter filter = (src->width == dst->width && src->height == dst->height)
                          ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    vkCmdBlitImage(cmd,
        src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, filter);

    // è½¬å›ž SHADER_READ_ONLYã€‚
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 2, barriers);

    resource_mgr_.EndSingleTimeCommands(cmd);
}

void VulkanRhiDevice::TransitionRenderTarget(unsigned int rt_handle,
                                              ResourceState from, ResourceState to) {
    if (from == to) return;

    const auto* rt = resource_mgr_.GetRenderTarget(rt_handle);
    if (!rt) return;

    // ç¡®å®šè¦è½¬æ¢çš„ VkImage å’Œ aspect mask
    VkImage image = VK_NULL_HANDLE;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    bool is_depth_transition = (from == ResourceState::DepthWrite || from == ResourceState::DepthRead ||
                                to == ResourceState::DepthWrite || to == ResourceState::DepthRead);
    if (is_depth_transition && rt->has_depth && rt->depth_texture.image != VK_NULL_HANDLE) {
        image = rt->depth_texture.image;
        // VUID-VkImageMemoryBarrier-image-03320: D24S8/D32S8 å¿…é¡»åŒæ—¶å£°æ˜Ž DEPTH+STENCIL aspect
        const VkFormat fmt = rt->depth_texture.format;
        const bool has_stencil = (fmt == VK_FORMAT_D24_UNORM_S8_UINT ||
                                  fmt == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                  fmt == VK_FORMAT_D16_UNORM_S8_UINT);
        aspect = has_stencil ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                             : VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (rt->has_color && rt->color_texture.image != VK_NULL_HANDLE) {
        image = rt->color_texture.image;
    } else {
        return;
    }

    auto src = MapResourceState(from);
    auto dst = MapResourceState(to);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = src.layout;
    barrier.newLayout = dst.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    barrier.srcAccessMask = src.access;
    barrier.dstAccessMask = dst.access;

    // ä¼˜å…ˆä½¿ç”¨æ´»è·ƒæ¸²æŸ“å‘½ä»¤ç¼“å†²ï¼›ä¸å¯ç”¨æ—¶èµ° single-time å‘½ä»¤
    VkCommandBuffer cmd = active_render_cmd_;
    if (cmd != VK_NULL_HANDLE) {
        vkCmdPipelineBarrier(cmd, src.stage, dst.stage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    } else {
        VkCommandBuffer one_shot = resource_mgr_.BeginSingleTimeCommands();
        vkCmdPipelineBarrier(one_shot, src.stage, dst.stage,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        resource_mgr_.EndSingleTimeCommands(one_shot);
    }
}

void VulkanRhiDevice::ComputeMemoryBarrier() {
    if (!in_compute_pass_ || compute_cmd_buffer_ == VK_NULL_HANDLE) return;

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(compute_cmd_buffer_,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VulkanRhiDevice::SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) {
    pending_compute_images_[binding] = { texture_handle, read_only, -1, false };
}

void VulkanRhiDevice::SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                                 int mip_level, bool read_only, bool r32f) {
    pending_compute_images_[binding] = { texture_handle, read_only, mip_level, r32f };
}

void VulkanRhiDevice::SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) {
    pending_compute_samplers_[unit] = texture_handle;
}

unsigned int VulkanRhiDevice::CreateHiZTexture(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return 0;
    if (!hiz_impl_) hiz_impl_ = std::make_unique<HiZImpl>();

    VkDevice device = context_.device();

    int mip_count = 1;
    {
        int w = width, h = height;
        while (w > 1 || h > 1) {
            w = (std::max)(1, w / 2);
            h = (std::max)(1, h / 2);
            ++mip_count;
        }
    }

    // åˆ›å»º VkImageï¼ˆR32_SFLOATï¼Œå®Œæ•´ mip chainï¼‰
    VkImageCreateInfo img_ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    img_ci.imageType = VK_IMAGE_TYPE_2D;
    img_ci.format = VK_FORMAT_R32_SFLOAT;
    img_ci.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    img_ci.mipLevels = static_cast<uint32_t>(mip_count);
    img_ci.arrayLayers = 1;
    img_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    img_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    HiZImpl::HiZTextureInfo info{};
    info.width = width;
    info.height = height;
    info.mip_count = mip_count;

    if (vkCreateImage(device, &img_ci, nullptr, &info.image) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create Hi-Z image");
        return 0;
    }

    VkMemoryRequirements mem_reqs;
    vkGetImageMemoryRequirements(device, info.image, &mem_reqs);
    VkMemoryAllocateInfo alloc_ci{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_ci.allocationSize = mem_reqs.size;
    alloc_ci.memoryTypeIndex = resource_mgr_.FindMemoryType(
        mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &alloc_ci, nullptr, &info.memory) != VK_SUCCESS) {
        vkDestroyImage(device, info.image, nullptr);
        DEBUG_LOG_ERROR("[Vulkan] Failed to allocate Hi-Z memory");
        return 0;
    }
    vkBindImageMemory(device, info.image, info.memory, 0);

    // Layout transition: UNDEFINED â†’ GENERALï¼ˆæ‰€æœ‰ mip çº§åˆ«ï¼‰
    {
        VkCommandBuffer cmd = resource_mgr_.BeginSingleTimeCommands();
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = info.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = static_cast<uint32_t>(mip_count);
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        resource_mgr_.EndSingleTimeCommands(cmd);
    }

    // å…¨ mip viewï¼ˆç”¨äºŽé‡‡æ ·ï¼‰
    VkImageViewCreateInfo view_ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_ci.image = info.image;
    view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format = VK_FORMAT_R32_SFLOAT;
    view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.baseMipLevel = 0;
    view_ci.subresourceRange.levelCount = static_cast<uint32_t>(mip_count);
    view_ci.subresourceRange.layerCount = 1;
    vkCreateImageView(device, &view_ci, nullptr, &info.full_view);

    // æ¯ä¸ª mip level ä¸€ä¸ª viewï¼ˆç”¨äºŽ compute storage image å†™å…¥ï¼‰
    info.mip_views.resize(mip_count);
    for (int i = 0; i < mip_count; ++i) {
        VkImageViewCreateInfo mip_view_ci = view_ci;
        mip_view_ci.subresourceRange.baseMipLevel = static_cast<uint32_t>(i);
        mip_view_ci.subresourceRange.levelCount = 1;
        vkCreateImageView(device, &mip_view_ci, nullptr, &info.mip_views[i]);
    }

    // æ³¨å†Œä¸ºçº¹ç†èµ„æºï¼ˆä¾› GetHiZGpuTexture é€šè¿‡ handle è¿”å›žï¼‰
    // ä½¿ç”¨ resource_mgr_ çš„ compute write texture åˆ›å»ºæ–¹å¼ç®€åŒ–
    // è¿™é‡Œç›´æŽ¥è¿”å›žä¸€ä¸ªè‡ªç®¡ç† handle
    unsigned int handle = hiz_impl_->next_handle++;
    info.texture_handle = handle;
    hiz_impl_->textures[handle] = std::move(info);

    DEBUG_LOG_INFO("[Vulkan] Hi-Z texture created: handle={} {}x{} mips={}", handle, width, height, mip_count);
    return handle;
}

void VulkanRhiDevice::DeleteHiZTexture(unsigned int handle) {
    if (!hiz_impl_) return;
    auto it = hiz_impl_->textures.find(handle);
    if (it == hiz_impl_->textures.end()) return;

    VkDevice device = context_.device();
    auto& info = it->second;
    for (auto& mv : info.mip_views) {
        if (mv) vkDestroyImageView(device, mv, nullptr);
    }
    if (info.full_view) vkDestroyImageView(device, info.full_view, nullptr);
    if (info.image) vkDestroyImage(device, info.image, nullptr);
    if (info.memory) vkFreeMemory(device, info.memory, nullptr);
    hiz_impl_->textures.erase(it);
}

int VulkanRhiDevice::GetHiZMipCount(unsigned int handle) const {
    if (!hiz_impl_) return 0;
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? it->second.mip_count : 0;
}

unsigned int VulkanRhiDevice::GetHiZGpuTexture(unsigned int handle) const {
    if (!hiz_impl_) return 0;
    auto it = hiz_impl_->textures.find(handle);
    return it != hiz_impl_->textures.end() ? handle : 0;
}

static void EnsurePushConstantCapacity(std::vector<uint8_t>& buf, size_t offset, size_t write_size) {
    size_t needed = offset + write_size;
    if (buf.size() < needed) buf.resize(needed, 0);
}

size_t VulkanRhiDevice::GetOrCreateUniformOffset(unsigned int shader, const char* name, size_t data_size) {
    auto& layout = compute_uniform_layouts_[shader];
    auto it = layout.name_to_offset.find(name);
    if (it != layout.name_to_offset.end()) {
        return it->second;
    }
    // 16-byte å¯¹é½ï¼ˆVulkan push constant å¸ƒå±€è¦æ±‚ï¼‰
    size_t offset = (compute_uniform_next_offset_ + 15) & ~size_t(15);
    layout.name_to_offset[name] = offset;
    compute_uniform_next_offset_ = offset + data_size;
    return offset;
}

void VulkanRhiDevice::SetComputeUniformInt(unsigned int shader, const char* name, int value) {
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(int));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(int));
    memcpy(compute_push_constants_.data() + offset, &value, sizeof(int));
}
void VulkanRhiDevice::SetComputeUniformFloat(unsigned int shader, const char* name, float value) {
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(float));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(float));
    memcpy(compute_push_constants_.data() + offset, &value, sizeof(float));
}
void VulkanRhiDevice::SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) {
    int data[2] = { x, y };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) {
    float data[2] = { x, y };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) {
    float data[3] = { x, y, z };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) {
    int data[3] = { x, y, z };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) {
    float data[4] = { x, y, z, w };
    size_t offset = GetOrCreateUniformOffset(shader, name, sizeof(data));
    EnsurePushConstantCapacity(compute_push_constants_, offset, sizeof(data));
    memcpy(compute_push_constants_.data() + offset, data, sizeof(data));
}
void VulkanRhiDevice::SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) {
    size_t offset = GetOrCreateUniformOffset(shader, name, 64);
    EnsurePushConstantCapacity(compute_push_constants_, offset, 64);
    memcpy(compute_push_constants_.data() + offset, data, 64);
}
unsigned int VulkanRhiDevice::CreateComputeShaderEx(

    const std::string& /*gl_src*/, const std::string& vk_src, const std::string& /*hlsl_src*/,
    uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count,
    uint32_t push_constant_bytes, const std::string& /*wgsl_src*/) {
    if (!initialized_) return 0u;
    if (ssbo_count == 0 && storage_image_count == 0 && sampler_count == 0)
        return shader_mgr_.CreateComputeProgramSSBO(vk_src, 0, push_constant_bytes);
    return shader_mgr_.CreateComputeProgramFull(
        vk_src, ssbo_count, storage_image_count, sampler_count, push_constant_bytes);
}

unsigned int VulkanRhiDevice::CreateComputeWriteTexture2D(int width, int height) {
    if (!initialized_) return 0;
    return resource_mgr_.CreateComputeWriteTexture2D(width, height);
}

} // namespace render
} // namespace dse


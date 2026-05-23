/**
 * @file vulkan_pipeline_state_manager.cpp
 * @brief Vulkan 管线状态管理器实现
 *
 * 包含：
 * - PipelineStateDesc → VkGraphicsPipelineCreateInfo 转换
 * - VkPipeline 创建与缓存
 * - VkRenderPass 创建与缓存
 * - BlendFactor/CompareFunc/CullFace → Vulkan 枚举映射
 */

#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

// ============================================================================
// 枚举映射
// ============================================================================

VkBlendFactor VulkanPipelineStateManager::ToVkBlendFactor(BlendFactor factor) {
    switch (factor) {
    case BlendFactor::Zero:            return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:             return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcColor:        return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstColor:        return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::SrcAlpha:        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha:        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:                           return VK_BLEND_FACTOR_ONE;
    }
}

VkCompareOp VulkanPipelineStateManager::ToVkCompareOp(CompareFunc func) {
    switch (func) {
    case CompareFunc::Never:        return VK_COMPARE_OP_NEVER;
    case CompareFunc::Less:         return VK_COMPARE_OP_LESS;
    case CompareFunc::Equal:        return VK_COMPARE_OP_EQUAL;
    case CompareFunc::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareFunc::Greater:      return VK_COMPARE_OP_GREATER;
    case CompareFunc::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
    case CompareFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareFunc::Always:       return VK_COMPARE_OP_ALWAYS;
    default:                        return VK_COMPARE_OP_LESS;
    }
}

VkCullModeFlagBits VulkanPipelineStateManager::ToVkCullMode(CullFace face) {
    switch (face) {
    case CullFace::None:  return VK_CULL_MODE_NONE;
    case CullFace::Front: return VK_CULL_MODE_FRONT_BIT;
    case CullFace::Back:  return VK_CULL_MODE_BACK_BIT;
    default:              return VK_CULL_MODE_NONE;
    }
}

VkFrontFace VulkanPipelineStateManager::ToVkFrontFace() {
    // 投影修正矩阵的 Y-flip 与 Vulkan viewport Y 方向互相抵消，
    // 帧缓冲中三角形绕序仍为 CCW（与 OpenGL 一致）
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

// ============================================================================
// Init / Shutdown
// ============================================================================

void VulkanPipelineStateManager::Init(VulkanContext* context,
                                       VulkanShaderManager* shader_mgr) {
    context_ = context;
    shader_mgr_ = shader_mgr;
    DEBUG_LOG_INFO("VulkanPipelineStateManager initialized");
}

void VulkanPipelineStateManager::Shutdown() {
    auto device = context_->device();

    // 销毁所有缓存的 VkPipeline（复合键缓存）
    for (auto& [key, pipeline] : pipeline_cache_) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
    }
    pipeline_cache_.clear();
    pipeline_states_.clear();

    // 销毁 RenderPass 缓存
    for (auto& [key, rp] : render_pass_cache_) {
        if (rp != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, rp, nullptr);
        }
    }
    render_pass_cache_.clear();

    DEBUG_LOG_INFO("VulkanPipelineStateManager shutdown, {} states remaining",
                  pipeline_states_.size());
}

// ============================================================================
// CreatePipelineState
// ============================================================================

unsigned int VulkanPipelineStateManager::CreatePipelineState(const PipelineStateDesc& desc) {
    unsigned int handle = next_handle_++;

    VulkanPipelineState state;
    state.desc = desc;
    // VkPipeline 尚未创建 — 需要在绑定着色器和顶点格式后延迟创建
    // Vulkan 的 VkPipeline 必须关联 shader + vertex input + render pass
    // 此处仅存储 desc，真正的 pipeline 创建在 Draw 时按需完成
    state.pipeline = VK_NULL_HANDLE;
    state.render_pass = VK_NULL_HANDLE;
    state.pipeline_layout = VK_NULL_HANDLE;
    state.shader_program_handle = 0;

    pipeline_states_[handle] = std::move(state);
    return handle;
}

VkPipeline VulkanPipelineStateManager::GetOrCreateVkPipeline(
    unsigned int handle,
    const VulkanShaderProgram* shader_program,
    VkRenderPass render_pass,
    const std::vector<VkVertexInputBindingDescription>& vertex_bindings,
    const std::vector<VkVertexInputAttributeDescription>& vertex_attributes,
    VkExtent2D extent,
    VkSampleCountFlagBits samples,
    uint32_t color_attachment_count,
    bool wireframe) {

    auto it = pipeline_states_.find(handle);
    if (it == pipeline_states_.end()) return VK_NULL_HANDLE;

    auto& state = it->second;

    // 复合键查找缓存：同一 handle 在不同 renderPass/samples/wireframe/overdraw 下各自有独立的 VkPipeline
    PipelineCacheKey cache_key{ handle, render_pass, samples, wireframe_mode_, overdraw_mode_ };
    auto cache_it = pipeline_cache_.find(cache_key);
    if (cache_it != pipeline_cache_.end()) {
        return cache_it->second;
    }

    if (!shader_program) return VK_NULL_HANDLE;

    auto device = context_->device();

    // --- Shader Stages ---
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = shader_program->vert_module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = shader_program->frag_module;
    stages[1].pName = "main";

    // --- Vertex Input ---
    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = static_cast<uint32_t>(vertex_bindings.size());
    vertex_input.pVertexBindingDescriptions = vertex_bindings.empty() ? nullptr : vertex_bindings.data();
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size());
    vertex_input.pVertexAttributeDescriptions = vertex_attributes.empty() ? nullptr : vertex_attributes.data();

    // --- Input Assembly ---
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // --- Viewport & Scissor ---
    VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, extent };
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // --- Rasterizer ---
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = wireframe_mode_ ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = state.desc.culling_enabled
                             ? ToVkCullMode(state.desc.cull_face)
                             : VK_CULL_MODE_NONE;
    rasterizer.frontFace = ToVkFrontFace();
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    // --- Multisample ---
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.rasterizationSamples = samples;

    // --- Depth Stencil ---
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = state.desc.depth_test_enabled ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = (state.desc.depth_write_enabled && !overdraw_mode_) ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = ToVkCompareOp(state.desc.depth_func);
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    // --- Blend ---
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (overdraw_mode_) {
        blend_attachment.blendEnable = VK_TRUE;
        blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
        blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        blend_attachment.blendEnable = state.desc.blend_enabled ? VK_TRUE : VK_FALSE;
        if (state.desc.blend_enabled) {
            blend_attachment.srcColorBlendFactor = ToVkBlendFactor(state.desc.blend_src);
            blend_attachment.dstColorBlendFactor = ToVkBlendFactor(state.desc.blend_dst);
            blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
            blend_attachment.srcAlphaBlendFactor = ToVkBlendFactor(state.desc.alpha_blend_src);
            blend_attachment.dstAlphaBlendFactor = ToVkBlendFactor(state.desc.alpha_blend_dst);
            blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }
    }

    // VUID-VkGraphicsPipelineCreateInfo-renderPass-07609: pColorBlendState->attachmentCount
    // 必须匹配 subpass.colorAttachmentCount。MRT GBuffer 时 >1。
    // 注：所有 MRT attachment 共用同一个 blend_attachment（GBuffer 都是 opaque write）。
    const uint32_t blend_count = (color_attachment_count > 0) ? color_attachment_count : 1;
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(blend_count, blend_attachment);

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.logicOpEnable = VK_FALSE;
    blend.attachmentCount = blend_count;
    blend.pAttachments = blend_attachments.data();

    // --- Dynamic State ---
    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    // --- 创建 Pipeline ---
    VkGraphicsPipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_ci.stageCount = 2;
    pipeline_ci.pStages = stages;
    pipeline_ci.pVertexInputState = &vertex_input;
    pipeline_ci.pInputAssemblyState = &input_assembly;
    pipeline_ci.pViewportState = &viewport_state;
    pipeline_ci.pRasterizationState = &rasterizer;
    pipeline_ci.pMultisampleState = &multisample;
    pipeline_ci.pDepthStencilState = &depth_stencil;
    pipeline_ci.pColorBlendState = &blend;
    pipeline_ci.pDynamicState = &dynamic_state;
    pipeline_ci.layout = shader_program->pipeline_layout;
    pipeline_ci.renderPass = render_pass;
    pipeline_ci.subpass = 0;

    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create graphics pipeline: {}", static_cast<int>(result));
        return VK_NULL_HANDLE;
    }

    // 存入复合键缓存（不销毁旧 pipeline，它可能仍被 command buffer 引用）
    pipeline_cache_[cache_key] = pipeline;

    // 同步更新 state（兼容旧的单 pipeline 查询接口）
    state.pipeline = pipeline;
    state.render_pass = render_pass;
    state.pipeline_layout = shader_program->pipeline_layout;
    state.samples = samples;

    DEBUG_LOG_TRACE("[Vulkan] Created VkPipeline for state handle {}", handle);
    return pipeline;
}

const PipelineStateDesc* VulkanPipelineStateManager::GetPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second.desc : nullptr;
}

const VulkanPipelineState* VulkanPipelineStateManager::GetVulkanPipelineState(
    unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

// ============================================================================
// VkRenderPass 创建与缓存
// ============================================================================

size_t VulkanPipelineStateManager::RenderPassKeyHash::operator()(
    const RenderPassKey& k) const {
    size_t h = 0;
    h ^= std::hash<bool>()(k.has_color) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>()(k.has_depth) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>()(k.color_clear) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>()(k.depth_clear) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

VkRenderPass VulkanPipelineStateManager::GetOrCreateRenderPass(
    const RenderPassKey& key) {

    auto it = render_pass_cache_.find(key);
    if (it != render_pass_cache_.end()) {
        return it->second;
    }

    // 附件描述
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_refs;
    VkAttachmentReference depth_ref{};

    // 颜色附件
    if (key.has_color) {
        VkAttachmentDescription color{};
        color.format = VK_FORMAT_B8G8R8A8_UNORM;  // 默认 swapchain 格式
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = key.color_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                          VK_ATTACHMENT_LOAD_OP_LOAD;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments.push_back(color);

        VkAttachmentReference color_ref{};
        color_ref.attachment = static_cast<uint32_t>(attachments.size() - 1);
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(color_ref);
    }

    // 深度附件
    if (key.has_depth) {
        VkAttachmentDescription depth{};
        depth.format = VK_FORMAT_D24_UNORM_S8_UINT;  // 默认深度格式
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = key.depth_clear ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                          VK_ATTACHMENT_LOAD_OP_LOAD;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depth);

        depth_ref.attachment = static_cast<uint32_t>(attachments.size() - 1);
        depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(color_refs.size());
    subpass.pColorAttachments = color_refs.empty() ? nullptr : color_refs.data();
    subpass.pDepthStencilAttachment = key.has_depth ? &depth_ref : nullptr;

    // Subpass dependency（外部 → subpass 0）
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = static_cast<uint32_t>(attachments.size());
    ci.pAttachments = attachments.data();
    ci.subpassCount = 1;
    ci.pSubpasses = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies = &dependency;

    VkRenderPass render_pass;
    if (vkCreateRenderPass(context_->device(), &ci, nullptr, &render_pass) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("Failed to create VkRenderPass");
        return VK_NULL_HANDLE;
    }

    render_pass_cache_[key] = render_pass;
    return render_pass;
}

} // namespace render
} // namespace dse

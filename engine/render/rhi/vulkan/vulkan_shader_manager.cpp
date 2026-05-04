/**
* @file vulkan_shader_manager.cpp
* @brief Vulkan 着色器管理器实现
*
* 包含：
* - GLSL → SPIR-V 运行时编译（glslang）
* - SPIR-V 反射（descriptor bindings、push constants）
* - DescriptorSetLayout / PipelineLayout 缓存
* - 内置 PBR/天空盒/粒子着色器初始化
*/

#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_shader_sources.h"
#include "engine/base/debug.h"

// glslang 运行时编译支持
#ifdef DSE_HAS_GLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/spirv.hpp>
#endif

namespace dse {
namespace render {

// ============================================================================
// DescriptorLayoutKeyHash
// ============================================================================

size_t VulkanShaderManager::DescriptorLayoutKeyHash::operator()(
    const DescriptorLayoutKey& k) const {
    size_t h = std::hash<uint32_t>()(k.set_index);
    for (const auto& b : k.bindings) {
        h ^= std::hash<uint32_t>()(b.binding) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<uint32_t>()(static_cast<uint32_t>(b.type)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

// ============================================================================
// Init / Shutdown
// ============================================================================

void VulkanShaderManager::Init(VulkanContext* context) {
    context_ = context;

#ifdef DSE_HAS_GLSLANG
    // 初始化 glslang 进程级资源（只需一次）
    static bool glslang_initialized = false;
    if (!glslang_initialized) {
        glslang::InitializeProcess();
        glslang_initialized = true;
    }
#endif
    DEBUG_LOG_INFO("VulkanShaderManager initialized");
}

void VulkanShaderManager::Shutdown() {
    auto device = context_->device();

    // 销毁所有着色器程序
    for (auto& [handle, program] : programs_) {
        if (program.pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, program.pipeline_layout, nullptr);
        }
        for (auto layout : program.descriptor_set_layouts) {
            // 可能被缓存共享，但 Shutdown 时统一销毁更安全
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
        if (program.vert_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, program.vert_module, nullptr);
        }
        if (program.frag_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, program.frag_module, nullptr);
        }
    }
    programs_.clear();

    // descriptor layout 缓存中的对象已在 program 销毁时清理
    descriptor_layout_cache_.clear();

    DEBUG_LOG_INFO("VulkanShaderManager shutdown: {} programs created, {} destroyed",
                  programs_created_, programs_destroyed_);
}

// ============================================================================
// GLSL → SPIR-V 编译
// ============================================================================

bool VulkanShaderManager::CompileGlslToSpirv(
    const std::string& source,
    VkShaderStageFlagBits stage,
    std::vector<uint32_t>& out_spirv) {
#ifdef DSE_HAS_GLSLANG
    EShLanguage glslang_stage;
    switch (stage) {
    case VK_SHADER_STAGE_VERTEX_BIT:   glslang_stage = EShLangVertex; break;
    case VK_SHADER_STAGE_FRAGMENT_BIT: glslang_stage = EShLangFragment; break;
    default:
        DEBUG_LOG_WARN("Unsupported shader stage: {}", static_cast<int>(stage));
        return false;
    }

    glslang::TShader shader(glslang_stage);

    // 设置 Vulkan SPIR-V 目标
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    shader.setEnvInput(glslang::EShSourceGlsl, glslang_stage,
                       glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);

    const char* source_ptr = source.c_str();
    shader.setStrings(&source_ptr, 1);

    TBuiltInResource resources = *GetDefaultResources();
    EShMessages messages = static_cast<EShMessages>(
        EShMsgSpvRules | EShMsgVulkanRules);

    if (!shader.parse(&resources, 450, false, messages)) {
        DEBUG_LOG_ERROR("GLSL parse failed (stage={}):\n{}",
                      glslang_stage == EShLangVertex ? "VS" : "FS",
                      shader.getInfoLog());
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages)) {
        DEBUG_LOG_ERROR("GLSL link failed:\n{}", program.getInfoLog());
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(glslang_stage), out_spirv);
    return true;
#else
    DEBUG_LOG_WARN("glslang not available, cannot compile GLSL to SPIR-V");
    (void)source; (void)stage; (void)out_spirv;
    return false;
#endif
}

// ============================================================================
// VkShaderModule 创建
// ============================================================================

VkShaderModule VulkanShaderManager::CreateShaderModule(const std::vector<uint32_t>& spirv) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv.size() * sizeof(uint32_t);
    ci.pCode = spirv.data();

    VkShaderModule module;
    if (vkCreateShaderModule(context_->device(), &ci, nullptr, &module) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("Failed to create VkShaderModule");
        return VK_NULL_HANDLE;
    }
    return module;
}

// ============================================================================
// SPIR-V 反射
// ============================================================================

bool VulkanShaderManager::ReflectSpirv(
    const std::vector<uint32_t>& vert_spirv,
    const std::vector<uint32_t>& frag_spirv,
    ShaderReflection& out_reflection) {
    out_reflection = {};

    auto reflect_one = [&](const std::vector<uint32_t>& spirv,
                           VkShaderStageFlagBits stage) {
        // 简化版反射：遍历 SPIR-V 指令提取 OpDescriptorSet 和 OpPushConstant
        const uint32_t* words = spirv.data();
        size_t word_count = spirv.size();

        if (word_count < 5) return;

        // SPIR-V 头部：magic, version, generator, bound, schema
        uint32_t bound = words[3];
        (void)bound;

        size_t i = 5; // 跳过头部
        while (i < word_count) {
            uint32_t inst_word = words[i];
            uint16_t opcode = static_cast<uint16_t>(inst_word & 0xFFFF);
            uint16_t word_count_inst = static_cast<uint16_t>(inst_word >> 16);

            if (word_count_inst == 0) break; // 防止无限循环

            // OpDecorate (71)
            if (opcode == 71 && word_count_inst >= 4) {
                uint32_t target_id = words[i + 1];
                uint32_t decoration = words[i + 2];

                // Decoration::DescriptorSet (34)
                if (decoration == 34 && word_count_inst >= 4) {
                    uint32_t set = words[i + 3];
                    // 暂存，后续与 binding 配对
                    // 实际反射需要完整的 ID→binding 映射
                    (void)target_id; (void)set;
                }
                // Decoration::Binding (33)
                if (decoration == 33 && word_count_inst >= 4) {
                    uint32_t binding = words[i + 3];
                    (void)binding;
                }
                // Decoration::Block (2) — UBO 标记
            }

            // OpTypeStruct (30) — 用于检测 push constant block

            i += word_count_inst;
        }
    };

    reflect_one(vert_spirv, VK_SHADER_STAGE_VERTEX_BIT);
    reflect_one(frag_spirv, VK_SHADER_STAGE_FRAGMENT_BIT);

    // TODO: [CodeBuddy-2026-05-04] 完整的 SPIR-V 反射需要维护 ID→binding 映射表
    // 当前使用简化的 fallback：根据着色器名称和约定的 set/binding 布局
    // 后续应替换为 SPIRV-Reflect 库

    // 约定的 descriptor set 布局（与 GL UBO 布局对齐）：
    // Set 0: PerFrame UBO (binding 0)
    // Set 1: PerScene UBO (binding 0)
    // Set 2: PerMaterial UBO (binding 0) + 纹理采样器 (binding 1-5)
    // Set 3: 逐对象 push constant (model matrix)

    out_reflection.bindings = {
        // Set 0: PerFrame
        {0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1},
        // Set 1: PerScene
        {1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1},
        // Set 2: PerMaterial + 纹理
        {2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1},
        {2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1}, // albedo
        {2, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1}, // normal
        {2, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1}, // metallic-roughness
        {2, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1}, // emissive
        {2, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1}, // occlusion
        // 阴影贴图
        {2, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3}, // shadow_map[3]
        {2, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4}, // spot_shadow_map[4]
    };

    out_reflection.has_push_constant = true;
    out_reflection.push_constant_range = {
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        128  // model matrix (64) + 额外数据空间
    };

    return true;
}

// ============================================================================
// PipelineLayout 创建
// ============================================================================

VkPipelineLayout VulkanShaderManager::CreatePipelineLayout(const ShaderReflection& reflection) {
    // 按 set 分组收集 bindings
    std::map<uint32_t, std::vector<DescriptorBindingInfo>> sets;
    for (const auto& b : reflection.bindings) {
        sets[b.set].push_back(b);
    }

    // 为每个 set 创建 VkDescriptorSetLayout
    std::vector<VkDescriptorSetLayout> set_layouts;
    for (auto& [set_index, bindings] : sets) {
        VkDescriptorSetLayout layout = GetOrCreateDescriptorSetLayout(bindings, set_index);
        if (layout == VK_NULL_HANDLE) {
            DEBUG_LOG_ERROR("Failed to create descriptor set layout for set {}", set_index);
            return VK_NULL_HANDLE;
        }
        set_layouts.push_back(layout);
    }

    // 创建 PipelineLayout
    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    ci.pSetLayouts = set_layouts.data();

    if (reflection.has_push_constant) {
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &reflection.push_constant_range;
    }

    VkPipelineLayout layout;
    if (vkCreatePipelineLayout(context_->device(), &ci, nullptr, &layout) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("Failed to create VkPipelineLayout");
        return VK_NULL_HANDLE;
    }

    return layout;
}

// ============================================================================
// DescriptorSetLayout 缓存
// ============================================================================

VkDescriptorSetLayout VulkanShaderManager::GetOrCreateDescriptorSetLayout(
    const std::vector<DescriptorBindingInfo>& bindings, uint32_t set_index) {

    DescriptorLayoutKey key;
    key.set_index = set_index;
    key.bindings = bindings;

    auto it = descriptor_layout_cache_.find(key);
    if (it != descriptor_layout_cache_.end()) {
        return it->second;
    }

    // 创建 VkDescriptorSetLayoutBinding 数组
    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    for (const auto& b : bindings) {
        VkDescriptorSetLayoutBinding vk_b{};
        vk_b.binding = b.binding;
        vk_b.descriptorType = b.type;
        vk_b.descriptorCount = b.count;
        vk_b.stageFlags = b.stage_flags;
        vk_b.pImmutableSamplers = nullptr;
        vk_bindings.push_back(vk_b);
    }

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = static_cast<uint32_t>(vk_bindings.size());
    ci.pBindings = vk_bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(context_->device(), &ci, nullptr, &layout) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("Failed to create VkDescriptorSetLayout for set {}", set_index);
        return VK_NULL_HANDLE;
    }

    descriptor_layout_cache_[key] = layout;
    return layout;
}

// ============================================================================
// CreateProgram / DeleteProgram
// ============================================================================

unsigned int VulkanShaderManager::CreateProgram(
    const std::string& vert_src, const std::string& frag_src) {

    // 1. 编译 VS → SPIR-V
    std::vector<uint32_t> vert_spirv;
    if (!CompileGlslToSpirv(vert_src, VK_SHADER_STAGE_VERTEX_BIT, vert_spirv)) {
        DEBUG_LOG_ERROR("Vertex shader compilation failed");
        return 0;
    }

    // 2. 编译 FS → SPIR-V
    std::vector<uint32_t> frag_spirv;
    if (!CompileGlslToSpirv(frag_src, VK_SHADER_STAGE_FRAGMENT_BIT, frag_spirv)) {
        DEBUG_LOG_ERROR("Fragment shader compilation failed");
        return 0;
    }

    // 3. 创建 VkShaderModule
    VkShaderModule vert_module = CreateShaderModule(vert_spirv);
    VkShaderModule frag_module = CreateShaderModule(frag_spirv);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        if (vert_module) vkDestroyShaderModule(context_->device(), vert_module, nullptr);
        if (frag_module) vkDestroyShaderModule(context_->device(), frag_module, nullptr);
        return 0;
    }

    // 4. 反射
    ShaderReflection reflection;
    ReflectSpirv(vert_spirv, frag_spirv, reflection);

    // 5. 创建 PipelineLayout
    VkPipelineLayout pipeline_layout = CreatePipelineLayout(reflection);
    if (pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_->device(), vert_module, nullptr);
        vkDestroyShaderModule(context_->device(), frag_module, nullptr);
        return 0;
    }

    // 6. 注册
    unsigned int handle = next_handle_++;
    VulkanShaderProgram program;
    program.vert_module = vert_module;
    program.frag_module = frag_module;
    program.pipeline_layout = pipeline_layout;
    program.reflection = reflection;

    // 收集 descriptor set layouts
    std::map<uint32_t, std::vector<DescriptorBindingInfo>> sets;
    for (const auto& b : reflection.bindings) {
        sets[b.set].push_back(b);
    }
    for (auto& [set_index, bindings] : sets) {
        VkDescriptorSetLayout layout = GetOrCreateDescriptorSetLayout(bindings, set_index);
        if (layout != VK_NULL_HANDLE) {
            program.descriptor_set_layouts.push_back(layout);
        }
    }

    programs_[handle] = std::move(program);
    programs_created_++;

    DEBUG_LOG_INFO("Created Vulkan shader program #{} (VS+FS)", handle);
    return handle;
}

void VulkanShaderManager::DeleteProgram(unsigned int handle) {
    auto it = programs_.find(handle);
    if (it == programs_.end()) return;

    auto device = context_->device();
    auto& program = it->second;

    if (program.pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, program.pipeline_layout, nullptr);
    }
    // descriptor_set_layouts 由缓存管理，不在此处销毁
    if (program.vert_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, program.vert_module, nullptr);
    }
    if (program.frag_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, program.frag_module, nullptr);
    }

    programs_.erase(it);
    programs_destroyed_++;
}

const VulkanShaderProgram* VulkanShaderManager::GetProgram(unsigned int handle) const {
    auto it = programs_.find(handle);
    return it != programs_.end() ? &it->second : nullptr;
}

// ============================================================================
// 内置着色器
// ============================================================================

void VulkanShaderManager::InitBuiltinPBRShader() {
    pbr_shader_handle_ = CreateProgram(
        vulkan_shaders::kPbrVertex, vulkan_shaders::kPbrFragment);
    if (pbr_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan PBR shader compilation failed");
    } else {
        DEBUG_LOG_INFO("Vulkan PBR shader created: handle={}", pbr_shader_handle_);
    }
}

void VulkanShaderManager::InitSkyboxShader() {
    skybox_shader_handle_ = CreateProgram(
        vulkan_shaders::kSkyboxVertex, vulkan_shaders::kSkyboxFragment);
    if (skybox_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan skybox shader compilation failed");
    } else {
        DEBUG_LOG_INFO("Vulkan skybox shader created: handle={}", skybox_shader_handle_);
    }
}

void VulkanShaderManager::InitParticleShader() {
    particle_shader_handle_ = CreateProgram(
        vulkan_shaders::kParticleVertex, vulkan_shaders::kParticleFragment);
    if (particle_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan particle shader compilation failed");
    } else {
        DEBUG_LOG_INFO("Vulkan particle shader created: handle={}", particle_shader_handle_);
    }
}

} // namespace render
} // namespace dse

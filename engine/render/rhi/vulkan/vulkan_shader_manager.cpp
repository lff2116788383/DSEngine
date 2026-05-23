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
#include "engine/base/debug.h"
#include <algorithm>

// 离线编译生成的 SPIR-V 内嵌头文件
#include "embed/pbr_vert.gen.h"
#include "embed/pbr_frag.gen.h"
#include "embed/skybox_vert.gen.h"
#include "embed/skybox_frag.gen.h"
#include "embed/particle_vert.gen.h"
#include "embed/particle_frag.gen.h"
#include "embed/sprite_vert.gen.h"
#include "embed/sprite_frag.gen.h"
#include "embed/postprocess_vert.gen.h"
#include "embed/postprocess_passthrough_frag.gen.h"
#include "embed/bloom_downsample_comp.gen.h"
#include "embed/bloom_upsample_comp.gen.h"
#include "embed/fxaa_frag.gen.h"
#include "embed/bloom_extract_frag.gen.h"
#include "embed/bloom_composite_frag.gen.h"
#include "embed/ssao_frag.gen.h"
#include "embed/ssao_blur_frag.gen.h"
#include "embed/ssao_apply_frag.gen.h"
#include "embed/contact_shadow_frag.gen.h"
#include "embed/bloom_composite_ssao_frag.gen.h"
#include "embed/bloom_composite_ssao_ae_frag.gen.h"
#include "embed/lum_compute_frag.gen.h"
#include "embed/lum_adapt_frag.gen.h"
#include "embed/tonemapping_frag.gen.h"
#include "embed/color_grading_frag.gen.h"
#include "embed/taa_resolve_frag.gen.h"
#include "embed/dof_frag.gen.h"
#include "embed/motion_blur_frag.gen.h"
#include "embed/ssr_frag.gen.h"
#include "embed/motion_vector_frag.gen.h"
#include "embed/deferred_lighting_frag.gen.h"
#include "embed/edge_detect_frag.gen.h"
#include "embed/volumetric_fog_frag.gen.h"
#include "embed/decal_frag.gen.h"
#include "embed/wboit_composite_frag.gen.h"
#include "embed/water_frag.gen.h"
#include "embed/light_shaft_frag.gen.h"
#include "embed/gbuffer_frag.gen.h"
#include "embed/shadow_vert.gen.h"
#include "embed/shadow_frag.gen.h"
#include "embed/pbr_gpu_driven_vert.gen.h"

// Reflection metadata for automated descriptor layout
#include "embed/pbr_vert_reflect.gen.h"
#include "embed/pbr_frag_reflect.gen.h"
#include "embed/shadow_vert_reflect.gen.h"
#include "embed/shadow_frag_reflect.gen.h"
#include "engine/render/shader_reflection.h"

// glslang 运行时编译支持
#ifdef DSE_HAS_GLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/spirv.hpp>
#endif

// spirv-cross 运行时 SPIR-V 反射（实现在 spirv_cross_embedded.cpp）
#ifdef DSE_HAS_SPIRV_CROSS
namespace dse { namespace render { namespace spirv_reflect_impl {
bool ReflectSpirvRuntime(
    const std::vector<uint32_t>& vert_spirv,
    const std::vector<uint32_t>& frag_spirv,
    std::vector<DescriptorBindingInfo>& out_bindings,
    uint32_t& out_push_constant_size);
}}} // namespace
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
        if (program.vert_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, program.vert_module, nullptr);
        }
        if (program.frag_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, program.frag_module, nullptr);
        }
    }
    programs_.clear();

    // 销毁 Compute 程序
    for (auto& [handle, prog] : compute_programs_) {
        if (prog.pipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(device, prog.pipeline, nullptr);
        if (prog.pipeline_layout != VK_NULL_HANDLE)
            vkDestroyPipelineLayout(device, prog.pipeline_layout, nullptr);
        if (prog.descriptor_set_layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        if (prog.comp_module != VK_NULL_HANDLE)
            vkDestroyShaderModule(device, prog.comp_module, nullptr);
    }
    compute_programs_.clear();

    for (auto& [key, layout] : descriptor_layout_cache_) {
        if (layout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }
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
    case VK_SHADER_STAGE_COMPUTE_BIT:  glslang_stage = EShLangCompute;  break;
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
        const char* stage_name = (glslang_stage == EShLangVertex)   ? "VS"
                               : (glslang_stage == EShLangCompute)  ? "CS" : "FS";
        DEBUG_LOG_ERROR("GLSL parse failed (stage={}):\n{}", stage_name, shader.getInfoLog());
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

#ifdef DSE_HAS_SPIRV_CROSS
    // 运行时 SPIR-V 反射：解析实际二进制，自动发现所有 bindings（含 GPU-driven SSBO）
    uint32_t pc_size = 0;
    if (!spirv_reflect_impl::ReflectSpirvRuntime(vert_spirv, frag_spirv,
                                                  out_reflection.bindings, pc_size)) {
        DEBUG_LOG_ERROR("[Vulkan] SPIR-V runtime reflection failed");
        return false;
    }
#else
    // 回退：离线 reflection 数据
    using namespace generated_shaders::reflect;
    shader_reflect::ProgramReflection prog_refl =
        dse::render::MakeProgramReflection(kpbr_vert_reflection, kpbr_frag_reflection);
    std::vector<vk_reflect::DescriptorBinding> vk_bindings;
    vk_reflect::ExtractDescriptorBindings(prog_refl, vk_bindings);
    out_reflection.bindings.clear();
    for (const auto& b : vk_bindings) {
        out_reflection.bindings.push_back({
            b.set, b.binding,
            static_cast<VkDescriptorType>(b.descriptor_type),
            static_cast<VkShaderStageFlags>(b.stage_flags),
            b.count
        });
    }
    uint32_t pc_size = 0;
    { uint32_t f, o; vk_reflect::ExtractPushConstantRange(prog_refl, f, o, pc_size); }
#endif

    // Push constants（保留最小 160B 兼容后处理）
    out_reflection.has_push_constant = true;
    out_reflection.push_constant_range = {
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        std::max(pc_size, uint32_t(160))
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

    // 为每个 set 创建 VkDescriptorSetLayout（包含空洞 set 的空 layout）
    uint32_t max_set = sets.empty() ? 0 : sets.rbegin()->first;
    std::vector<VkDescriptorSetLayout> set_layouts;
    for (uint32_t s = 0; s <= max_set; ++s) {
        auto it = sets.find(s);
        if (it != sets.end()) {
            VkDescriptorSetLayout layout = GetOrCreateDescriptorSetLayout(it->second, s);
            if (layout == VK_NULL_HANDLE) {
                DEBUG_LOG_ERROR("Failed to create descriptor set layout for set {}", s);
                return VK_NULL_HANDLE;
            }
            set_layouts.push_back(layout);
        } else {
            std::vector<DescriptorBindingInfo> empty_bindings;
            VkDescriptorSetLayout empty_layout = GetOrCreateDescriptorSetLayout(empty_bindings, s);
            if (empty_layout == VK_NULL_HANDLE) {
                DEBUG_LOG_ERROR("Failed to create empty descriptor set layout for set {}", s);
                return VK_NULL_HANDLE;
            }
            set_layouts.push_back(empty_layout);
        }
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

    if (!context_ || context_->device() == VK_NULL_HANDLE) return 0;

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
    if (!ReflectSpirv(vert_spirv, frag_spirv, reflection)) {
        vkDestroyShaderModule(context_->device(), vert_module, nullptr);
        vkDestroyShaderModule(context_->device(), frag_module, nullptr);
        return 0;
    }

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

    // 收集 descriptor set layouts（填充空洞 set 的空 layout，与 pipeline layout 对齐）
    std::map<uint32_t, std::vector<DescriptorBindingInfo>> sets;
    for (const auto& b : reflection.bindings) {
        sets[b.set].push_back(b);
    }
    uint32_t max_set = sets.empty() ? 0 : sets.rbegin()->first;
    for (uint32_t s = 0; s <= max_set; ++s) {
        auto it = sets.find(s);
        if (it != sets.end()) {
            VkDescriptorSetLayout layout = GetOrCreateDescriptorSetLayout(it->second, s);
            if (layout != VK_NULL_HANDLE) {
                program.descriptor_set_layouts.push_back(layout);
            }
        } else {
            std::vector<DescriptorBindingInfo> empty_bindings;
            VkDescriptorSetLayout empty_layout = GetOrCreateDescriptorSetLayout(empty_bindings, s);
            if (empty_layout != VK_NULL_HANDLE) {
                program.descriptor_set_layouts.push_back(empty_layout);
            }
        }
    }

    programs_[handle] = std::move(program);
    programs_created_++;

    DEBUG_LOG_INFO("Created Vulkan shader program #{} (VS+FS)", handle);
    return handle;
}

unsigned int VulkanShaderManager::CreateProgramFromSpirv(
    const uint32_t* vert_spv, size_t vert_word_count,
    const uint32_t* frag_spv, size_t frag_word_count) {

    std::vector<uint32_t> vert_spirv(vert_spv, vert_spv + vert_word_count);
    std::vector<uint32_t> frag_spirv(frag_spv, frag_spv + frag_word_count);

    VkShaderModule vert_module = CreateShaderModule(vert_spirv);
    VkShaderModule frag_module = CreateShaderModule(frag_spirv);
    if (vert_module == VK_NULL_HANDLE || frag_module == VK_NULL_HANDLE) {
        if (vert_module) vkDestroyShaderModule(context_->device(), vert_module, nullptr);
        if (frag_module) vkDestroyShaderModule(context_->device(), frag_module, nullptr);
        return 0;
    }

    ShaderReflection reflection;
    if (!ReflectSpirv(vert_spirv, frag_spirv, reflection)) {
        vkDestroyShaderModule(context_->device(), vert_module, nullptr);
        vkDestroyShaderModule(context_->device(), frag_module, nullptr);
        return 0;
    }

    VkPipelineLayout pipeline_layout = CreatePipelineLayout(reflection);
    if (pipeline_layout == VK_NULL_HANDLE) {
        vkDestroyShaderModule(context_->device(), vert_module, nullptr);
        vkDestroyShaderModule(context_->device(), frag_module, nullptr);
        return 0;
    }

    unsigned int handle = next_handle_++;
    VulkanShaderProgram program;
    program.vert_module = vert_module;
    program.frag_module = frag_module;
    program.pipeline_layout = pipeline_layout;
    program.reflection = reflection;

    std::map<uint32_t, std::vector<DescriptorBindingInfo>> sets;
    for (const auto& b : reflection.bindings) {
        sets[b.set].push_back(b);
    }
    uint32_t max_set = sets.empty() ? 0 : sets.rbegin()->first;
    for (uint32_t s = 0; s <= max_set; ++s) {
        auto it = sets.find(s);
        if (it != sets.end()) {
            VkDescriptorSetLayout layout = GetOrCreateDescriptorSetLayout(it->second, s);
            if (layout != VK_NULL_HANDLE) {
                program.descriptor_set_layouts.push_back(layout);
            }
        } else {
            std::vector<DescriptorBindingInfo> empty_bindings;
            VkDescriptorSetLayout empty_layout = GetOrCreateDescriptorSetLayout(empty_bindings, s);
            if (empty_layout != VK_NULL_HANDLE) {
                program.descriptor_set_layouts.push_back(empty_layout);
            }
        }
    }

    programs_[handle] = std::move(program);
    programs_created_++;

    DEBUG_LOG_INFO("Created Vulkan shader program #{} (pre-compiled SPIR-V)", handle);
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
    using namespace dse::render::generated_shaders;
    pbr_shader_handle_ = CreateProgramFromSpirv(
        kpbr_vert_spv, kpbr_vert_spv_size,
        kpbr_frag_spv, kpbr_frag_spv_size);
    if (pbr_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan PBR shader creation failed (pre-compiled SPIR-V)");
    } else {
        DEBUG_LOG_INFO("Vulkan PBR shader created: handle={}", pbr_shader_handle_);
    }
}

void VulkanShaderManager::InitSkyboxShader() {
    using namespace dse::render::generated_shaders;
    skybox_shader_handle_ = CreateProgramFromSpirv(
        kskybox_vert_spv, kskybox_vert_spv_size,
        kskybox_frag_spv, kskybox_frag_spv_size);
    if (skybox_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan skybox shader creation failed (pre-compiled SPIR-V)");
    } else {
        DEBUG_LOG_INFO("Vulkan skybox shader created: handle={}", skybox_shader_handle_);
    }
}

void VulkanShaderManager::InitParticleShader() {
    using namespace dse::render::generated_shaders;
    particle_shader_handle_ = CreateProgramFromSpirv(
        kparticle_vert_spv, kparticle_vert_spv_size,
        kparticle_frag_spv, kparticle_frag_spv_size);
    if (particle_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan particle shader creation failed (pre-compiled SPIR-V)");
    } else {
        DEBUG_LOG_INFO("Vulkan particle shader created: handle={}", particle_shader_handle_);
    }
}

void VulkanShaderManager::InitSpriteShader() {
    using namespace dse::render::generated_shaders;
    sprite_shader_handle_ = CreateProgramFromSpirv(
        ksprite_vert_spv, ksprite_vert_spv_size,
        ksprite_frag_spv, ksprite_frag_spv_size);
    if (sprite_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan sprite shader creation failed (pre-compiled SPIR-V)");
    } else {
        DEBUG_LOG_INFO("Vulkan sprite shader created: handle={}", sprite_shader_handle_);
    }
}

void VulkanShaderManager::InitShadowShader() {
    using namespace dse::render::generated_shaders;
    shadow_shader_handle_ = CreateProgramFromSpirv(
        kshadow_vert_spv, kshadow_vert_spv_size,
        kshadow_frag_spv, kshadow_frag_spv_size);
    if (shadow_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan shadow shader creation failed (pre-compiled SPIR-V)");
    } else {
        DEBUG_LOG_INFO("Vulkan shadow shader created: handle={}", shadow_shader_handle_);
    }
}

void VulkanShaderManager::InitPostProcessShader() {
    using namespace dse::render::generated_shaders;
    postprocess_shader_handle_ = CreateProgramFromSpirv(
        kpostprocess_vert_spv, kpostprocess_vert_spv_size,
        kpostprocess_passthrough_frag_spv, kpostprocess_passthrough_frag_spv_size);
    if (postprocess_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("Vulkan post-process shader creation failed (pre-compiled SPIR-V)");
    } else {
        DEBUG_LOG_INFO("Vulkan post-process shader created: handle={}", postprocess_shader_handle_);
    }

    // Bloom Extract shader（预编译 SPIR-V from gen.h）
    {
        bloom_extract_shader_handle_ = CreateProgramFromSpirv(
            generated_shaders::kpostprocess_vert_spv, generated_shaders::kpostprocess_vert_spv_size,
            generated_shaders::kbloom_extract_frag_spv, generated_shaders::kbloom_extract_frag_spv_size);
        if (bloom_extract_shader_handle_)
            DEBUG_LOG_INFO("[Vulkan] BloomExtract shader created (gen.h SPIR-V): handle={}", bloom_extract_shader_handle_);
        else
            DEBUG_LOG_WARN("[Vulkan] BloomExtract shader creation failed");
    }

    // FXAA shader（预编译 SPIR-V from gen.h）
    {
        fxaa_shader_handle_ = CreateProgramFromSpirv(
            generated_shaders::kpostprocess_vert_spv, generated_shaders::kpostprocess_vert_spv_size,
            generated_shaders::kfxaa_frag_spv, generated_shaders::kfxaa_frag_spv_size);
        if (fxaa_shader_handle_)
            DEBUG_LOG_INFO("[Vulkan] FXAA shader created (gen.h SPIR-V): handle={}", fxaa_shader_handle_);
        else
            DEBUG_LOG_WARN("[Vulkan] FXAA shader creation failed");
    }

    // ---- 以下全部使用预编译 SPIR-V (gen.h) ----
    auto create_pp_spv = [&](const uint32_t* frag_spv, size_t frag_size, const char* name) -> unsigned int {
        unsigned int h = CreateProgramFromSpirv(
            generated_shaders::kpostprocess_vert_spv, generated_shaders::kpostprocess_vert_spv_size,
            frag_spv, frag_size);
        if (h)
            DEBUG_LOG_INFO("[Vulkan] {} shader created (gen.h SPIR-V): handle={}", name, h);
        else
            DEBUG_LOG_WARN("[Vulkan] {} shader creation failed", name);
        return h;
    };

    ssao_shader_handle_ = create_pp_spv(generated_shaders::kssao_frag_spv, generated_shaders::kssao_frag_spv_size, "SSAO");
    ssao_blur_shader_handle_ = create_pp_spv(generated_shaders::kssao_blur_frag_spv, generated_shaders::kssao_blur_frag_spv_size, "SSAO Blur");
    contact_shadow_shader_handle_ = create_pp_spv(generated_shaders::kcontact_shadow_frag_spv, generated_shaders::kcontact_shadow_frag_spv_size, "ContactShadow");
    lum_compute_shader_handle_ = create_pp_spv(generated_shaders::klum_compute_frag_spv, generated_shaders::klum_compute_frag_spv_size, "LumCompute");
    lum_adapt_shader_handle_ = create_pp_spv(generated_shaders::klum_adapt_frag_spv, generated_shaders::klum_adapt_frag_spv_size, "LumAdapt");
    tonemapping_shader_handle_ = create_pp_spv(generated_shaders::ktonemapping_frag_spv, generated_shaders::ktonemapping_frag_spv_size, "Tonemapping");
    bloom_composite_ssao_ae_shader_handle_ = create_pp_spv(generated_shaders::kbloom_composite_ssao_ae_frag_spv, generated_shaders::kbloom_composite_ssao_ae_frag_spv_size, "BloomCompositeSsaoAe");
    color_grading_shader_handle_ = create_pp_spv(generated_shaders::kcolor_grading_frag_spv, generated_shaders::kcolor_grading_frag_spv_size, "ColorGrading");
    taa_resolve_shader_handle_ = create_pp_spv(generated_shaders::ktaa_resolve_frag_spv, generated_shaders::ktaa_resolve_frag_spv_size, "TAA Resolve");
    dof_shader_handle_ = create_pp_spv(generated_shaders::kdof_frag_spv, generated_shaders::kdof_frag_spv_size, "DOF");
    motion_blur_shader_handle_ = create_pp_spv(generated_shaders::kmotion_blur_frag_spv, generated_shaders::kmotion_blur_frag_spv_size, "Motion Blur");
    ssr_shader_handle_ = create_pp_spv(generated_shaders::kssr_frag_spv, generated_shaders::kssr_frag_spv_size, "SSR");
    motion_vector_shader_handle_ = create_pp_spv(generated_shaders::kmotion_vector_frag_spv, generated_shaders::kmotion_vector_frag_spv_size, "Motion Vector");

    // GBuffer shader（复用 PBR 顶点着色器 + GBuffer 片段着色器，预编译 SPIR-V）
    {
        gbuffer_shader_handle_ = CreateProgramFromSpirv(
            generated_shaders::kpbr_vert_spv, generated_shaders::kpbr_vert_spv_size,
            generated_shaders::kgbuffer_frag_spv, generated_shaders::kgbuffer_frag_spv_size);
        if (gbuffer_shader_handle_)
            DEBUG_LOG_INFO("[Vulkan] GBuffer shader created (SPIR-V): handle={}", gbuffer_shader_handle_);
        else
            DEBUG_LOG_WARN("[Vulkan] GBuffer shader creation failed");
    }

    deferred_lighting_shader_handle_ = create_pp_spv(generated_shaders::kdeferred_lighting_frag_spv, generated_shaders::kdeferred_lighting_frag_spv_size, "Deferred Lighting");
    edge_detect_shader_handle_ = create_pp_spv(generated_shaders::kedge_detect_frag_spv, generated_shaders::kedge_detect_frag_spv_size, "Edge Detect");
    volumetric_fog_shader_handle_ = create_pp_spv(generated_shaders::kvolumetric_fog_frag_spv, generated_shaders::kvolumetric_fog_frag_spv_size, "Volumetric Fog");
    decal_shader_handle_ = create_pp_spv(generated_shaders::kdecal_frag_spv, generated_shaders::kdecal_frag_spv_size, "Decal");
    wboit_composite_shader_handle_ = create_pp_spv(generated_shaders::kwboit_composite_frag_spv, generated_shaders::kwboit_composite_frag_spv_size, "WBOIT Composite");
    water_shader_handle_ = create_pp_spv(generated_shaders::kwater_frag_spv, generated_shaders::kwater_frag_spv_size, "Water");
    light_shaft_shader_handle_ = create_pp_spv(generated_shaders::klight_shaft_frag_spv, generated_shaders::klight_shaft_frag_spv_size, "Light Shaft");
}

// ============================================================================
// Compute 程序
// ============================================================================

unsigned int VulkanShaderManager::CreateComputeProgram(const std::string& comp_src) {
    std::vector<uint32_t> comp_spirv;
    if (!CompileGlslToSpirv(comp_src, VK_SHADER_STAGE_COMPUTE_BIT, comp_spirv)) {
        DEBUG_LOG_ERROR("[Vulkan] Compute shader compilation failed");
        return 0;
    }

    auto device = context_->device();
    VulkanComputeProgram prog;
    prog.comp_module = CreateShaderModule(comp_spirv);
    if (prog.comp_module == VK_NULL_HANDLE) return 0;

    // Descriptor set layout: binding0 = sampler2D src, binding1 = image2D dst
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 2;
    dsl_ci.pBindings    = bindings;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &prog.descriptor_set_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    // Pipeline layout: descriptor set + push constants (4 floats)
    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc_range.offset     = 0;
    pc_range.size       = sizeof(float) * 4;

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount         = 1;
    pl_ci.pSetLayouts            = &prog.descriptor_set_layout;
    pl_ci.pushConstantRangeCount = 1;
    pl_ci.pPushConstantRanges    = &pc_range;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &prog.pipeline_layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    // Compute pipeline
    VkComputePipelineCreateInfo cp_ci{};
    cp_ci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp_ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cp_ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    cp_ci.stage.module = prog.comp_module;
    cp_ci.stage.pName  = "main";
    cp_ci.layout       = prog.pipeline_layout;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cp_ci, nullptr, &prog.pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, prog.pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    unsigned int handle = next_handle_++;
    compute_programs_[handle] = prog;
    DEBUG_LOG_INFO("[Vulkan] Compute program created: handle={}", handle);
    return handle;
}

unsigned int VulkanShaderManager::CreateComputeProgramSSBO(
    const std::string& comp_src,
    uint32_t ssbo_binding_count,
    uint32_t push_constant_bytes) {

    std::vector<uint32_t> comp_spirv;
    if (!CompileGlslToSpirv(comp_src, VK_SHADER_STAGE_COMPUTE_BIT, comp_spirv)) {
        DEBUG_LOG_ERROR("[Vulkan] SSBO compute shader compilation failed");
        return 0;
    }

    auto device = context_->device();
    VulkanComputeProgram prog;
    prog.comp_module = CreateShaderModule(comp_spirv);
    if (prog.comp_module == VK_NULL_HANDLE) return 0;
    prog.uses_ssbo_bindings = true;
    prog.push_constant_size = push_constant_bytes;
    prog.ssbo_binding_count = ssbo_binding_count;

    // Descriptor set layout: N SSBO bindings (binding 0..N-1)
    std::vector<VkDescriptorSetLayoutBinding> bindings(ssbo_binding_count);
    for (uint32_t i = 0; i < ssbo_binding_count; ++i) {
        bindings[i] = {};
        bindings[i].binding         = i;
        bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = ssbo_binding_count;
    dsl_ci.pBindings    = bindings.data();
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &prog.descriptor_set_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    // Pipeline layout
    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc_range.offset     = 0;
    pc_range.size       = push_constant_bytes > 0 ? push_constant_bytes : 4;

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount         = 1;
    pl_ci.pSetLayouts            = &prog.descriptor_set_layout;
    pl_ci.pushConstantRangeCount = push_constant_bytes > 0 ? 1u : 0u;
    pl_ci.pPushConstantRanges    = push_constant_bytes > 0 ? &pc_range : nullptr;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &prog.pipeline_layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    // Compute pipeline
    VkComputePipelineCreateInfo cp_ci{};
    cp_ci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp_ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cp_ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    cp_ci.stage.module = prog.comp_module;
    cp_ci.stage.pName  = "main";
    cp_ci.layout       = prog.pipeline_layout;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cp_ci, nullptr, &prog.pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, prog.pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    unsigned int handle = next_handle_++;
    compute_programs_[handle] = prog;
    DEBUG_LOG_INFO("[Vulkan] SSBO compute program created: handle={}, ssbo_bindings={}, pc_bytes={}",
                   handle, ssbo_binding_count, push_constant_bytes);
    return handle;
}

unsigned int VulkanShaderManager::CreateComputeProgramFromSpirv(
    const uint32_t* comp_spv, size_t comp_word_count) {

    std::vector<uint32_t> comp_spirv(comp_spv, comp_spv + comp_word_count);

    auto device = context_->device();
    VulkanComputeProgram prog;
    prog.comp_module = CreateShaderModule(comp_spirv);
    if (prog.comp_module == VK_NULL_HANDLE) return 0;

    // Descriptor set layout: binding0 = sampler2D src, binding1 = image2D dst
    VkDescriptorSetLayoutBinding bindings[2] = {};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = 2;
    dsl_ci.pBindings    = bindings;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &prog.descriptor_set_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    // Pipeline layout: descriptor set + push constants (4 floats)
    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc_range.offset     = 0;
    pc_range.size       = sizeof(float) * 4;

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount         = 1;
    pl_ci.pSetLayouts            = &prog.descriptor_set_layout;
    pl_ci.pushConstantRangeCount = 1;
    pl_ci.pPushConstantRanges    = &pc_range;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &prog.pipeline_layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    // Compute pipeline
    VkComputePipelineCreateInfo cp_ci{};
    cp_ci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp_ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cp_ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    cp_ci.stage.module = prog.comp_module;
    cp_ci.stage.pName  = "main";
    cp_ci.layout       = prog.pipeline_layout;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cp_ci, nullptr, &prog.pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, prog.pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    unsigned int handle = next_handle_++;
    compute_programs_[handle] = prog;
    DEBUG_LOG_INFO("[Vulkan] Compute program created (pre-compiled SPIR-V): handle={}", handle);
    return handle;
}

unsigned int VulkanShaderManager::CreateComputeProgramFull(
    const std::string& comp_src,
    uint32_t ssbo_count,
    uint32_t storage_image_count,
    uint32_t sampler_count,
    uint32_t push_constant_bytes) {

    std::vector<uint32_t> comp_spirv;
    if (!CompileGlslToSpirv(comp_src, VK_SHADER_STAGE_COMPUTE_BIT, comp_spirv)) {
        DEBUG_LOG_ERROR("[Vulkan] Full-layout compute shader compilation failed");
        return 0;
    }

    auto device = context_->device();
    VulkanComputeProgram prog;
    prog.comp_module = CreateShaderModule(comp_spirv);
    if (prog.comp_module == VK_NULL_HANDLE) return 0;
    prog.uses_ssbo_bindings   = true;
    prog.push_constant_size   = push_constant_bytes;
    prog.ssbo_binding_count   = ssbo_count;
    prog.storage_image_count  = storage_image_count;
    prog.sampler_count        = sampler_count;

    // 构建 descriptor set layout bindings：
    // binding [0..ssbo_count)            = STORAGE_BUFFER
    // binding [ssbo_count..+img_count)   = STORAGE_IMAGE
    // binding [ssbo_count+img_count..)   = COMBINED_IMAGE_SAMPLER
    uint32_t total_bindings = ssbo_count + storage_image_count + sampler_count;
    std::vector<VkDescriptorSetLayoutBinding> bindings(total_bindings);
    uint32_t bi = 0;
    for (uint32_t i = 0; i < ssbo_count; ++i, ++bi) {
        bindings[bi] = {};
        bindings[bi].binding         = bi;
        bindings[bi].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[bi].descriptorCount = 1;
        bindings[bi].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    for (uint32_t i = 0; i < storage_image_count; ++i, ++bi) {
        bindings[bi] = {};
        bindings[bi].binding         = bi;
        bindings[bi].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[bi].descriptorCount = 1;
        bindings[bi].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    for (uint32_t i = 0; i < sampler_count; ++i, ++bi) {
        bindings[bi] = {};
        bindings[bi].binding         = bi;
        bindings[bi].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[bi].descriptorCount = 1;
        bindings[bi].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo dsl_ci{};
    dsl_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl_ci.bindingCount = total_bindings;
    dsl_ci.pBindings    = total_bindings > 0 ? bindings.data() : nullptr;
    if (vkCreateDescriptorSetLayout(device, &dsl_ci, nullptr, &prog.descriptor_set_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc_range.offset     = 0;
    pc_range.size       = push_constant_bytes > 0 ? push_constant_bytes : 4;

    VkPipelineLayoutCreateInfo pl_ci{};
    pl_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pl_ci.setLayoutCount         = 1;
    pl_ci.pSetLayouts            = &prog.descriptor_set_layout;
    pl_ci.pushConstantRangeCount = push_constant_bytes > 0 ? 1u : 0u;
    pl_ci.pPushConstantRanges    = push_constant_bytes > 0 ? &pc_range : nullptr;
    if (vkCreatePipelineLayout(device, &pl_ci, nullptr, &prog.pipeline_layout) != VK_SUCCESS) {
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    VkComputePipelineCreateInfo cp_ci{};
    cp_ci.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cp_ci.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cp_ci.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    cp_ci.stage.module = prog.comp_module;
    cp_ci.stage.pName  = "main";
    cp_ci.layout       = prog.pipeline_layout;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cp_ci, nullptr, &prog.pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(device, prog.pipeline_layout, nullptr);
        vkDestroyDescriptorSetLayout(device, prog.descriptor_set_layout, nullptr);
        vkDestroyShaderModule(device, prog.comp_module, nullptr);
        return 0;
    }

    unsigned int handle = next_handle_++;
    compute_programs_[handle] = prog;
    DEBUG_LOG_INFO("[Vulkan] Full compute program created: handle={}, ssbo={}, img={}, smp={}, pc={}",
                   handle, ssbo_count, storage_image_count, sampler_count, push_constant_bytes);
    return handle;
}

void VulkanShaderManager::InitBloomComputeShaders() {
    using namespace dse::render::generated_shaders;
    bloom_downsample_cs_handle_ = CreateComputeProgramFromSpirv(
        kbloom_downsample_comp_spv, kbloom_downsample_comp_spv_size);
    bloom_upsample_cs_handle_ = CreateComputeProgramFromSpirv(
        kbloom_upsample_comp_spv, kbloom_upsample_comp_spv_size);
    if (bloom_downsample_cs_handle_ && bloom_upsample_cs_handle_) {
        DEBUG_LOG_INFO("[Vulkan] Bloom CS programs initialized (down={} up={})",
                       bloom_downsample_cs_handle_, bloom_upsample_cs_handle_);
    } else {
        DEBUG_LOG_WARN("[Vulkan] Bloom CS initialization failed");
    }
}

// ============================================================================
// GPU-Driven PBR Shader
// ============================================================================

void VulkanShaderManager::InitGPUDrivenPBRShader() {
#ifndef DSE_HAS_GLSLANG
    DEBUG_LOG_WARN("[Vulkan] GPU-driven PBR shader requires glslang, skipping");
    return;
#else
    using namespace dse::render::generated_shaders;

    auto replace_first = [](std::string& s, const std::string& from, const std::string& to) {
        auto p = s.find(from);
        if (p != std::string::npos) s.replace(p, from.size(), to);
        else DEBUG_LOG_WARN("[VK GPU-driven PBR] patch target not found: {}", from.substr(0, 60));
    };
    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    // --- VS patch: GLSL 430 → 450, gl_InstanceID+BaseInstance → gl_InstanceIndex, add v_material_id ---
    std::string vert_src = kpbr_gpu_driven_vert_glsl430;
    replace_first(vert_src, "#version 430", "#version 450");
    replace_all(vert_src, "#ifdef GL_ARB_shader_draw_parameters\n#extension GL_ARB_shader_draw_parameters : enable\n#endif\n", "");
    replace_all(vert_src, "(gl_InstanceID + SPIRV_Cross_BaseInstance)", "gl_InstanceIndex");
    replace_all(vert_src,
        "#ifdef GL_ARB_shader_draw_parameters\n#define SPIRV_Cross_BaseInstance gl_BaseInstanceARB\n#else\nuniform int SPIRV_Cross_BaseInstance;\n#endif\n",
        "");
    replace_first(vert_src, "layout(binding = 5, std430)", "layout(set = 4, binding = 0, std430)");

    // 声明 flat out v_material_id，传递给 FS
    replace_first(vert_src,
        "layout(location = 0) out vec4 vColor;",
        "flat out uint v_material_id;\nlayout(location = 0) out vec4 vColor;");
    // 在 vColor 赋值后输出 material_id
    replace_first(vert_src,
        "vColor = aColor;\n    vTexCoord = aTexCoord;",
        "vColor = aColor;\n    v_material_id = dse_inst[gl_InstanceIndex].mat_id;\n    vTexCoord = aTexCoord;");

    // --- FS patch: GLSL 430 → 450, PerMaterial UBO → MaterialSSBO (逐 instance 读材质) ---
    std::string frag_src(kpbr_frag_glsl430);
    replace_first(frag_src, "#version 430", "#version 450");
    replace_first(frag_src, "layout(binding = 5, std140) uniform LightProbeData", "layout(set = 1, binding = 5, std140) uniform LightProbeData");
    replace_first(frag_src, "layout(binding = 0, std140) uniform PerScene", "layout(set = 1, binding = 0, std140) uniform PerScene");
    replace_first(frag_src, "layout(binding = 10, std140) uniform SpotLightData", "layout(set = 2, binding = 10, std140) uniform SpotLightData");
    replace_first(frag_src, "layout(binding = 0, std140) uniform PerFrame", "layout(set = 0, binding = 0, std140) uniform PerFrame");
    replace_first(frag_src, "layout(binding = 16, std140) uniform TerrainParams", "layout(set = 2, binding = 16, std140) uniform TerrainParams");
    replace_first(frag_src, "layout(binding = 3, std430) readonly buffer ClusterInfoSSBO", "layout(set = 1, binding = 3, std430) readonly buffer ClusterInfoSSBO");
    replace_first(frag_src, "layout(binding = 4, std430) readonly buffer LightIndexSSBO", "layout(set = 1, binding = 4, std430) readonly buffer LightIndexSSBO");
    replace_first(frag_src, "layout(binding = 1, std430) readonly buffer PointLightSSBO", "layout(set = 1, binding = 1, std430) readonly buffer PointLightSSBO");
    replace_first(frag_src, "layout(binding = 2, std430) readonly buffer SpotLightSSBO", "layout(set = 1, binding = 2, std430) readonly buffer SpotLightSSBO");
    replace_first(frag_src, "layout(binding = 2) uniform sampler2D u_normal_map;", "layout(set = 2, binding = 2) uniform sampler2D u_normal_map;");
    replace_first(frag_src, "layout(binding = 17) uniform samplerCube u_reflection_cubemap;", "layout(set = 2, binding = 17) uniform samplerCube u_reflection_cubemap;");
    replace_first(frag_src, "layout(binding = 18) uniform sampler2D u_brdf_lut;", "layout(set = 2, binding = 18) uniform sampler2D u_brdf_lut;");
    replace_first(frag_src, "layout(binding = 6) uniform sampler2DShadow u_shadow_maps[3];", "layout(set = 2, binding = 6) uniform sampler2DShadow u_shadow_maps[3];");
    replace_first(frag_src, "layout(binding = 7) uniform sampler2D u_spot_shadow_maps[4];", "layout(set = 2, binding = 7) uniform sampler2D u_spot_shadow_maps[4];");
    replace_first(frag_src, "layout(binding = 0) uniform samplerCube u_point_shadow_maps[4];", "layout(set = 3, binding = 0) uniform samplerCube u_point_shadow_maps[4];");
    replace_first(frag_src, "layout(binding = 11) uniform sampler2D u_splat_weight_map;", "layout(set = 2, binding = 11) uniform sampler2D u_splat_weight_map;");
    replace_first(frag_src, "layout(binding = 12) uniform sampler2D u_splat_layer0;", "layout(set = 2, binding = 12) uniform sampler2D u_splat_layer0;");
    replace_first(frag_src, "layout(binding = 13) uniform sampler2D u_splat_layer1;", "layout(set = 2, binding = 13) uniform sampler2D u_splat_layer1;");
    replace_first(frag_src, "layout(binding = 14) uniform sampler2D u_splat_layer2;", "layout(set = 2, binding = 14) uniform sampler2D u_splat_layer2;");
    replace_first(frag_src, "layout(binding = 15) uniform sampler2D u_splat_layer3;", "layout(set = 2, binding = 15) uniform sampler2D u_splat_layer3;");
    replace_first(frag_src, "layout(binding = 1) uniform sampler2D u_texture;", "layout(set = 2, binding = 1) uniform sampler2D u_texture;");
    replace_first(frag_src, "layout(binding = 4) uniform sampler2D u_emissive_map;", "layout(set = 2, binding = 4) uniform sampler2D u_emissive_map;");
    replace_first(frag_src, "layout(binding = 3) uniform sampler2D u_metallic_roughness_map;", "layout(set = 2, binding = 3) uniform sampler2D u_metallic_roughness_map;");
    replace_first(frag_src, "layout(binding = 5) uniform sampler2D u_occlusion_map;", "layout(set = 2, binding = 5) uniform sampler2D u_occlusion_map;");

    const std::string per_mat_ubo =
        "layout(binding = 0, std140) uniform PerMaterial\n"
        "{\n"
        "    vec4 albedo;\n"
        "    vec4 roughness_ao;\n"
        "    vec4 emissive;\n"
        "    vec4 flags;\n"
        "    vec4 extra_params;\n"
        "    vec4 extra_params2;\n"
        "    vec4 toon_shadow_color;\n"
        "    vec4 toon_params;\n"
        "} _1407;";

    const std::string mat_ssbo_replacement =
        "struct DSEGPUMat {\n"
        "    vec4 albedo;\n"
        "    vec4 roughness_ao;\n"
        "    vec4 emissive;\n"
        "    vec4 flags;\n"
        "    vec4 extra_params;\n"
        "    vec4 extra_params2;\n"
        "    vec4 toon_shadow_color;\n"
        "    vec4 toon_params;\n"
        "};\n"
        "layout(set = 2, binding = 9, std430) readonly buffer MaterialSSBO {\n"
        "    DSEGPUMat gpu_materials[];\n"
        "};\n"
        "flat in uint v_material_id;\n"
        "#define _1407 gpu_materials[v_material_id]";

    replace_first(frag_src, per_mat_ubo, mat_ssbo_replacement);

    gpu_driven_pbr_shader_handle_ = CreateProgram(vert_src, frag_src);
    if (gpu_driven_pbr_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("[Vulkan] GPU-driven PBR shader compilation failed");
    } else {
        DEBUG_LOG_INFO("[Vulkan] GPU-driven PBR shader created: handle={}", gpu_driven_pbr_shader_handle_);
    }
#endif
}

// ============================================================================
// GPU-Driven Shadow Shader
// ============================================================================

void VulkanShaderManager::InitGPUDrivenShadowShader() {
#ifndef DSE_HAS_GLSLANG
    DEBUG_LOG_WARN("[Vulkan] GPU-driven Shadow shader requires glslang, skipping");
    return;
#else
    using namespace dse::render::generated_shaders;

    auto replace_first = [](std::string& s, const std::string& from, const std::string& to) {
        auto p = s.find(from);
        if (p != std::string::npos) s.replace(p, from.size(), to);
        else DEBUG_LOG_WARN("[VK GPU-driven Shadow] patch target not found: {}", from.substr(0, 60));
    };
    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    // Shadow VS: 从标准 shadow vert GLSL patch，替换 u_model → SSBO fetch
    std::string vert_src(kshadow_vert_glsl430);
    replace_first(vert_src, "#version 430", "#version 450");

    // 注入 instance SSBO 声明（在 PerFrame UBO 前面）
    const std::string ssbo_decl =
        "struct DSEGPUInst {\n"
        "    mat4 model;\n"
        "    uint mat_id;\n"
        "    uint cmd_id;\n"
        "    uint pad0;\n"
        "    uint pad1;\n"
        "};\n"
        "layout(set = 4, binding = 0, std430) readonly buffer DSEInstBuf {\n"
        "    DSEGPUInst dse_inst[];\n"
        "};\n\n";

    // 在 PerFrame UBO 声明前插入 SSBO
    replace_first(vert_src,
        "layout(binding = 0, std140) uniform PerFrame",
        ssbo_decl + "layout(binding = 0, std140) uniform PerFrame");

    // 替换 u_model → dse_inst[gl_InstanceIndex].model
    replace_all(vert_src, "u_model * localPos",      "dse_inst[gl_InstanceIndex].model * localPos");
    replace_all(vert_src, "u_model * boneTransform", "dse_inst[gl_InstanceIndex].model * boneTransform");
    // 移除 standalone uniforms（Vulkan GLSL 不支持非 opaque standalone uniform）
    replace_all(vert_src, "uniform mat4 u_model;\n", "");
    replace_all(vert_src, "uniform int u_skinned;\n", "const int u_skinned = 0;\n");
    replace_all(vert_src, "uniform int u_morph_enabled;\n", "const int u_morph_enabled = 0;\n");

    std::string frag_src(kshadow_frag_glsl430);
    replace_first(frag_src, "#version 430", "#version 450");
    gpu_driven_shadow_shader_handle_ = CreateProgram(vert_src, frag_src);
    if (gpu_driven_shadow_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("[Vulkan] GPU-driven Shadow shader compilation failed");
    } else {
        DEBUG_LOG_INFO("[Vulkan] GPU-driven Shadow shader created: handle={}", gpu_driven_shadow_shader_handle_);
    }
#endif
}

const VulkanComputeProgram* VulkanShaderManager::GetComputeProgram(unsigned int handle) const {
    auto it = compute_programs_.find(handle);
    return it != compute_programs_.end() ? &it->second : nullptr;
}

} // namespace render
} // namespace dse

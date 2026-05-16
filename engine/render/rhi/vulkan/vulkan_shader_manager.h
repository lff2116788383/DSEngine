/**
 * @file vulkan_shader_manager.h
 * @brief Vulkan 着色器管理器 - SPIR-V 模块编译/反射与 Descriptor Layout 缓存
 *
 * 职责：
 * 1. GLSL 源码 → SPIR-V 运行时编译（通过 glslang）
 * 2. SPIR-V 模块反射：提取 descriptor set layout、push constant range
 * 3. VkDescriptorSetLayout / VkPipelineLayout 缓存与复用
 * 4. 内置 PBR/天空盒/粒子着色器的初始化
 */

#ifndef DSE_RENDER_VULKAN_SHADER_MANAGER_H
#define DSE_RENDER_VULKAN_SHADER_MANAGER_H

#include <vulkan/vulkan.h>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>

namespace dse {
namespace render {

class VulkanContext;

/// Descriptor binding 信息（从 SPIR-V 反射获得）
struct DescriptorBindingInfo {
    uint32_t set = 0;           ///< descriptor set 编号
    uint32_t binding = 0;       ///< binding 编号
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;  ///< descriptor 类型
    VkShaderStageFlags stage_flags = 0;  ///< 可见着色器阶段
    uint32_t count = 1;         ///< 数组大小

    bool operator==(const DescriptorBindingInfo& o) const {
        return set == o.set && binding == o.binding &&
               type == o.type && stage_flags == o.stage_flags &&
               count == o.count;
    }
};

/// 一个着色器程序（VS+FS）的反射结果
struct ShaderReflection {
    std::vector<DescriptorBindingInfo> bindings;
    VkPushConstantRange push_constant_range = {};
    bool has_push_constant = false;
};

/// Compute 着色器程序（Bloom CS）
struct VulkanComputeProgram {
    VkShaderModule comp_module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
};

/// 着色器程序句柄对应的 Vulkan 对象集合
struct VulkanShaderProgram {
    VkShaderModule vert_module = VK_NULL_HANDLE;
    VkShaderModule frag_module = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
    ShaderReflection reflection;
};

/**
 * @class VulkanShaderManager
 * @brief Vulkan 着色器管理器
 *
 * 负责将 GLSL 源码编译为 SPIR-V，创建 VkShaderModule，
 * 通过反射构建 DescriptorSetLayout 和 PipelineLayout。
 */
class VulkanShaderManager {
public:
    VulkanShaderManager() = default;
    ~VulkanShaderManager() = default;

    /// 初始化（需在 VulkanContext 初始化后调用）
    void Init(VulkanContext* context);

    /// 清理所有着色器资源
    void Shutdown();

    /// 从 GLSL 源码创建着色器程序，返回句柄（0 = 失败）
    unsigned int CreateProgram(const std::string& vert_src, const std::string& frag_src);

    /// 从预编译 SPIR-V 创建着色器程序（跳过 glslang 编译）
    unsigned int CreateProgramFromSpirv(
        const uint32_t* vert_spv, size_t vert_word_count,
        const uint32_t* frag_spv, size_t frag_word_count);

    /// 销毁着色器程序
    void DeleteProgram(unsigned int handle);

    /// 查询着色器程序信息
    const VulkanShaderProgram* GetProgram(unsigned int handle) const;

    /// 获取或创建 VkDescriptorSetLayout（基于 reflection hash）
    VkDescriptorSetLayout GetOrCreateDescriptorSetLayout(
        const std::vector<DescriptorBindingInfo>& bindings, uint32_t set_index);

    /// 初始化内置 PBR 着色器
    void InitBuiltinPBRShader();

    /// 初始化天空盒着色器
    void InitSkyboxShader();

    /// 初始化粒子着色器
    void InitParticleShader();

    /// 初始化 2D 精灵着色器
    void InitSpriteShader();

    /// 初始化后处理着色器（直通/全屏四边形）
    void InitPostProcessShader();

    /// 初始化 Bloom Compute 着色器
    void InitBloomComputeShaders();

    /// 从 GLSL 源码创建 Compute 程序，返回句柄（0 = 失败）
    unsigned int CreateComputeProgram(const std::string& comp_src);

    /// 从预编译 SPIR-V 创建 Compute 程序
    unsigned int CreateComputeProgramFromSpirv(const uint32_t* comp_spv, size_t comp_word_count);

    /// 查询 Compute 程序
    const VulkanComputeProgram* GetComputeProgram(unsigned int handle) const;

    // --- 内置着色器访问器 ---
    unsigned int pbr_shader_handle() const { return pbr_shader_handle_; }
    unsigned int skybox_shader_handle() const { return skybox_shader_handle_; }
    unsigned int particle_shader_handle() const { return particle_shader_handle_; }
    unsigned int sprite_shader_handle() const { return sprite_shader_handle_; }
    unsigned int postprocess_shader_handle() const { return postprocess_shader_handle_; }
    unsigned int bloom_downsample_cs_handle() const { return bloom_downsample_cs_handle_; }
    unsigned int bloom_upsample_cs_handle() const { return bloom_upsample_cs_handle_; }
    unsigned int fxaa_shader_handle() const { return fxaa_shader_handle_; }
    unsigned int ssao_shader_handle() const { return ssao_shader_handle_; }
    unsigned int ssao_blur_shader_handle() const { return ssao_blur_shader_handle_; }
    unsigned int contact_shadow_shader_handle() const { return contact_shadow_shader_handle_; }
    unsigned int lum_compute_shader_handle() const { return lum_compute_shader_handle_; }
    unsigned int lum_adapt_shader_handle() const { return lum_adapt_shader_handle_; }
    unsigned int tonemapping_shader_handle() const { return tonemapping_shader_handle_; }
    unsigned int bloom_composite_ssao_ae_shader_handle() const { return bloom_composite_ssao_ae_shader_handle_; }
    unsigned int color_grading_shader_handle() const { return color_grading_shader_handle_; }
    unsigned int taa_resolve_shader_handle() const { return taa_resolve_shader_handle_; }
    unsigned int dof_shader_handle() const { return dof_shader_handle_; }
    unsigned int motion_blur_shader_handle() const { return motion_blur_shader_handle_; }
    unsigned int ssr_shader_handle() const { return ssr_shader_handle_; }
    unsigned int motion_vector_shader_handle() const { return motion_vector_shader_handle_; }
    unsigned int gbuffer_shader_handle() const { return gbuffer_shader_handle_; }
    unsigned int deferred_lighting_shader_handle() const { return deferred_lighting_shader_handle_; }
    unsigned int edge_detect_shader_handle() const { return edge_detect_shader_handle_; }
    unsigned int volumetric_fog_shader_handle() const { return volumetric_fog_shader_handle_; }
    unsigned int decal_shader_handle() const { return decal_shader_handle_; }
    unsigned int wboit_composite_shader_handle() const { return wboit_composite_shader_handle_; }
    unsigned int water_shader_handle() const { return water_shader_handle_; }

    /// 着色器程序计数
    std::size_t programs_created() const { return programs_created_; }
    std::size_t programs_destroyed() const { return programs_destroyed_; }

private:
    /// 编译 GLSL → SPIR-V
    bool CompileGlslToSpirv(const std::string& source,
                             VkShaderStageFlagBits stage,
                             std::vector<uint32_t>& out_spirv);

    /// 创建 VkShaderModule
    VkShaderModule CreateShaderModule(const std::vector<uint32_t>& spirv);

    /// 对 SPIR-V 执行反射
    bool ReflectSpirv(const std::vector<uint32_t>& vert_spirv,
                       const std::vector<uint32_t>& frag_spirv,
                       ShaderReflection& out_reflection);

    /// 根据 reflection 创建 PipelineLayout
    VkPipelineLayout CreatePipelineLayout(const ShaderReflection& reflection);

    /// Descriptor set layout 缓存键
    struct DescriptorLayoutKey {
        uint32_t set_index;
        std::vector<DescriptorBindingInfo> bindings;
        bool operator==(const DescriptorLayoutKey& o) const {
            return set_index == o.set_index && bindings == o.bindings;
        }
    };
    struct DescriptorLayoutKeyHash {
        size_t operator()(const DescriptorLayoutKey& k) const;
    };

    VulkanContext* context_ = nullptr;

    /// 着色器程序句柄 → Vulkan 对象
    std::unordered_map<unsigned int, VulkanShaderProgram> programs_;
    unsigned int next_handle_ = 540000;

    /// Descriptor set layout 缓存
    std::unordered_map<DescriptorLayoutKey, VkDescriptorSetLayout,
                       DescriptorLayoutKeyHash> descriptor_layout_cache_;

    /// 内置着色器句柄
    unsigned int pbr_shader_handle_ = 0;
    unsigned int skybox_shader_handle_ = 0;
    unsigned int particle_shader_handle_ = 0;
    unsigned int sprite_shader_handle_ = 0;
    unsigned int postprocess_shader_handle_ = 0;
    unsigned int bloom_downsample_cs_handle_ = 0;
    unsigned int bloom_upsample_cs_handle_ = 0;
    unsigned int fxaa_shader_handle_ = 0;
    unsigned int ssao_shader_handle_ = 0;
    unsigned int ssao_blur_shader_handle_ = 0;
    unsigned int contact_shadow_shader_handle_ = 0;
    unsigned int lum_compute_shader_handle_ = 0;
    unsigned int lum_adapt_shader_handle_ = 0;
    unsigned int tonemapping_shader_handle_ = 0;
    unsigned int bloom_composite_ssao_ae_shader_handle_ = 0;
    unsigned int color_grading_shader_handle_ = 0;
    unsigned int taa_resolve_shader_handle_ = 0;
    unsigned int dof_shader_handle_ = 0;
    unsigned int motion_blur_shader_handle_ = 0;
    unsigned int ssr_shader_handle_ = 0;
    unsigned int motion_vector_shader_handle_ = 0;
    unsigned int gbuffer_shader_handle_ = 0;
    unsigned int deferred_lighting_shader_handle_ = 0;
    unsigned int edge_detect_shader_handle_ = 0;
    unsigned int volumetric_fog_shader_handle_ = 0;
    unsigned int decal_shader_handle_ = 0;
    unsigned int wboit_composite_shader_handle_ = 0;
    unsigned int water_shader_handle_ = 0;

    /// Compute 着色器程序
    std::unordered_map<unsigned int, VulkanComputeProgram> compute_programs_;

    std::size_t programs_created_ = 0;
    std::size_t programs_destroyed_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_SHADER_MANAGER_H

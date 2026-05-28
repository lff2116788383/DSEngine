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
#include "engine/render/rhi/shader_manager_base.h"

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

/// Compute 着色器程序
struct VulkanComputeProgram {
    VkShaderModule comp_module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    uint32_t push_constant_size = 0;    ///< push constant 大小（字节）
    bool uses_ssbo_bindings = false;    ///< 是否使用 SSBO 绑定（从 bound_ssbos_ 解析）
    uint32_t ssbo_binding_count = 0;    ///< SSBO binding 数量（Full layout 使用）
    uint32_t storage_image_count = 0;   ///< storage image 绑定数量
    uint32_t sampler_count = 0;         ///< combined image sampler 绑定数量
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
class VulkanShaderManager : public ShaderManagerBase {
public:
    VulkanShaderManager() { next_handle_ = 540000; }
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

    /// 初始化 SDF 文本着色器
    void InitTextSdfShader();

    /// 初始化 UI 视觉效果着色器（圆角/渐变/模糊）
    void InitUIEffectsShader();

    /// 初始化阴影深度着色器
    void InitShadowShader();

    /// 初始化 GPU-Driven PBR 着色器（VS 从 SSBO 读 model, FS 从 Material SSBO 读材质）
    void InitGPUDrivenPBRShader();

    /// 初始化 GPU-Driven Shadow 着色器（depth-only, VS 从 SSBO 读 model）
    void InitGPUDrivenShadowShader();

    /// 初始化后处理着色器（直通/全屏四边形）
    void InitPostProcessShader();

    /// 初始化 Bloom Compute 着色器
    void InitBloomComputeShaders();

    /// 从 GLSL 源码创建 Compute 程序，返回句柄（0 = 失败）
    /// 默认使用 Bloom 风格 layout: binding0=sampler, binding1=storage_image, 4-float push constant
    unsigned int CreateComputeProgram(const std::string& comp_src);

    /// 从 GLSL 源码创建 SSBO 驱动的 Compute 程序
    /// @param comp_src  GLSL 源码
    /// @param ssbo_binding_count  SSBO 绑定数量（binding 0..N-1，类型 STORAGE_BUFFER）
    /// @param push_constant_bytes push constant 大小（字节），0 表示无 push constant
    /// @return 句柄，0 = 失败
    unsigned int CreateComputeProgramSSBO(const std::string& comp_src,
                                          uint32_t ssbo_binding_count,
                                          uint32_t push_constant_bytes);

    /// 从预编译 SPIR-V 创建 Compute 程序
    unsigned int CreateComputeProgramFromSpirv(const uint32_t* comp_spv, size_t comp_word_count);

    /// 从 GLSL 源码创建完整布局 Compute 程序（SSBO + storage image + sampler + push constant）
    /// binding 顺序: [0..ssbo_count-1]=STORAGE_BUFFER, [ssbo_count..+img_count-1]=STORAGE_IMAGE,
    ///              [ssbo_count+img_count..]=COMBINED_IMAGE_SAMPLER
    unsigned int CreateComputeProgramFull(const std::string& comp_src,
                                          uint32_t ssbo_count,
                                          uint32_t storage_image_count,
                                          uint32_t sampler_count,
                                          uint32_t push_constant_bytes);

    /// 查询 Compute 程序
    const VulkanComputeProgram* GetComputeProgram(unsigned int handle) const;

    // 内置着色器句柄访问器、计数器继承自 ShaderManagerBase

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

    /// Descriptor set layout 缓存
    std::unordered_map<DescriptorLayoutKey, VkDescriptorSetLayout,
                       DescriptorLayoutKeyHash> descriptor_layout_cache_;

    /// Compute 着色器程序
    std::unordered_map<unsigned int, VulkanComputeProgram> compute_programs_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_SHADER_MANAGER_H

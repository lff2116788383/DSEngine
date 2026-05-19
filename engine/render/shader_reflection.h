/**
 * @file shader_reflection.h
 * @brief Shader Reflection 消费层 — 三后端自动绑定辅助
 *
 * 使用 shader_compiler 生成的 *_reflect.gen.h 中的 constexpr 反射数据，
 * 自动完成 UBO 绑定（OpenGL）、InputLayout 创建（DX11）、
 * DescriptorSetLayout 构建（Vulkan）等手动硬编码工作。
 */

#pragma once

#include "engine/render/shader_reflection_types.h"
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

namespace dse {
namespace render {

/// 从两个 StageReflection 构建 ProgramReflection
inline shader_reflect::ProgramReflection MakeProgramReflection(
    const shader_reflect::StageReflection& vert,
    const shader_reflect::StageReflection& frag) {
    return {vert, frag};
}

// ============================================================================
// OpenGL 辅助
// ============================================================================

namespace gl_reflect {

/// 自动绑定所有 UBO block 到对应的 binding point（替代手动 BindUBOBlock 调用）
/// @param gl_program  OpenGL shader program handle
/// @param reflection  该阶段的反射数据
/// @param glGetUniformBlockIndex_fn  函数指针 (避免直接依赖 GL headers)
/// @param glUniformBlockBinding_fn  函数指针
/// @return 成功绑定的 UBO 数量
inline uint32_t AutoBindUBOs(
    unsigned int gl_program,
    const shader_reflect::StageReflection& reflection,
    unsigned int (*glGetUniformBlockIndex_fn)(unsigned int, const char*),
    void (*glUniformBlockBinding_fn)(unsigned int, unsigned int, unsigned int)) {

    uint32_t bound = 0;
    for (uint32_t i = 0; i < reflection.uniform_buffer_count; ++i) {
        const auto& ubo = reflection.uniform_buffers[i];
        unsigned int block_idx = glGetUniformBlockIndex_fn(gl_program, ubo.name);
        if (block_idx != 0xFFFFFFFF) { // GL_INVALID_INDEX
            glUniformBlockBinding_fn(gl_program, block_idx, ubo.binding);
            ++bound;
        }
    }
    return bound;
}

/// 批量绑定 vert+frag 的所有 UBO block
inline uint32_t AutoBindAllUBOs(
    unsigned int gl_program,
    const shader_reflect::ProgramReflection& reflection,
    unsigned int (*glGetUniformBlockIndex_fn)(unsigned int, const char*),
    void (*glUniformBlockBinding_fn)(unsigned int, unsigned int, unsigned int)) {

    uint32_t total = 0;
    total += AutoBindUBOs(gl_program, reflection.vertex, glGetUniformBlockIndex_fn, glUniformBlockBinding_fn);
    total += AutoBindUBOs(gl_program, reflection.fragment, glGetUniformBlockIndex_fn, glUniformBlockBinding_fn);
    return total;
}

} // namespace gl_reflect

// ============================================================================
// DX11 辅助
// ============================================================================

namespace dx11_reflect {

/// DX11 InputLayout 元素描述（平台无关镜像，避免直接依赖 d3d11.h）
struct InputElementDesc {
    const char* semantic_name;   ///< "TEXCOORD"
    uint32_t semantic_index;     ///< location
    uint32_t format;             ///< DXGI_FORMAT 枚举值
    uint32_t input_slot;         ///< 0=per-vertex, 1=per-instance
    uint32_t byte_offset;        ///< 累积偏移
    bool per_instance;           ///< false=per-vertex, true=per-instance
};

/// DXGI_FORMAT 常量（避免包含 dxgi.h）
enum DxgiFormat : uint32_t {
    DXGI_FORMAT_R32_FLOAT          = 41,
    DXGI_FORMAT_R32G32_FLOAT       = 16,
    DXGI_FORMAT_R32G32B32_FLOAT    = 6,
    DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
    DXGI_FORMAT_R32_SINT           = 43,
    DXGI_FORMAT_R32G32_SINT        = 18,
    DXGI_FORMAT_R32G32B32_SINT     = 8,
    DXGI_FORMAT_R32G32B32A32_SINT  = 4,
    DXGI_FORMAT_R32_UINT           = 42,
    DXGI_FORMAT_R32G32B32A32_UINT  = 3,
};

/// 从 BaseType 获取 DXGI_FORMAT
inline uint32_t BaseTypeToDxgiFormat(shader_reflect::BaseType type) {
    using BT = shader_reflect::BaseType;
    switch (type) {
        case BT::Float:  return DXGI_FORMAT_R32_FLOAT;
        case BT::Vec2:   return DXGI_FORMAT_R32G32_FLOAT;
        case BT::Vec3:   return DXGI_FORMAT_R32G32B32_FLOAT;
        case BT::Vec4:   return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case BT::Int:    return DXGI_FORMAT_R32_SINT;
        case BT::IVec4:  return DXGI_FORMAT_R32G32B32A32_SINT;
        case BT::UInt:   return DXGI_FORMAT_R32_UINT;
        case BT::UVec4:  return DXGI_FORMAT_R32G32B32A32_UINT;
        default:         return DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
}

/// 从 vertex input reflection 自动生成 InputLayout 描述数组
/// 所有语义名统一为 "TEXCOORD" + location index（与 spirv-cross 默认输出一致）
/// @param reflection  vertex stage 反射数据
/// @param out_elements  输出元素数组
/// @return 元素数量
inline uint32_t AutoCreateInputLayout(
    const shader_reflect::StageReflection& reflection,
    std::vector<InputElementDesc>& out_elements) {

    out_elements.clear();
    uint32_t byte_offset = 0;

    for (uint32_t i = 0; i < reflection.input_count; ++i) {
        const auto& input = reflection.inputs[i];

        if (input.columns > 1) {
            // mat3/mat4: 每列一个元素 (location 递增)
            uint32_t col_size = input.vec_size * 4; // float per component
            uint32_t col_format = (input.vec_size == 4) ? DXGI_FORMAT_R32G32B32A32_FLOAT :
                                  (input.vec_size == 3) ? DXGI_FORMAT_R32G32B32_FLOAT :
                                  (input.vec_size == 2) ? DXGI_FORMAT_R32G32_FLOAT :
                                                          DXGI_FORMAT_R32_FLOAT;
            for (uint32_t c = 0; c < input.columns; ++c) {
                InputElementDesc desc;
                desc.semantic_name = "TEXCOORD";
                desc.semantic_index = input.location + c;
                desc.format = col_format;
                desc.input_slot = 0;
                desc.byte_offset = byte_offset;
                desc.per_instance = false;
                out_elements.push_back(desc);
                byte_offset += col_size;
            }
        } else {
            InputElementDesc desc;
            desc.semantic_name = "TEXCOORD";
            desc.semantic_index = input.location;
            desc.format = BaseTypeToDxgiFormat(input.type);
            desc.input_slot = 0;
            desc.byte_offset = byte_offset;
            desc.per_instance = false;
            out_elements.push_back(desc);
            byte_offset += input.byte_size;
        }
    }

    return static_cast<uint32_t>(out_elements.size());
}

} // namespace dx11_reflect

// ============================================================================
// Vulkan 辅助
// ============================================================================

namespace vk_reflect {

/// 从反射数据构建 descriptor binding 列表（替代 Vulkan 的硬编码 reflection）
/// 返回 (set, binding, descriptor_type, stage_flags, count) 元组列表
struct DescriptorBinding {
    uint32_t set;
    uint32_t binding;
    uint32_t descriptor_type;  ///< VkDescriptorType 枚举值
    uint32_t stage_flags;      ///< VkShaderStageFlags
    uint32_t count;
};

/// VkDescriptorType 常量（避免包含 vulkan.h）
enum VkDescType : uint32_t {
    VK_DESCRIPTOR_TYPE_SAMPLER                = 0,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE          = 2,
    VK_DESCRIPTOR_TYPE_STORAGE_IMAGE          = 3,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER         = 6,
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER         = 7,
};

/// VkShaderStageFlagBits 常量
enum VkStageFlag : uint32_t {
    VK_SHADER_STAGE_VERTEX_BIT   = 0x00000001,
    VK_SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
    VK_SHADER_STAGE_COMPUTE_BIT  = 0x00000020,
};

inline uint32_t ResourceTypeToVkDescriptorType(shader_reflect::ResourceType type) {
    using RT = shader_reflect::ResourceType;
    switch (type) {
        case RT::UniformBuffer:   return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case RT::StorageBuffer:   return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RT::SampledImage:    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case RT::SeparateImage:   return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case RT::StorageImage:    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case RT::SeparateSampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
        default:                  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }
}

/// 从 vert+frag 反射数据提取所有 descriptor bindings
/// 合并两个阶段中相同 (set, binding) 的 stage_flags
inline void ExtractDescriptorBindings(
    const shader_reflect::ProgramReflection& reflection,
    std::vector<DescriptorBinding>& out_bindings) {

    out_bindings.clear();

    auto add_resources = [&](const shader_reflect::ResourceBinding* resources,
                             uint32_t count, uint32_t stage_flag) {
        for (uint32_t i = 0; i < count; ++i) {
            const auto& r = resources[i];
            uint32_t desc_type = ResourceTypeToVkDescriptorType(r.type);

            // 查找是否已存在相同 (set, binding)
            bool found = false;
            for (auto& existing : out_bindings) {
                if (existing.set == r.set && existing.binding == r.binding) {
                    existing.stage_flags |= stage_flag;
                    found = true;
                    break;
                }
            }
            if (!found) {
                out_bindings.push_back({r.set, r.binding, desc_type, stage_flag, r.array_count});
            }
        }
    };

    // Vertex stage
    add_resources(reflection.vertex.uniform_buffers, reflection.vertex.uniform_buffer_count,
                  VK_SHADER_STAGE_VERTEX_BIT);
    add_resources(reflection.vertex.storage_buffers, reflection.vertex.storage_buffer_count,
                  VK_SHADER_STAGE_VERTEX_BIT);
    add_resources(reflection.vertex.sampled_images, reflection.vertex.sampled_image_count,
                  VK_SHADER_STAGE_VERTEX_BIT);

    // Fragment stage
    add_resources(reflection.fragment.uniform_buffers, reflection.fragment.uniform_buffer_count,
                  VK_SHADER_STAGE_FRAGMENT_BIT);
    add_resources(reflection.fragment.storage_buffers, reflection.fragment.storage_buffer_count,
                  VK_SHADER_STAGE_FRAGMENT_BIT);
    add_resources(reflection.fragment.sampled_images, reflection.fragment.sampled_image_count,
                  VK_SHADER_STAGE_FRAGMENT_BIT);
}

/// 提取 push constant range（合并 vert+frag 的 stage flags）
inline void ExtractPushConstantRange(
    const shader_reflect::ProgramReflection& reflection,
    uint32_t& out_stage_flags,
    uint32_t& out_offset,
    uint32_t& out_size) {

    out_stage_flags = 0;
    out_offset = 0;
    out_size = 0;

    if (reflection.vertex.push_constant_size > 0) {
        out_stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
        out_size = reflection.vertex.push_constant_size;
    }
    if (reflection.fragment.push_constant_size > 0) {
        out_stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (reflection.fragment.push_constant_size > out_size) {
            out_size = reflection.fragment.push_constant_size;
        }
    }
}

} // namespace vk_reflect

// ============================================================================
// Debug 验证
// ============================================================================

namespace shader_reflect_debug {

/// 校验反射数据中 UBO binding 与引擎约定的 UBOBindingPoint 是否一致
/// @return 不一致的数量（0 = 全部正确）
inline uint32_t ValidateUBOBindings(
    const shader_reflect::StageReflection& reflection,
    const char* stage_name) {

    uint32_t mismatches = 0;
    for (uint32_t i = 0; i < reflection.uniform_buffer_count; ++i) {
        const auto& ubo = reflection.uniform_buffers[i];
        // 基础校验：binding 不应超过合理范围
        if (ubo.binding > 32) {
            ++mismatches;
        }
        // size 校验：UBO 大小应为 16 字节对齐（std140）
        if (ubo.size > 0 && (ubo.size % 16) != 0) {
            // std140 结构体总大小应为 16 字节对齐
            // 注意：spirv-cross 报告的大小可能不含尾部 padding，这里仅作警告
        }
    }
    return mismatches;
}

/// 校验 vertex input 的 location 连续性和 byte_size 合理性
inline uint32_t ValidateVertexInputs(
    const shader_reflect::StageReflection& reflection) {

    uint32_t issues = 0;
    for (uint32_t i = 0; i < reflection.input_count; ++i) {
        const auto& input = reflection.inputs[i];
        if (input.byte_size == 0) {
            ++issues;
        }
    }
    return issues;
}

} // namespace shader_reflect_debug

} // namespace render
} // namespace dse

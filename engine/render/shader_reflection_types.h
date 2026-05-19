/**
 * @file shader_reflection_types.h
 * @brief Shader Reflection 纯 POD 类型定义
 *
 * 由 shader_compiler 生成的 *_reflect.gen.h 头文件引用此文件中的类型。
 * 本文件不依赖任何引擎头文件，可被 engine/ 自由包含。
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace dse {
namespace render {
namespace shader_reflect {

/// 资源类型
enum class ResourceType : uint8_t {
    UniformBuffer   = 0,
    StorageBuffer   = 1,
    SampledImage    = 2,  ///< combined image sampler (sampler2D, samplerCube, etc.)
    SeparateImage   = 3,
    SeparateSampler = 4,
    StorageImage    = 5,
    PushConstant    = 6,
};

/// 基础数据类型
enum class BaseType : uint8_t {
    Float   = 0,
    Int     = 1,
    UInt    = 2,
    Bool    = 3,
    Vec2    = 4,
    Vec3    = 5,
    Vec4    = 6,
    Mat3    = 7,
    Mat4    = 8,
    Sampler2D       = 9,
    SamplerCube     = 10,
    Sampler2DShadow = 11,
    IVec4   = 12,
    UVec4   = 13,
    Double  = 14,
};

/// 图像维度类型
enum class ImageDimension : uint8_t {
    Dim2D   = 0,
    DimCube = 1,
    Dim3D   = 2,
    Dim1D   = 3,
};

/// UBO/PushConstant 结构体成员
struct MemberInfo {
    const char* name;
    uint32_t offset;    ///< std140/std430 偏移 (bytes)
    uint32_t size;      ///< 数据大小 (bytes)
    BaseType type;
    uint32_t array_size; ///< 0=非数组, N=数组长度
};

/// 资源绑定信息（UBO/SSBO/Texture）
struct ResourceBinding {
    const char* name;
    ResourceType type;
    uint32_t set;           ///< descriptor set (Vulkan), 0 for GL/DX
    uint32_t binding;       ///< binding point
    uint32_t size;          ///< UBO/SSBO 字节大小, 0 = runtime-sized
    uint32_t array_count;   ///< 1=非数组, N=数组
    ImageDimension image_dim; ///< 仅对 SampledImage 有效
    bool depth_compare;     ///< sampler2DShadow / samplerCubeShadow
};

/// 顶点输入属性
struct VertexInput {
    const char* name;
    uint32_t location;
    BaseType type;
    uint32_t vec_size;      ///< 1~4 for scalar/vec, 列数 for mat
    uint32_t columns;       ///< 1 for vec, 3 for mat3, 4 for mat4
    uint32_t byte_size;     ///< 总字节大小
};

/// Push Constant 成员
struct PushConstantMember {
    const char* name;
    uint32_t offset;
    uint32_t size;
    BaseType type;
    uint32_t array_size; ///< 0=非数组
};

/// 单个着色器阶段的反射数据（constexpr 友好）
struct StageReflection {
    const ResourceBinding* uniform_buffers;
    uint32_t uniform_buffer_count;

    const ResourceBinding* storage_buffers;
    uint32_t storage_buffer_count;

    const ResourceBinding* sampled_images;
    uint32_t sampled_image_count;

    const VertexInput* inputs;
    uint32_t input_count;

    const PushConstantMember* push_constants;
    uint32_t push_constant_count;
    uint32_t push_constant_size;   ///< 总 push constant 大小 (bytes)
};

/// 完整着色器程序的反射数据（vert+frag 合并）
struct ProgramReflection {
    StageReflection vertex;
    StageReflection fragment;
};

} // namespace shader_reflect
} // namespace render
} // namespace dse

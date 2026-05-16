/**
 * @file gpu_scene_types.h
 * @brief GPU Driven 渲染所需的 GPU 侧数据结构定义
 *
 * 这些结构体的内存布局必须与 GLSL std430 布局严格对齐，
 * 供 Compute Shader 和 Vertex Shader 通过 SSBO 读取。
 */

#ifndef DSE_GPU_SCENE_TYPES_H
#define DSE_GPU_SCENE_TYPES_H

#include <glm/glm.hpp>
#include <cstdint>

namespace dse {
namespace render {

/// GPU 侧每个 opaque mesh instance 的数据（std430, 80 bytes）
/// SSBO binding 5
struct GPUInstanceData {
    glm::mat4 model;             ///< 64B — 模型矩阵
    uint32_t  material_id;       ///< 4B  — 索引 GPUMaterialData SSBO
    uint32_t  draw_cmd_id;       ///< 4B  — 索引 DrawElementsIndirectCommand
    uint32_t  pad[2];            ///< 8B  — padding to 16B alignment
};
static_assert(sizeof(GPUInstanceData) == 80, "GPUInstanceData must be 80 bytes for std430");

/// GPU 侧材质参数（std430, 128 bytes = 8 × vec4）
/// SSBO binding 9
struct GPUMaterialData {
    glm::vec4 albedo_alpha;      ///< rgb + alpha
    glm::vec4 params0;           ///< metallic, roughness, ao, normal_strength
    glm::vec4 params1;           ///< alpha_cutoff, sss_strength, clear_coat, anisotropy
    glm::vec4 emissive_shading;  ///< emissive.rgb + shading_mode
    glm::vec4 toon_shadow;       ///< shadow_color.rgb + threshold
    glm::vec4 toon_params;       ///< softness, spec_size, spec_strength, rim
    glm::vec4 extra0;            ///< pom_height, cc_roughness, watercolor_paper, watercolor_edge
    glm::vec4 extra1;            ///< watercolor_bleed, watercolor_density, flags (packed), unused
};
static_assert(sizeof(GPUMaterialData) == 128, "GPUMaterialData must be 128 bytes for std430");

/// GPU Driven 管线中使用的 SSBO binding point 常量
namespace gpu_driven {
    constexpr unsigned int kSSBOBindingAABB          = 0;  ///< Hi-Z AABB（已有）
    constexpr unsigned int kSSBOBindingVisibility     = 1;  ///< Hi-Z visibility（已有）
    constexpr unsigned int kSSBOBindingInstances      = 5;  ///< GPUInstanceData[]
    constexpr unsigned int kSSBOBindingDrawCommands   = 6;  ///< DrawElementsIndirectCommand[]（作为 SSBO 供 compute 写入）
    constexpr unsigned int kSSBOBindingVisibleIndices = 7;  ///< visible instance index buffer
    constexpr unsigned int kSSBOBindingAtomicCounter  = 8;  ///< atomic draw count（用于 compacted draw）
    constexpr unsigned int kSSBOBindingMaterials      = 9;  ///< GPUMaterialData[]
}

/// Mega Buffer 中每个唯一 mesh 的注册信息
struct MeshBatchEntry {
    int32_t  base_vertex;   ///< mega VBO 中的起始顶点偏移
    uint32_t first_index;   ///< mega IBO 中的起始 index 偏移
    uint32_t index_count;   ///< 该 mesh 的 index 数量
    uint32_t vertex_count;  ///< 该 mesh 的顶点数量
};

} // namespace render
} // namespace dse

#endif // DSE_GPU_SCENE_TYPES_H

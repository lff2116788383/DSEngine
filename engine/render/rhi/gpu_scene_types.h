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
/// 字段名与布局与 PerMaterial UBO 完全一致，
/// GPU-driven FS 可通过 #define 重定向所有字段访问。
struct GPUMaterialData {
    glm::vec4 albedo;            ///< rgb + metallic
    glm::vec4 roughness_ao;      ///< roughness, ao, normal_strength, alpha_cutoff
    glm::vec4 emissive;          ///< emissive.rgb + alpha_test (0/1)
    glm::vec4 flags;             ///< has_normal_map, has_mr_map, has_emissive_map, has_occlusion_map
    glm::vec4 extra_params;      ///< sss_strength, clear_coat, cc_roughness, anisotropy
    glm::vec4 extra_params2;     ///< pom_height, sss_tint.xyz
    glm::vec4 toon_shadow_color; ///< shadow_color.rgb + threshold
    glm::vec4 toon_params;       ///< softness, spec_size, spec_strength, rim
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

/// GPU-Driven 每个 draw 的纹理句柄组合（用于分桶排序）
struct GPUDrawTextures {
    unsigned int albedo = 0;
    unsigned int normal = 0;
    unsigned int metallic_roughness = 0;
    unsigned int emissive = 0;
    unsigned int occlusion = 0;

    bool operator<(const GPUDrawTextures& o) const {
        if (albedo != o.albedo) return albedo < o.albedo;
        if (normal != o.normal) return normal < o.normal;
        if (metallic_roughness != o.metallic_roughness) return metallic_roughness < o.metallic_roughness;
        if (emissive != o.emissive) return emissive < o.emissive;
        return occlusion < o.occlusion;
    }
    bool operator==(const GPUDrawTextures& o) const {
        return albedo == o.albedo && normal == o.normal
            && metallic_roughness == o.metallic_roughness
            && emissive == o.emissive && occlusion == o.occlusion;
    }
    bool operator!=(const GPUDrawTextures& o) const { return !(*this == o); }
};

/// GPU-Driven 纹理分桶：连续纹理相同的 draw commands 组成一个桶
struct TextureBucket {
    uint32_t cmd_offset;     ///< draw commands 数组中的起始偏移
    uint32_t cmd_count;      ///< 本桶 draw command 数量
    uint32_t material_id;    ///< GPUMaterialData 索引（per-bucket PerMaterial 更新用）
    GPUDrawTextures textures;
};

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

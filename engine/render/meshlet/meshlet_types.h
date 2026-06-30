/**
 * @file meshlet_types.h
 * @brief Meshlet/Cluster 渲染数据结构定义
 *
 * 用于 per-cluster GPU 剔除的核心数据类型。
 * 每个 Meshlet 是原始 mesh 的一个 64-128 三角形子集（cluster），
 * 可独立进行视锥和遮挡剔除。
 */

#ifndef DSE_MESHLET_TYPES_H
#define DSE_MESHLET_TYPES_H

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>

namespace dse {
namespace render {

/// 单个 meshlet（cluster）描述符
struct Meshlet {
    uint32_t vertex_offset;     ///< meshlet 局部顶点索引表的起始偏移（在 meshlet_vertices 数组中）
    uint32_t vertex_count;      ///< meshlet 引用的唯一顶点数
    uint32_t triangle_offset;   ///< 三角形索引表的起始偏移（在 meshlet_triangles 数组中）
    uint32_t triangle_count;    ///< 三角形数量

    /// Bounding sphere (object space) for frustum/occlusion culling
    glm::vec3 center;           ///< 包围球中心（object space）
    float     radius;           ///< 包围球半径

    /// Cone culling (normal cone for backface cluster culling)
    glm::vec3 cone_axis;        ///< 法线锥轴（归一化，object space）
    float     cone_cutoff;      ///< cos(cone_half_angle)，<0 表示禁用锥剔除
};

/// 从 mesh 构建出的完整 meshlet 数据
struct MeshletMesh {
    std::string name;

    /// 原始顶点数据（position only，用于剔除计算）
    std::vector<glm::vec3> positions;

    /// Meshlet 描述符列表
    std::vector<Meshlet> meshlets;

    /// Meshlet 顶点索引表：meshlet[i] 引用原始顶点的索引
    /// meshlets[i].vertex_offset 开始连续 meshlets[i].vertex_count 个 uint32
    std::vector<uint32_t> meshlet_vertices;

    /// Meshlet 三角形索引表：每 3 个 uint8 为一个三角形，索引到 meshlet_vertices 的局部偏移
    /// meshlets[i].triangle_offset 开始连续 meshlets[i].triangle_count * 3 个 uint8
    std::vector<uint8_t> meshlet_triangles;

    /// 对应原始 mesh 的全局索引（用于最终绘制）
    std::vector<uint32_t> global_indices;

    /// 每个 meshlet 在 global_indices 中的起始偏移和 index 数量
    struct DrawRange {
        uint32_t index_offset;  ///< global_indices 中的起始位置
        uint32_t index_count;   ///< 三角形数 × 3
    };
    std::vector<DrawRange> draw_ranges;
};

/// GPU 侧 meshlet AABB 数据（供 compute shader 读取，std430 对齐）
struct MeshletGPUData {
    glm::vec4 sphere;           ///< xyz = center(world), w = radius
    glm::vec4 cone;             ///< xyz = cone_axis(world), w = cone_cutoff
};
static_assert(sizeof(MeshletGPUData) == 32, "MeshletGPUData must be 32 bytes for std430");

/// GPU 侧 meshlet indirect draw command
struct MeshletDrawCommand {
    uint32_t count;             ///< index count
    uint32_t instance_count;    ///< 0 or 1 (culling result)
    uint32_t first_index;       ///< offset into global IBO
    int32_t  base_vertex;       ///< base vertex in mega VBO
    uint32_t base_instance;     ///< unused, set 0
};
static_assert(sizeof(MeshletDrawCommand) == 20, "MeshletDrawCommand must match DrawElementsIndirectCommand");

/// .dmeshlet 文件头
struct DmeshletHeader {
    uint32_t magic;             ///< 'DMLT' = 0x544C4D44
    uint32_t version;           ///< 格式版本（当前 1）
    uint32_t meshlet_count;     ///< meshlet 数量
    uint32_t vertex_count;      ///< 原始顶点数
    uint32_t meshlet_vertex_count;   ///< meshlet_vertices 数组大小
    uint32_t meshlet_triangle_count; ///< meshlet_triangles 数组大小（字节数）
    uint32_t global_index_count;     ///< global_indices 数组大小
    uint32_t reserved;          ///< 保留，填 0
};
static_assert(sizeof(DmeshletHeader) == 32, "DmeshletHeader must be 32 bytes");

constexpr uint32_t kDmeshletMagic = 0x544C4D44; // 'DMLT'
constexpr uint32_t kDmeshletVersion = 1;
constexpr uint32_t kMaxMeshletVertices = 64;     ///< 每个 meshlet 最大顶点数
constexpr uint32_t kMaxMeshletTriangles = 128;   ///< 每个 meshlet 最大三角形数

} // namespace render
} // namespace dse

#endif // DSE_MESHLET_TYPES_H

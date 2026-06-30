/**
 * @file meshlet_builder.h
 * @brief 离线 Meshlet 构建器：将三角形网格切分为 64-128 三角形的 cluster
 *
 * 使用贪心空间局部性聚类算法，输出 MeshletMesh 或序列化为 .dmeshlet 文件。
 */

#ifndef DSE_MESHLET_BUILDER_H
#define DSE_MESHLET_BUILDER_H

#include "engine/render/meshlet/meshlet_types.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace dse {
namespace render {

/// Meshlet 构建配置
struct MeshletBuildConfig {
    uint32_t max_vertices   = kMaxMeshletVertices;    ///< 每 meshlet 最大顶点数（≤64）
    uint32_t max_triangles  = kMaxMeshletTriangles;   ///< 每 meshlet 最大三角形数（≤128）
    float    cone_weight    = 0.5f;                   ///< 法线锥权重（用于质量评估）
};

class MeshletBuilder {
public:
    MeshletBuilder() = default;
    ~MeshletBuilder() = default;

    /// 从位置 + 索引构建 meshlet 数据
    /// @param positions  顶点位置数组
    /// @param indices    三角形索引数组（数量必须是 3 的倍数）
    /// @param config     构建配置
    /// @return 构建结果
    MeshletMesh Build(const std::vector<glm::vec3>& positions,
                      const std::vector<uint32_t>& indices,
                      const MeshletBuildConfig& config = {});

    /// 从带完整顶点属性的 mesh 构建（只使用 position 做聚类）
    MeshletMesh BuildFromFullMesh(const float* vertex_data, uint32_t vertex_count,
                                   uint32_t vertex_stride_floats,
                                   const uint32_t* indices, uint32_t index_count,
                                   const MeshletBuildConfig& config = {});

    /// 序列化 MeshletMesh 到 .dmeshlet 二进制文件
    static bool Serialize(const MeshletMesh& mesh, const std::string& path);

    /// 从 .dmeshlet 文件反序列化
    static bool Deserialize(const std::string& path, MeshletMesh& out_mesh);

private:
    void ComputeMeshletBounds(Meshlet& meshlet, const std::vector<glm::vec3>& positions,
                              const std::vector<uint32_t>& meshlet_vertices,
                              uint32_t vertex_offset, uint32_t vertex_count);
    void ComputeNormalCone(Meshlet& meshlet, const std::vector<glm::vec3>& positions,
                           const std::vector<uint32_t>& meshlet_vertices,
                           const std::vector<uint8_t>& meshlet_triangles,
                           uint32_t vertex_offset, uint32_t tri_offset, uint32_t tri_count);
};

} // namespace render
} // namespace dse

#endif // DSE_MESHLET_BUILDER_H

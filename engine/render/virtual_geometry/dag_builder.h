/**
 * @file dag_builder.h
 * @brief Offline DAG LOD hierarchy builder for virtual geometry
 *
 * Takes a flat MeshletMesh (from MeshletBuilder) and produces a VirtualGeometryMesh
 * with a multi-level DAG where each interior node is a simplified merge of its children.
 */

#ifndef DSE_DAG_BUILDER_H
#define DSE_DAG_BUILDER_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_types.h"
#include "engine/render/meshlet/meshlet_types.h"
#include <glm/glm.hpp>
#include <vector>

namespace dse {
namespace render {
namespace vg {

struct DAGBuildConfig {
    uint32_t max_lod_levels     = 8;
    uint32_t merge_group_size   = 4;
    float    simplification_ratio = 0.5f;
    float    error_tolerance    = 0.01f;
    bool     preserve_boundaries = true;
};

class DAGBuilder {
public:
    DAGBuilder() = default;

    /// Build a complete virtual geometry mesh from positions + indices.
    /// Internally runs MeshletBuilder then builds the DAG hierarchy.
    VirtualGeometryMesh Build(const std::vector<glm::vec3>& positions,
                              const std::vector<glm::vec3>& normals,
                              const std::vector<glm::vec2>& uvs,
                              const std::vector<uint32_t>& indices,
                              const DAGBuildConfig& config = {});

    /// Build from an existing MeshletMesh (skip meshlet generation)
    VirtualGeometryMesh BuildFromMeshletMesh(const MeshletMesh& base_mesh,
                                             const std::vector<glm::vec3>& normals,
                                             const std::vector<glm::vec2>& uvs,
                                             const DAGBuildConfig& config = {});

    /// Serialize a VirtualGeometryMesh to .dvgeo binary file
    static bool Serialize(const VirtualGeometryMesh& mesh, const std::string& path);

    /// Deserialize from .dvgeo file
    static bool Deserialize(const std::string& path, VirtualGeometryMesh& out);

private:
    struct MergeGroup {
        std::vector<uint32_t> cluster_indices;
        glm::vec3 centroid;
    };

    void BuildLeafLevel(VirtualGeometryMesh& vgm, const MeshletMesh& base);
    void BuildDAGLevels(VirtualGeometryMesh& vgm, const DAGBuildConfig& config);
    std::vector<MergeGroup> FormMergeGroups(const VirtualGeometryMesh& vgm,
                                            const std::vector<uint32_t>& level_nodes,
                                            uint32_t group_size);
    uint32_t SimplifyMergeGroup(VirtualGeometryMesh& vgm,
                                const MergeGroup& group,
                                float target_ratio,
                                float& out_error);
    void ComputeDAGBounds(DAGNode& node, const VirtualGeometryMesh& vgm);
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_DAG_BUILDER_H

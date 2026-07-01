/**
 * @file virtual_geometry_types.h
 * @brief Data types for the DAG LOD hierarchy and virtual geometry pipeline
 */

#ifndef DSE_VIRTUAL_GEOMETRY_TYPES_H
#define DSE_VIRTUAL_GEOMETRY_TYPES_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/meshlet/meshlet_types.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>

namespace dse {
namespace render {
namespace vg {

/// A node in the cluster DAG.  Leaf nodes reference a single Meshlet;
/// interior nodes are simplified merge groups whose children they replace
/// when the screen-space error is small enough.
struct DAGNode {
    uint32_t cluster_index;          ///< Index into the flat cluster array
    uint32_t parent;                 ///< Parent node index (UINT32_MAX = root)
    uint32_t first_child;            ///< First child index (UINT32_MAX = leaf)
    uint32_t child_count;            ///< Number of children

    float    lod_error;              ///< Object-space geometric error of this simplification level
    glm::vec3 bound_center;          ///< Bounding sphere center (object space)
    float     bound_radius;          ///< Bounding sphere radius

    uint8_t  lod_level;              ///< 0 = finest (original meshlets), increases with simplification
    uint8_t  pad[3];
    uint32_t reserved[2];            ///< Pad to 48 bytes (16-byte aligned for GPU upload)
};
static_assert(sizeof(DAGNode) == 48, "DAGNode must be 48 bytes");

/// GPU-side DAG node for LOD selection compute shader (std430)
struct DAGNodeGPU {
    glm::vec4 sphere;       ///< xyz = center(world), w = radius
    float     lod_error;    ///< Object-space error
    uint32_t  parent;       ///< Parent index
    uint32_t  first_child;  ///< First child index
    uint32_t  child_count;  ///< Number of children
};
static_assert(sizeof(DAGNodeGPU) == 32, "DAGNodeGPU must be 32 bytes for std430");

/// GPU-side per-cluster data for software rasterizer (std430)
struct ClusterGPU {
    glm::vec4 sphere;           ///< xyz = center(world), w = radius
    glm::vec4 cone;             ///< xyz = cone_axis(world), w = cone_cutoff
    uint32_t  vertex_offset;    ///< Offset into mega vertex buffer
    uint32_t  vertex_count;
    uint32_t  index_offset;     ///< Offset into mega index buffer
    uint32_t  index_count;
    uint32_t  material_id;
    uint32_t  instance_id;      ///< Which instance this cluster belongs to
    uint32_t  pad[2];
};
static_assert(sizeof(ClusterGPU) == 64, "ClusterGPU must be 64 bytes for std430");

/// Visibility Buffer entry (packed into uint64 or 2x uint32)
struct VisBufferEntry {
    uint32_t depth_bits;    ///< Float depth reinterpreted as uint for comparison
    uint32_t payload;       ///< cluster_id:16 | triangle_id:8 | material_id:8
};

/// Result of LOD selection: which clusters are visible and how to rasterize them
struct ClusterVisibility {
    uint32_t cluster_index;
    uint32_t raster_mode;       ///< 0 = hardware, 1 = software
};

/// A complete virtual geometry mesh asset (offline-built)
struct VirtualGeometryMesh {
    std::string name;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t>  indices;

    /// Flat array of clusters (meshlets with DAG metadata)
    std::vector<Meshlet>   clusters;
    std::vector<uint32_t>  cluster_vertices;   ///< Local-to-global vertex mapping per cluster
    std::vector<uint8_t>   cluster_triangles;  ///< Local triangle indices per cluster

    /// DAG LOD hierarchy
    std::vector<DAGNode>   dag_nodes;
    uint32_t               num_lod_levels = 1;
    float                  max_lod_error  = 0.0f;

    /// Per-cluster draw ranges (in global index buffer)
    std::vector<MeshletMesh::DrawRange> draw_ranges;
};

/// Runtime instance of a virtual geometry mesh
struct VGInstance {
    uint32_t  mesh_id;          ///< Reference to registered VirtualGeometryMesh
    glm::mat4 model;            ///< World transform
    uint32_t  material_id;
    bool      nanite_static;    ///< If true, eligible for VisBuffer path
};

/// .dvgeo file header
struct DVGeoHeader {
    uint32_t magic;             ///< 'DVGE' = 0x45475644
    uint32_t version;           ///< Format version (1)
    uint32_t cluster_count;
    uint32_t dag_node_count;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t cluster_vertex_count;
    uint32_t cluster_triangle_bytes;
    uint32_t num_lod_levels;
    float    max_lod_error;
    uint32_t flags;             ///< bit0 = has_normals, bit1 = has_uvs
    uint32_t reserved;
};
static_assert(sizeof(DVGeoHeader) == 48, "DVGeoHeader must be 48 bytes");

constexpr uint32_t kDVGeoMagic   = 0x45475644;  // 'DVGE'
constexpr uint32_t kDVGeoVersion = 1;

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_VIRTUAL_GEOMETRY_TYPES_H

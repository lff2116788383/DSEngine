#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse {
namespace asset {
namespace compiler {

enum class VertexAttribute : uint32_t {
    Position = 1 << 0,
    Normal   = 1 << 1,
    Tangent  = 1 << 2,
    TexCoord = 1 << 3,
    Color    = 1 << 4,
    Joints   = 1 << 5,
    Weights  = 1 << 6
};

inline VertexAttribute operator|(VertexAttribute a, VertexAttribute b) {
    return static_cast<VertexAttribute>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline VertexAttribute operator&(VertexAttribute a, VertexAttribute b) {
    return static_cast<VertexAttribute>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

struct RawSubMesh {
    std::string name;
    uint32_t material_index = 0;
    
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> tangents;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec4> colors;
    std::vector<glm::ivec4> joint_indices;
    std::vector<glm::vec4> joint_weights;
    
    std::vector<uint32_t> indices;
};

struct RawBone {
    std::string name;
    int parent_index = -1;
    glm::mat4 inverse_bind_matrix{1.0f};
    glm::mat4 local_transform{1.0f};
};

struct RawMaterial {
    std::string name;
    glm::vec4 base_color_factor{1.0f};
    float metallic_factor{0.0f};
    float roughness_factor{0.5f};
    glm::vec3 emissive_factor{0.0f};
    
    std::string base_color_texture;
    std::string normal_texture;
    std::string metallic_roughness_texture;
};

struct RawAnimationChannel {
    int target_node_index = -1;
    std::vector<float> time_keys;
    std::vector<glm::vec3> position_keys;
    std::vector<glm::quat> rotation_keys;
    std::vector<glm::vec3> scale_keys;
};

struct RawAnimation {
    std::string name;
    float duration = 0.0f;
    std::vector<RawAnimationChannel> channels;
};

struct RawSceneData {
    std::vector<RawSubMesh> meshes;
    std::vector<RawMaterial> materials;
    std::vector<RawBone> skeleton;
    std::vector<RawAnimation> animations;
};

// --- Runtime Formats (.dmesh) ---
#pragma pack(push, 1)

struct MeshHeader {
    char magic[4] = {'D', 'S', 'E', 'M'};
    uint32_t version = 1;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    uint32_t submesh_count = 0;
    uint32_t attribute_mask = 0;
    
    uint64_t vertex_data_offset = 0;
    uint64_t index_data_offset = 0;
    uint64_t submesh_data_offset = 0;
};

struct SubMeshDesc {
    uint32_t index_start;
    uint32_t index_count;
    uint32_t base_vertex;
    uint32_t material_id;
    glm::vec3 bounding_box_min;
    glm::vec3 bounding_box_max;
};

struct AnimHeader {
    char magic[4] = {'D', 'S', 'E', 'A'};
    uint32_t version = 1;
    float duration = 0.0f;
    uint32_t channel_count = 0;
};

struct AnimChannelDesc {
    int target_node_index;
    uint32_t position_key_count;
    uint32_t rotation_key_count;
    uint32_t scale_key_count;
    uint64_t time_offset;
    uint64_t position_offset;
    uint64_t rotation_offset;
    uint64_t scale_offset;
};

struct SkelHeader {
    char magic[4] = {'D', 'S', 'E', 'S'};
    uint32_t version = 1;
    uint32_t bone_count = 0;
};

struct BoneDesc {
    int parent_index;
    glm::mat4 inverse_bind_matrix;
    glm::mat4 local_transform;
};

#pragma pack(pop)

} // namespace compiler
} // namespace asset
} // namespace dse

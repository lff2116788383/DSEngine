#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/base/debug.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <rapidjson/document.h>

#include "engine/assets/compiler/raw_scene_data.h"

namespace dse {
namespace gameplay3d {

void MeshRenderSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

namespace {
AssetManager& RequireAssetManager(AssetManager* asset_manager) {
    if (asset_manager) {
        return *asset_manager;
    }
    throw std::runtime_error("MeshRenderSystem requires an injected AssetManager");
}

struct RawMeshData {
    std::vector<float> vertices;
    std::vector<unsigned short> indices;
};

std::string ToLower(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

#ifdef DSE_VSE_1522_DIAG

std::string ClassifyVse1522MeshForDiagnostics(const MeshRendererComponent& mesh_renderer) {
    if (mesh_renderer.mesh_path == "procedural:DSE_OceanPlaneQuad") {
        return "OceanPlane";
    }
    if (mesh_renderer.mesh_path.empty() &&
        mesh_renderer.temp_vertices.size() == 12 &&
        mesh_renderer.temp_indices.size() == 6 &&
        mesh_renderer.depth_write_enabled) {
        return "OceanPlane";
    }
    if (mesh_renderer.mesh_path.find("vse_demo/15_22/cooked/Monster.dmesh") != std::string::npos) {
        return "Monster";
    }
    return std::string();
}

bool ShouldLogVse1522MeshDiagnostics(const MeshRendererComponent& mesh_renderer) {
    return !ClassifyVse1522MeshForDiagnostics(mesh_renderer).empty();
}

#endif // DSE_VSE_1522_DIAG

bool LoadTextFile(AssetManager& asset_manager, const std::string& path, std::string& out_text) {
    std::vector<uint8_t> bytes;
    if (!asset_manager.LoadFileToMemory(path, bytes)) {
        return false;
    }
    out_text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

int ResolveObjIndex(int value, int count) {
    if (value > 0) {
        return value - 1;
    }
    if (value < 0) {
        return count + value;
    }
    return -1;
}

bool ParseObjMesh(const std::string& text, RawMeshData& out_mesh) {
    std::vector<glm::vec3> positions;
    std::unordered_map<std::string, unsigned short> vertex_map;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        if (line[0] == 'v' && line.size() > 1 && std::isspace(static_cast<unsigned char>(line[1]))) {
            std::istringstream ls(line.substr(1));
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (!(ls >> x >> y >> z)) {
                continue;
            }
            positions.emplace_back(x, y, z);
            continue;
        }
        if (line[0] == 'f' && line.size() > 1 && std::isspace(static_cast<unsigned char>(line[1]))) {
            std::istringstream ls(line.substr(1));
            std::vector<unsigned short> face;
            std::string token;
            while (ls >> token) {
                auto cached = vertex_map.find(token);
                if (cached != vertex_map.end()) {
                    face.push_back(cached->second);
                    continue;
                }
                std::size_t slash = token.find('/');
                std::string pos_token = slash == std::string::npos ? token : token.substr(0, slash);
                int idx = 0;
                try {
                    idx = std::stoi(pos_token);
                } catch (...) {
                    continue;
                }
                int pos_index = ResolveObjIndex(idx, static_cast<int>(positions.size()));
                if (pos_index < 0 || pos_index >= static_cast<int>(positions.size())) {
                    continue;
                }
                if (out_mesh.vertices.size() / 3 >= std::numeric_limits<unsigned short>::max()) {
                    return false;
                }
                const glm::vec3& p = positions[static_cast<std::size_t>(pos_index)];
                unsigned short new_index = static_cast<unsigned short>(out_mesh.vertices.size() / 3);
                out_mesh.vertices.push_back(p.x);
                out_mesh.vertices.push_back(p.y);
                out_mesh.vertices.push_back(p.z);
                vertex_map.emplace(token, new_index);
                face.push_back(new_index);
            }
            if (face.size() < 3) {
                continue;
            }
            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                out_mesh.indices.push_back(face[0]);
                out_mesh.indices.push_back(face[i]);
                out_mesh.indices.push_back(face[i + 1]);
            }
        }
    }
    return !out_mesh.vertices.empty() && !out_mesh.indices.empty();
}

bool Base64Decode(const std::string& input, std::vector<uint8_t>& output) {
    static const int table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    output.clear();
    int val = 0;
    int bits = -8;
    for (unsigned char c : input) {
        int d = table[c];
        if (d == -1) {
            continue;
        }
        if (d == -2) {
            break;
        }
        val = (val << 6) + d;
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return !output.empty();
}

bool LoadGltfBuffer(AssetManager& asset_manager, const std::string& gltf_path, const std::string& uri, std::vector<uint8_t>& out_data) {
    if (uri.rfind("data:", 0) == 0) {
        std::size_t comma = uri.find(',');
        if (comma == std::string::npos) {
            return false;
        }
        std::string encoded = uri.substr(comma + 1);
        return Base64Decode(encoded, out_data);
    }
    std::filesystem::path base = std::filesystem::path(gltf_path).parent_path();
    std::filesystem::path full = base / uri;
    return asset_manager.LoadFileToMemory(full.string(), out_data);
}

bool ParseGltfMesh(AssetManager& asset_manager, const std::string& gltf_path, const std::string& text, RawMeshData& out_mesh) {
    rapidjson::Document doc;
    doc.Parse(text.c_str(), text.size());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }
    if (!doc.HasMember("meshes") || !doc["meshes"].IsArray() || doc["meshes"].Empty()) {
        return false;
    }
    if (!doc.HasMember("buffers") || !doc["buffers"].IsArray() || doc["buffers"].Empty()) {
        return false;
    }
    if (!doc.HasMember("bufferViews") || !doc["bufferViews"].IsArray()) {
        return false;
    }
    if (!doc.HasMember("accessors") || !doc["accessors"].IsArray()) {
        return false;
    }

    std::vector<std::vector<uint8_t>> buffers(doc["buffers"].Size());
    for (rapidjson::SizeType i = 0; i < doc["buffers"].Size(); ++i) {
        const auto& buffer = doc["buffers"][i];
        if (!buffer.IsObject() || !buffer.HasMember("uri") || !buffer["uri"].IsString()) {
            return false;
        }
        if (!LoadGltfBuffer(asset_manager, gltf_path, buffer["uri"].GetString(), buffers[i])) {
            return false;
        }
    }

    const auto& mesh = doc["meshes"][0];
    if (!mesh.IsObject() || !mesh.HasMember("primitives") || !mesh["primitives"].IsArray() || mesh["primitives"].Empty()) {
        return false;
    }
    const auto& primitive = mesh["primitives"][0];
    if (!primitive.IsObject() || !primitive.HasMember("attributes") || !primitive["attributes"].IsObject()) {
        return false;
    }
    const auto& attrs = primitive["attributes"];
    if (!attrs.HasMember("POSITION") || !attrs["POSITION"].IsInt()) {
        return false;
    }
    int pos_accessor_index = attrs["POSITION"].GetInt();
    int idx_accessor_index = -1;
    if (primitive.HasMember("indices") && primitive["indices"].IsInt()) {
        idx_accessor_index = primitive["indices"].GetInt();
    }
    if (pos_accessor_index < 0 || pos_accessor_index >= static_cast<int>(doc["accessors"].Size())) {
        return false;
    }
    const auto& pos_accessor = doc["accessors"][pos_accessor_index];
    if (!pos_accessor.IsObject() || !pos_accessor.HasMember("bufferView") || !pos_accessor.HasMember("count")) {
        return false;
    }
    int pos_view_index = pos_accessor["bufferView"].GetInt();
    int pos_count = pos_accessor["count"].GetInt();
    if (pos_view_index < 0 || pos_view_index >= static_cast<int>(doc["bufferViews"].Size()) || pos_count <= 0) {
        return false;
    }
    const auto& pos_view = doc["bufferViews"][pos_view_index];
    if (!pos_view.IsObject() || !pos_view.HasMember("buffer")) {
        return false;
    }
    int pos_buffer_index = pos_view["buffer"].GetInt();
    if (pos_buffer_index < 0 || pos_buffer_index >= static_cast<int>(buffers.size())) {
        return false;
    }
    std::size_t pos_view_offset = pos_view.HasMember("byteOffset") ? static_cast<std::size_t>(pos_view["byteOffset"].GetUint()) : 0;
    std::size_t pos_accessor_offset = pos_accessor.HasMember("byteOffset") ? static_cast<std::size_t>(pos_accessor["byteOffset"].GetUint()) : 0;
    std::size_t pos_stride = pos_view.HasMember("byteStride") ? static_cast<std::size_t>(pos_view["byteStride"].GetUint()) : sizeof(float) * 3;
    const std::vector<uint8_t>& pos_buffer = buffers[static_cast<std::size_t>(pos_buffer_index)];
    std::size_t pos_base = pos_view_offset + pos_accessor_offset;

    out_mesh.vertices.reserve(static_cast<std::size_t>(pos_count) * 3);
    for (int i = 0; i < pos_count; ++i) {
        std::size_t offset = pos_base + static_cast<std::size_t>(i) * pos_stride;
        if (offset + sizeof(float) * 3 > pos_buffer.size()) {
            return false;
        }
        float value[3];
        std::memcpy(value, pos_buffer.data() + offset, sizeof(float) * 3);
        out_mesh.vertices.push_back(value[0]);
        out_mesh.vertices.push_back(value[1]);
        out_mesh.vertices.push_back(value[2]);
    }

    if (idx_accessor_index >= 0) {
        if (idx_accessor_index >= static_cast<int>(doc["accessors"].Size())) {
            return false;
        }
        const auto& idx_accessor = doc["accessors"][idx_accessor_index];
        if (!idx_accessor.IsObject() || !idx_accessor.HasMember("bufferView") || !idx_accessor.HasMember("count") || !idx_accessor.HasMember("componentType")) {
            return false;
        }
        int idx_view_index = idx_accessor["bufferView"].GetInt();
        int idx_count = idx_accessor["count"].GetInt();
        int component_type = idx_accessor["componentType"].GetInt();
        if (idx_view_index < 0 || idx_view_index >= static_cast<int>(doc["bufferViews"].Size()) || idx_count <= 0) {
            return false;
        }
        const auto& idx_view = doc["bufferViews"][idx_view_index];
        int idx_buffer_index = idx_view["buffer"].GetInt();
        if (idx_buffer_index < 0 || idx_buffer_index >= static_cast<int>(buffers.size())) {
            return false;
        }
        std::size_t idx_view_offset = idx_view.HasMember("byteOffset") ? static_cast<std::size_t>(idx_view["byteOffset"].GetUint()) : 0;
        std::size_t idx_accessor_offset = idx_accessor.HasMember("byteOffset") ? static_cast<std::size_t>(idx_accessor["byteOffset"].GetUint()) : 0;
        const std::vector<uint8_t>& idx_buffer = buffers[static_cast<std::size_t>(idx_buffer_index)];
        std::size_t component_size = component_type == 5125 ? 4u : (component_type == 5123 ? 2u : (component_type == 5121 ? 1u : 0u));
        if (component_size == 0) {
            return false;
        }
        std::size_t stride = idx_view.HasMember("byteStride") ? static_cast<std::size_t>(idx_view["byteStride"].GetUint()) : component_size;
        std::size_t idx_base = idx_view_offset + idx_accessor_offset;
        out_mesh.indices.reserve(static_cast<std::size_t>(idx_count));
        for (int i = 0; i < idx_count; ++i) {
            std::size_t offset = idx_base + static_cast<std::size_t>(i) * stride;
            if (offset + component_size > idx_buffer.size()) {
                return false;
            }
            std::uint32_t value = 0;
            if (component_type == 5125) {
                std::memcpy(&value, idx_buffer.data() + offset, 4);
            } else if (component_type == 5123) {
                std::uint16_t v16 = 0;
                std::memcpy(&v16, idx_buffer.data() + offset, 2);
                value = v16;
            } else {
                value = idx_buffer[offset];
            }
            if (value > std::numeric_limits<unsigned short>::max()) {
                return false;
            }
            out_mesh.indices.push_back(static_cast<unsigned short>(value));
        }
    } else {
        std::size_t vertex_count = out_mesh.vertices.size() / 3;
        if (vertex_count > std::numeric_limits<unsigned short>::max()) {
            return false;
        }
        out_mesh.indices.reserve(vertex_count);
        for (std::size_t i = 0; i < vertex_count; ++i) {
            out_mesh.indices.push_back(static_cast<unsigned short>(i));
        }
    }
    return !out_mesh.vertices.empty() && !out_mesh.indices.empty();
}

bool ParseFbxArray(const std::string& text, const std::string& key, std::vector<double>& out_values) {
    std::size_t start = text.find(key);
    if (start == std::string::npos) {
        return false;
    }
    start = text.find("a:", start);
    if (start == std::string::npos) {
        return false;
    }
    start += 2;
    std::size_t end = text.find('}', start);
    if (end == std::string::npos) {
        return false;
    }
    std::string numbers = text.substr(start, end - start);
    for (char& ch : numbers) {
        if (ch == ',' || ch == '\r' || ch == '\n' || ch == '\t') {
            ch = ' ';
        }
    }
    std::istringstream ss(numbers);
    double value = 0.0;
    while (ss >> value) {
        out_values.push_back(value);
    }
    return !out_values.empty();
}

bool ParseFbxMesh(const std::string& text, RawMeshData& out_mesh) {
    if (text.find("Kaydara FBX Binary") != std::string::npos) {
        return false;
    }
    std::vector<double> vertices;
    std::vector<double> polygon_indices;
    if (!ParseFbxArray(text, "Vertices:", vertices)) {
        return false;
    }
    if (!ParseFbxArray(text, "PolygonVertexIndex:", polygon_indices)) {
        return false;
    }
    if (vertices.size() < 3) {
        return false;
    }
    std::size_t vertex_count = vertices.size() / 3;
    if (vertex_count > std::numeric_limits<unsigned short>::max()) {
        return false;
    }
    out_mesh.vertices.reserve(vertex_count * 3);
    for (std::size_t i = 0; i + 2 < vertices.size(); i += 3) {
        out_mesh.vertices.push_back(static_cast<float>(vertices[i + 0]));
        out_mesh.vertices.push_back(static_cast<float>(vertices[i + 1]));
        out_mesh.vertices.push_back(static_cast<float>(vertices[i + 2]));
    }

    std::vector<unsigned short> face;
    for (double raw : polygon_indices) {
        int idx = static_cast<int>(raw);
        bool end = idx < 0;
        if (end) {
            idx = -idx - 1;
        }
        if (idx < 0 || idx >= static_cast<int>(vertex_count)) {
            return false;
        }
        face.push_back(static_cast<unsigned short>(idx));
        if (end) {
            if (face.size() >= 3) {
                for (std::size_t i = 1; i + 1 < face.size(); ++i) {
                    out_mesh.indices.push_back(face[0]);
                    out_mesh.indices.push_back(face[i]);
                    out_mesh.indices.push_back(face[i + 1]);
                }
            }
            face.clear();
        }
    }
    return !out_mesh.vertices.empty() && !out_mesh.indices.empty();
}

bool LoadMeshByPath(AssetManager& asset_manager, const std::string& mesh_path, RawMeshData& out_mesh) {
    std::string extension = ToLower(std::filesystem::path(mesh_path).extension().string());
    std::string text;
    if (extension == ".obj") {
        if (!LoadTextFile(asset_manager, mesh_path, text)) {
            return false;
        }
        return ParseObjMesh(text, out_mesh);
    }
    if (extension == ".gltf") {
        if (!LoadTextFile(asset_manager, mesh_path, text)) {
            return false;
        }
        return ParseGltfMesh(asset_manager, mesh_path, text, out_mesh);
    }
    if (extension == ".fbx") {
        if (!LoadTextFile(asset_manager, mesh_path, text)) {
            return false;
        }
        return ParseFbxMesh(text, out_mesh);
    }
    return false;
}

void EnsureMeshPathDataLoaded(AssetManager& asset_manager, World& world, entt::entity entity, MeshRendererComponent& mesh_renderer) {
    if (mesh_renderer.mesh_path.empty()) {
        return;
    }
    if (!mesh_renderer.temp_vertices.empty() && !mesh_renderer.temp_indices.empty()) {
        return;
    }

    auto update_bounding_box = [&](const std::vector<float>& vertices, size_t stride) {
        if (!world.registry().all_of<BoundingBoxComponent>(entity)) {
            world.registry().emplace<BoundingBoxComponent>(entity);
        }
        auto& bbox = world.registry().get<BoundingBoxComponent>(entity);
        if (vertices.empty()) return;
        glm::vec3 min_ext(vertices[0], vertices[1], vertices[2]);
        glm::vec3 max_ext = min_ext;
        for (size_t i = 0; i < vertices.size(); i += stride) {
            min_ext.x = std::min(min_ext.x, vertices[i]);
            min_ext.y = std::min(min_ext.y, vertices[i+1]);
            min_ext.z = std::min(min_ext.z, vertices[i+2]);
            max_ext.x = std::max(max_ext.x, vertices[i]);
            max_ext.y = std::max(max_ext.y, vertices[i+1]);
            max_ext.z = std::max(max_ext.z, vertices[i+2]);
        }
        bbox.min_extents = min_ext;
        bbox.max_extents = max_ext;
    };

    // Fast path: Check if it's a compiled .dmesh
    if (mesh_renderer.mesh_path.find(".dmesh") != std::string::npos) {
        auto dmesh = asset_manager.LoadDmesh(mesh_renderer.mesh_path);
        if (dmesh && !dmesh->GetData().empty()) {
            const uint8_t* data = dmesh->GetData().data();
            const dse::asset::compiler::MeshHeader* header = reinterpret_cast<const dse::asset::compiler::MeshHeader*>(data);
            if (header->magic[0] == 'D' && header->magic[1] == 'S' && header->magic[2] == 'E' && header->magic[3] == 'M') {
                const dse::asset::compiler::SubMeshDesc* submeshes = reinterpret_cast<const dse::asset::compiler::SubMeshDesc*>(data + header->submesh_data_offset);
                if (header->submesh_count > 0) {
                    const uint32_t* indices = reinterpret_cast<const uint32_t*>(data + header->index_data_offset);
                    const float* vertices = reinterpret_cast<const float*>(data + header->vertex_data_offset);
                    
                    const int kDmeshVertexFloatStride = (header->version >= 2) ? 24 : 20;
                    mesh_renderer.dmesh_vertex_stride = kDmeshVertexFloatStride;
                    mesh_renderer.temp_vertices.reserve(static_cast<std::size_t>(header->vertex_count) * kDmeshVertexFloatStride);
                    for (uint32_t i = 0; i < header->vertex_count; ++i) {
                        for (int j = 0; j < kDmeshVertexFloatStride; ++j) {
                            mesh_renderer.temp_vertices.push_back(vertices[static_cast<std::size_t>(i) * kDmeshVertexFloatStride + j]);
                        }
                    }

                    mesh_renderer.temp_indices.reserve(header->index_count);
                    bool index_overflow = false;
                    for (uint32_t submesh_index = 0; submesh_index < header->submesh_count; ++submesh_index) {
                        const auto& submesh = submeshes[submesh_index];
                        for (uint32_t i = 0; i < submesh.index_count; ++i) {
                            const uint32_t resolved_index = submesh.base_vertex + indices[submesh.index_start + i];
                            if (resolved_index >= header->vertex_count || resolved_index > std::numeric_limits<unsigned short>::max()) {
                                index_overflow = true;
                                break;
                            }
                            mesh_renderer.temp_indices.push_back(static_cast<unsigned short>(resolved_index));
                        }
                        if (index_overflow) {
                            break;
                        }
                    }
                    if (index_overflow) {
                        mesh_renderer.temp_vertices.clear();
                        mesh_renderer.temp_indices.clear();
                        return;
                    }
                    update_bounding_box(mesh_renderer.temp_vertices, mesh_renderer.dmesh_vertex_stride);
                    return;
                }
            }
        }
    }

    // Fallback to legacy raw mesh parser
    static std::unordered_map<std::string, RawMeshData> cache;
    auto it = cache.find(mesh_renderer.mesh_path);
    if (it != cache.end()) {
        mesh_renderer.temp_vertices = it->second.vertices;
        mesh_renderer.temp_indices = it->second.indices;
        update_bounding_box(mesh_renderer.temp_vertices, 3);
        return;
    }
    RawMeshData mesh;
    if (LoadMeshByPath(asset_manager, mesh_renderer.mesh_path, mesh)) {
        cache.emplace(mesh_renderer.mesh_path, mesh);
        mesh_renderer.temp_vertices = mesh.vertices;
        mesh_renderer.temp_indices = mesh.indices;
        update_bounding_box(mesh_renderer.temp_vertices, 3);
    }
}

// GPU Instancing: 零分配合批 key
struct InstancingKeyData {
    unsigned int tex[5];
    float color[4];
    float scalars[5];
    float emissive[3];
    unsigned int blend_mode;
    int shading_mode, sorting_layer, order_in_layer, flags;
};

struct InstancingKey {
    const std::string* mesh_path;
    InstancingKeyData data;

    bool operator==(const InstancingKey& o) const {
        return *mesh_path == *o.mesh_path
            && std::memcmp(&data, &o.data, sizeof(InstancingKeyData)) == 0;
    }
};

struct InstancingKeyHash {
    size_t operator()(const InstancingKey& k) const {
        size_t h = std::hash<std::string>{}(*k.mesh_path);
        const auto* p = reinterpret_cast<const unsigned char*>(&k.data);
        for (size_t i = 0; i < sizeof(InstancingKeyData); ++i)
            h ^= static_cast<size_t>(p[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

}

void MeshRenderSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    auto& asset_manager = RequireAssetManager(asset_manager_);
    auto view = world.registry().view<TransformComponent, MeshRendererComponent>();
    auto light_view = world.registry().view<DirectionalLight3DComponent>();
    DirectionalLight3DComponent light_data;
    bool has_light = false;
    for (auto light_entity : light_view) {
        auto& light = light_view.get<DirectionalLight3DComponent>(light_entity);
        if (!light.enabled) {
            continue;
        }
        light_data = light;
        has_light = true;
        break;
    }
    
    // Collect Point Lights
    std::vector<MeshDrawItem::PointLightData> point_lights;
    auto point_light_view = world.registry().view<TransformComponent, PointLightComponent>();
    int next_point_shadow_index = 0;
    for (auto entity : point_light_view) {
        if (point_lights.size() >= 64) break; // MAX_POINT_LIGHTS = 64 (Clustered Forward+)
        auto& transform = point_light_view.get<TransformComponent>(entity);
        auto& light = point_light_view.get<PointLightComponent>(entity);
        if (light.enabled) {
            const float effective_radius = light.radius * std::max(0.1f, light.falloff);
            int shadow_index = -1;
            if (light.cast_shadow && next_point_shadow_index < 4) {
                shadow_index = next_point_shadow_index;
                ++next_point_shadow_index;
            }
            point_lights.push_back({light.color, transform.position, light.intensity, effective_radius, light.cast_shadow && shadow_index >= 0, shadow_index});
        }
    }

    glm::vec3 skylight_up_color(0.0f);
    glm::vec3 skylight_down_color(0.0f);
    float skylight_intensity = 0.0f;
    auto sky_light_view = world.registry().view<SkyLightComponent>();
    for (auto entity : sky_light_view) {
        const auto& light = sky_light_view.get<SkyLightComponent>(entity);
        if (!light.enabled) {
            continue;
        }
        skylight_up_color = light.up_color;
        skylight_down_color = light.down_color;
        skylight_intensity = light.intensity;
        break;
    }
    const glm::vec3 skylight_ambient = glm::mix(skylight_down_color, skylight_up_color, 0.5f) * skylight_intensity;
    
    // Collect Spot Lights
    std::vector<MeshDrawItem::SpotLightData> spot_lights;
    auto spot_light_view = world.registry().view<TransformComponent, SpotLightComponent>();
    int next_spot_shadow_index = 0;
    for (auto entity : spot_light_view) {
        auto& transform = spot_light_view.get<TransformComponent>(entity);
        auto& light = spot_light_view.get<SpotLightComponent>(entity);
        if (light.enabled) {
            glm::vec3 forward = glm::normalize(transform.rotation * light.direction);
            const float effective_radius = light.radius * std::max(0.1f, light.falloff);
            int shadow_index = -1;
            if (light.cast_shadow && next_spot_shadow_index < 4) {
                shadow_index = next_spot_shadow_index;
                ++next_spot_shadow_index;
            }
            spot_lights.push_back({light.color, transform.position, forward, light.intensity, effective_radius, light.inner_cone_angle, light.outer_cone_angle, light.cast_shadow && shadow_index >= 0, shadow_index});
        }
    }
    
    std::vector<MeshDrawItem> batch_items;
    batch_items.reserve(view.size_hint());

    // Hi-Z: 准备收集本帧 AABB
    cached_aabbs_.clear();
    cached_aabbs_.reserve(view.size_hint());
    int hiz_mesh_index = 0;

    // GPU Instancing: 相同 mesh_path + 材质的非蛮皮实体合批（零堆分配 key）
    std::unordered_map<InstancingKey, size_t, InstancingKeyHash> instancing_map;

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& mesh_renderer = view.get<MeshRendererComponent>(entity);
        
        EnsureMeshPathDataLoaded(asset_manager, world, entity, mesh_renderer);
        if (!mesh_renderer.visible) continue;
        if (mesh_renderer.is_static && static_batches_built_) continue;

        // Hi-Z: 计算并缓存 local bounds（仅首次加载后计算一次）
        if (!mesh_renderer.local_bounds_valid && !mesh_renderer.temp_vertices.empty()) {
            const bool is_dmesh_lb = mesh_renderer.mesh_path.find(".dmesh") != std::string::npos;
            const int stride = is_dmesh_lb ? mesh_renderer.dmesh_vertex_stride : 3;
            const size_t vertex_count = mesh_renderer.temp_vertices.size() / stride;
            glm::vec3 lb_min(std::numeric_limits<float>::max());
            glm::vec3 lb_max(std::numeric_limits<float>::lowest());
            for (size_t i = 0; i < vertex_count; ++i) {
                glm::vec3 p(mesh_renderer.temp_vertices[i * stride],
                            mesh_renderer.temp_vertices[i * stride + 1],
                            mesh_renderer.temp_vertices[i * stride + 2]);
                lb_min = glm::min(lb_min, p);
                lb_max = glm::max(lb_max, p);
            }
            mesh_renderer.local_bounds_min = lb_min;
            mesh_renderer.local_bounds_max = lb_max;
            mesh_renderer.local_bounds_valid = true;
        }

        // Hi-Z: 早期遮挡剔除 — 用 local bounds + transform 快速计算保守世界 AABB
        if (mesh_renderer.local_bounds_valid) {
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), transform.position)
                                  * glm::mat4_cast(transform.rotation)
                                  * glm::scale(glm::mat4(1.0f), transform.scale);
            const glm::vec3& bmin = mesh_renderer.local_bounds_min;
            const glm::vec3& bmax = mesh_renderer.local_bounds_max;
            glm::vec3 w_min(std::numeric_limits<float>::max());
            glm::vec3 w_max(std::numeric_limits<float>::lowest());
            for (int ci = 0; ci < 8; ++ci) {
                glm::vec3 corner = glm::vec3(model * glm::vec4(
                    (ci & 1) ? bmax.x : bmin.x,
                    (ci & 2) ? bmax.y : bmin.y,
                    (ci & 4) ? bmax.z : bmin.z, 1.0f));
                w_min = glm::min(w_min, corner);
                w_max = glm::max(w_max, corner);
            }
            cached_aabbs_.push_back({glm::vec4(w_min, 0.0f), glm::vec4(w_max, 0.0f)});

            // 检查上一帧的可见性
            if (hiz_mesh_index < static_cast<int>(hiz_visibility_.size())) {
                if (hiz_visibility_[hiz_mesh_index] == 0) {
                    ++hiz_mesh_index;
                    continue;  // 被遮挡，跳过后续全部顶点处理
                }
            }
            ++hiz_mesh_index;
        }
        
        std::shared_ptr<MaterialAsset> material_instance;
        if (mesh_renderer.material_instance_id != 0) {
            material_instance = asset_manager.GetMaterialInstance(mesh_renderer.material_instance_id);
        }

        const bool prefer_material_instance =
            mesh_renderer.material_data_source == MeshRendererComponent::MaterialDataSource::MaterialInstance &&
            material_instance != nullptr;

        const std::string& resolved_shader_variant = prefer_material_instance
            ? material_instance->GetShaderVariant()
            : mesh_renderer.shader_variant;
        const glm::vec4 resolved_base_color = prefer_material_instance
            ? material_instance->GetBaseColor()
            : mesh_renderer.color;
        const glm::vec3 resolved_emissive = prefer_material_instance
            ? material_instance->GetEmissiveColor()
            : mesh_renderer.emissive;
        const MaterialAsset::TextureSlots resolved_texture_slots = prefer_material_instance
            ? material_instance->GetTextureSlots()
            : MaterialAsset::TextureSlots{
                .albedo              = mesh_renderer.albedo_texture_handle,
                .normal              = mesh_renderer.normal_texture_handle,
                .metallic_roughness  = mesh_renderer.metallic_roughness_texture_handle,
                .emissive            = mesh_renderer.emissive_texture_handle,
                .occlusion           = mesh_renderer.occlusion_texture_handle
            };
        const MaterialAsset::ScalarOverrides resolved_scalars = prefer_material_instance
            ? material_instance->GetScalarOverrides()
            : MaterialAsset::ScalarOverrides{
                .metallic             = mesh_renderer.metallic,
                .roughness            = mesh_renderer.roughness,
                .ao                   = mesh_renderer.ao,
                .normal_strength      = mesh_renderer.normal_strength,
                .alpha_cutoff         = mesh_renderer.material_alpha_cutoff,
                .alpha_test           = mesh_renderer.material_alpha_test,
                .sss_strength         = mesh_renderer.sss_strength,
                .sss_tint             = mesh_renderer.sss_tint,
                .clear_coat           = mesh_renderer.clear_coat,
                .clear_coat_roughness = mesh_renderer.clear_coat_roughness,
                .anisotropy           = mesh_renderer.anisotropy,
                .pom_height_scale     = mesh_renderer.pom_height_scale
            };
        const MaterialBlendMode resolved_blend_mode = prefer_material_instance
            ? material_instance->GetBlendMode()
            : MaterialBlendMode::Opaque;
        const bool resolved_alpha_test = prefer_material_instance
            ? material_instance->GetScalarOverrides().alpha_test
            : mesh_renderer.material_alpha_test;
        const bool resolved_double_sided = prefer_material_instance
            ? material_instance->GetRasterOverrides().double_sided
            : mesh_renderer.material_double_sided;
        
        MeshDrawItem item;
        const glm::mat4 mesh_model = transform.local_to_world;
#ifdef DSE_VSE_1522_DIAG
        item.debug_label = ClassifyVse1522MeshForDiagnostics(mesh_renderer);
#endif // DSE_VSE_1522_DIAG
        item.model = glm::mat4(1.0f);
        item.blend_mode = static_cast<unsigned int>(resolved_blend_mode);
        item.sorting_layer = mesh_renderer.sorting_layer;
        item.order_in_layer = mesh_renderer.order_in_layer;
        item.lighting_enabled = resolved_shader_variant != "MESH_UNLIT";
        if (resolved_shader_variant == "MESH_HALFLAMBERT") {
            item.shading_mode = 2;  // Half-Lambert skin (KF knight/zombie)
        } else if (resolved_shader_variant == "MESH_HALFLAMBERT_STATIC") {
            item.shading_mode = 3;  // Half-Lambert static (KF default shader)
        } else if (resolved_shader_variant == "MESH_TOON") {
            item.shading_mode = 4;  // Toon / Cel shading
        } else if (resolved_shader_variant == "MESH_WATERCOLOR") {
            item.shading_mode = 5;  // Watercolor stylization
        }
        
        if (world.registry().all_of<Animator3DComponent>(entity)) {
            const auto& animator = world.registry().get<Animator3DComponent>(entity);
            if (animator.enabled && !animator.final_bone_matrices.empty()) {
                item.skinned = true;
                item.bone_matrices = animator.final_bone_matrices;
                for (auto& mat : item.bone_matrices) {
                    mat = mesh_model * mat;
                }
            }
        }
        
        if (world.registry().all_of<MorphComponent>(entity)) {
            const auto& morph = world.registry().get<MorphComponent>(entity);
            if (morph.enabled && !morph.targets.empty()) {
                item.morph_enabled = true;
                for (const auto& target : morph.targets) {
                    item.morph_weights.push_back(target.weight);
                }
            }
        }
        
        item.texture_handle = resolved_texture_slots.albedo != 0
            ? resolved_texture_slots.albedo
            : (prefer_material_instance ? material_instance->GetTextureHandle() : 0);
        item.normal_map_handle = resolved_texture_slots.normal;
        item.metallic_roughness_map_handle = resolved_texture_slots.metallic_roughness;
        item.emissive_map_handle = resolved_texture_slots.emissive;
        item.occlusion_map_handle = resolved_texture_slots.occlusion;
        item.color = resolved_base_color;
        item.material_albedo = glm::vec3(resolved_base_color);
        item.material_metallic = resolved_scalars.metallic;
        item.material_roughness = resolved_scalars.roughness;
        item.material_ao = resolved_scalars.ao;
        item.material_normal_strength = resolved_scalars.normal_strength;
        item.material_alpha_cutoff = resolved_scalars.alpha_cutoff;
        item.material_alpha_test = resolved_alpha_test;
        item.material_double_sided = resolved_double_sided;
        item.material_sss_strength = resolved_scalars.sss_strength;
        item.material_sss_tint = resolved_scalars.sss_tint;
        item.material_clear_coat = resolved_scalars.clear_coat;
        item.material_clear_coat_roughness = resolved_scalars.clear_coat_roughness;
        item.material_anisotropy = resolved_scalars.anisotropy;
        item.material_pom_height_scale = resolved_scalars.pom_height_scale;
        item.toon_shadow_color = mesh_renderer.toon_shadow_color;
        item.toon_shadow_threshold = mesh_renderer.toon_shadow_threshold;
        item.toon_shadow_softness = mesh_renderer.toon_shadow_softness;
        item.toon_specular_size = mesh_renderer.toon_specular_size;
        item.toon_specular_strength = mesh_renderer.toon_specular_strength;
        item.toon_rim_strength = mesh_renderer.toon_rim_strength;
        item.watercolor_paper_strength = mesh_renderer.watercolor_paper_strength;
        item.watercolor_edge_darkening = mesh_renderer.watercolor_edge_darkening;
        item.watercolor_color_bleed = mesh_renderer.watercolor_color_bleed;
        item.watercolor_pigment_density = mesh_renderer.watercolor_pigment_density;
        item.material_uses_instance_data = prefer_material_instance;
        item.material_emissive = resolved_emissive;
        item.receive_shadow = mesh_renderer.receive_shadow;
        item.depth_test_enabled = mesh_renderer.depth_test_enabled;
        item.depth_write_enabled = mesh_renderer.depth_write_enabled;
        
        item.point_lights.clear();
        for (const auto& pt_data : point_lights) {
            MeshDrawItem::PointLightData pld;
            pld.color = pt_data.color;
            pld.position = pt_data.position;
            pld.intensity = pt_data.intensity;
            pld.radius = pt_data.radius;
            pld.cast_shadow = pt_data.cast_shadow;
            pld.shadow_index = pt_data.shadow_index;
            item.point_lights.push_back(pld);
        }
        
        item.spot_lights.clear();
        for (const auto& sp_data : spot_lights) {
            MeshDrawItem::SpotLightData sld;
            sld.color = sp_data.color;
            sld.position = sp_data.position;
            sld.direction = sp_data.direction;
            sld.intensity = sp_data.intensity;
            sld.radius = sp_data.radius;
            sld.inner_cone = sp_data.inner_cone;
            sld.outer_cone = sp_data.outer_cone;
            sld.cast_shadow = sp_data.cast_shadow;
            sld.shadow_index = sp_data.shadow_index;
            item.spot_lights.push_back(sld);
        }
        
        if (has_light) {
            item.light_direction = light_data.direction;
            item.light_color = light_data.color;
            item.light_intensity = light_data.intensity;
            item.ambient_intensity = light_data.ambient_intensity + skylight_intensity;
            item.shadow_strength = light_data.shadow_strength;
            item.material_emissive += skylight_ambient * 0.05f;
        } else if (skylight_intensity > 0.0f) {
            item.ambient_intensity = skylight_intensity;
            item.material_emissive += skylight_ambient * 0.05f;
        }

        // GPU Instancing: 非蒙皮非变形 + 有 mesh_path → 检查合批（静态物体走 StaticBatch）
        const bool can_instance = !item.skinned && !item.morph_enabled
            && !mesh_renderer.is_static
            && item.blend_mode == static_cast<unsigned int>(MaterialBlendMode::Opaque)
            && !mesh_renderer.mesh_path.empty()
            && !mesh_renderer.temp_vertices.empty()
            && !mesh_renderer.temp_indices.empty();

        InstancingKey inst_key{};
        if (can_instance) {
            inst_key.mesh_path = &mesh_renderer.mesh_path;
            auto& kd = inst_key.data;
            kd.tex[0] = item.texture_handle;
            kd.tex[1] = item.normal_map_handle;
            kd.tex[2] = item.metallic_roughness_map_handle;
            kd.tex[3] = item.emissive_map_handle;
            kd.tex[4] = item.occlusion_map_handle;
            kd.color[0] = item.color.r; kd.color[1] = item.color.g;
            kd.color[2] = item.color.b; kd.color[3] = item.color.a;
            kd.scalars[0] = item.material_metallic;
            kd.scalars[1] = item.material_roughness;
            kd.scalars[2] = item.material_ao;
            kd.scalars[3] = item.material_normal_strength;
            kd.scalars[4] = item.material_alpha_cutoff;
            kd.emissive[0] = item.material_emissive.r;
            kd.emissive[1] = item.material_emissive.g;
            kd.emissive[2] = item.material_emissive.b;
            kd.blend_mode = item.blend_mode;
            kd.shading_mode = item.shading_mode;
            kd.sorting_layer = item.sorting_layer;
            kd.order_in_layer = item.order_in_layer;
            kd.flags = (item.material_alpha_test ? 1 : 0)
                     | (item.material_double_sided ? 2 : 0)
                     | (item.receive_shadow ? 4 : 0)
                     | (item.depth_test_enabled ? 8 : 0)
                     | (item.depth_write_enabled ? 16 : 0)
                     | (item.lighting_enabled ? 32 : 0);

            auto map_it = instancing_map.find(inst_key);
            if (map_it != instancing_map.end()) {
                batch_items[map_it->second].instance_transforms.push_back(mesh_model);
                continue;
            }

            item.model = mesh_model;
            item.instance_transforms.push_back(mesh_model);
        }

        if (!mesh_renderer.temp_vertices.empty() && !mesh_renderer.temp_indices.empty()) {
#ifdef DSE_VSE_1522_DIAG
            const bool emit_vse1522_depth_diag = ShouldLogVse1522MeshDiagnostics(mesh_renderer);
#else
            const bool emit_vse1522_depth_diag = false;
#endif // DSE_VSE_1522_DIAG
            const bool is_dmesh_format = mesh_renderer.mesh_path.find(".dmesh") != std::string::npos;
            const std::size_t vertex_count_from_pos3 = mesh_renderer.temp_vertices.size() / 3;
            const bool has_lua_uvs = !is_dmesh_format && vertex_count_from_pos3 > 0 && mesh_renderer.temp_uvs.size() == vertex_count_from_pos3 * 2;
            const bool has_lua_normals = !is_dmesh_format && vertex_count_from_pos3 > 0 && mesh_renderer.temp_normals.size() == vertex_count_from_pos3 * 3;
            const bool has_lua_tangents = !is_dmesh_format && vertex_count_from_pos3 > 0 && mesh_renderer.temp_tangents.size() == vertex_count_from_pos3 * 3;
            size_t stride = is_dmesh_format ? static_cast<size_t>(mesh_renderer.dmesh_vertex_stride) : 3;
            if (mesh_renderer.temp_vertices.size() % stride != 0) {
                continue;
            }
            size_t vertex_count = mesh_renderer.temp_vertices.size() / stride;
            std::vector<glm::vec3> local_positions(vertex_count);
            for (size_t i = 0; i < vertex_count; ++i) {
                local_positions[i] = glm::vec3(
                    mesh_renderer.temp_vertices[i * stride + 0],
                    mesh_renderer.temp_vertices[i * stride + 1],
                    mesh_renderer.temp_vertices[i * stride + 2]
                );
            }
            glm::vec3 world_min(std::numeric_limits<float>::max());
            glm::vec3 world_max(std::numeric_limits<float>::lowest());
            std::vector<glm::vec3> local_normals(vertex_count, glm::vec3(0.0f));
            bool invalid_index = false;
            for (size_t i = 0; i + 2 < mesh_renderer.temp_indices.size(); i += 3) {
                const unsigned short i0 = mesh_renderer.temp_indices[i + 0];
                const unsigned short i1 = mesh_renderer.temp_indices[i + 1];
                const unsigned short i2 = mesh_renderer.temp_indices[i + 2];
                if (i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) {
                    invalid_index = true;
                    break;
                }
                const glm::vec3 e1 = local_positions[i1] - local_positions[i0];
                const glm::vec3 e2 = local_positions[i2] - local_positions[i0];
                const glm::vec3 face_normal = glm::cross(e1, e2);
                local_normals[i0] += face_normal;
                local_normals[i1] += face_normal;
                local_normals[i2] += face_normal;
            }
            if (invalid_index) {
                continue;
            }

            // CPU skinning 后深度估算：采样前 N 个顶点做 skinning，估算最终 NDC z 范围
            float skinned_ndc_z_min = std::numeric_limits<float>::max();
            float skinned_ndc_z_max = std::numeric_limits<float>::lowest();
            int skinned_valid_count = 0;
#ifdef DSE_VSE_1522_DIAG
            if (emit_vse1522_depth_diag && item.skinned && !item.bone_matrices.empty()) {
                // 获取 camera view/projection
                auto camera3d_view = world.registry().view<Camera3DComponent>();
                entt::entity cam_entity = entt::null;
                int cam_priority = std::numeric_limits<int>::min();
                for (auto ce : camera3d_view) {
                    auto& cam = camera3d_view.get<Camera3DComponent>(ce);
                    if (cam.enabled && cam.priority > cam_priority) {
                        cam_entity = ce;
                        cam_priority = cam.priority;
                    }
                }
                if (cam_entity != entt::null) {
                    auto& cam = camera3d_view.get<Camera3DComponent>(cam_entity);
                    glm::mat4 proj = glm::perspective(glm::radians(cam.fov),
                        cam.aspect_ratio,
                        cam.near_clip, cam.far_clip);
                    glm::mat4 cam_view(1.0f);
                    if (world.registry().all_of<TransformComponent>(cam_entity)) {
                        auto& cam_tf = world.registry().get<TransformComponent>(cam_entity);
                        glm::vec3 front = cam_tf.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
                        glm::vec3 up = cam_tf.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
                        cam_view = glm::lookAt(cam_tf.position, cam_tf.position + front, up);
                    }
                    const glm::mat4 vp = proj * cam_view;

                    // 采样最多 64 个顶点做 CPU skinning
                    const size_t sample_count = std::min(vertex_count, static_cast<size_t>(64));
                    for (size_t si = 0; si < sample_count; ++si) {
                        const glm::vec3& lpos = local_positions[si];
                        glm::vec4 skinned_pos(0.0f);
                        if (is_dmesh_format && stride >= 12) {
                            // dmesh 顶点有 weights 和 joints
                            glm::vec4 weights(
                                mesh_renderer.temp_vertices[si * stride + 8],
                                mesh_renderer.temp_vertices[si * stride + 9],
                                mesh_renderer.temp_vertices[si * stride + 10],
                                mesh_renderer.temp_vertices[si * stride + 11]
                            );
                            int j0, j1, j2, j3;
                            std::memcpy(&j0, &mesh_renderer.temp_vertices[si * stride + 12], sizeof(float));
                            std::memcpy(&j1, &mesh_renderer.temp_vertices[si * stride + 13], sizeof(float));
                            std::memcpy(&j2, &mesh_renderer.temp_vertices[si * stride + 14], sizeof(float));
                            std::memcpy(&j3, &mesh_renderer.temp_vertices[si * stride + 15], sizeof(float));
                            const float w_sum = weights.x + weights.y + weights.z + weights.w;
                            if (w_sum < 1e-6f) continue;
                            // bone_matrices 已经预乘了 mesh_model
                            const int bone_indices[4] = {j0, j1, j2, j3};
                            const float bone_weights[4] = {weights.x, weights.y, weights.z, weights.w};
                            for (int b = 0; b < 4; ++b) {
                                if (bone_weights[b] < 1e-6f) continue;
                                const int bi = bone_indices[b];
                                if (bi >= 0 && static_cast<size_t>(bi) < item.bone_matrices.size()) {
                                    skinned_pos += bone_weights[b] * (item.bone_matrices[bi] * glm::vec4(lpos, 1.0f));
                                }
                            }
                        } else {
                            // 非 dmesh：无骨骼信息，跳过
                            continue;
                        }
                        const glm::vec4 clip = vp * skinned_pos;
                        if (clip.w > 1e-6f) {
                            const float ndc_z = clip.z / clip.w;
                            skinned_ndc_z_min = std::min(skinned_ndc_z_min, ndc_z);
                            skinned_ndc_z_max = std::max(skinned_ndc_z_max, ndc_z);
                            ++skinned_valid_count;
                        }
                    }
                }
            }
#endif // DSE_VSE_1522_DIAG
            item.vertices.reserve(vertex_count);
            for (size_t i = 0; i < vertex_count; ++i) {
                BatchVertex bv;
                glm::vec4 world_pos = mesh_model * glm::vec4(
                    local_positions[i].x,
                    local_positions[i].y,
                    local_positions[i].z,
                    1.0f
                );
                const glm::vec3 world_pos3(world_pos);
                world_min = glm::min(world_min, world_pos3);
                world_max = glm::max(world_max, world_pos3);
                bv.pos = (item.skinned || can_instance) ? local_positions[i] : world_pos3;
                bv.color = item.color;
                
                glm::vec3 normal;
                if (is_dmesh_format) {
                    normal = glm::vec3(mesh_renderer.temp_vertices[i * stride + 3], mesh_renderer.temp_vertices[i * stride + 4], mesh_renderer.temp_vertices[i * stride + 5]);
                    bv.uv = glm::vec2(mesh_renderer.temp_vertices[i * stride + 6], mesh_renderer.temp_vertices[i * stride + 7]);
                    bv.weights = glm::vec4(
                        mesh_renderer.temp_vertices[i * stride + 8],
                        mesh_renderer.temp_vertices[i * stride + 9],
                        mesh_renderer.temp_vertices[i * stride + 10],
                        mesh_renderer.temp_vertices[i * stride + 11]
                    );
                    
                    // Interpret floats as ints for bone indices
                    int j0, j1, j2, j3;
                    std::memcpy(&j0, &mesh_renderer.temp_vertices[i * stride + 12], sizeof(float));
                    std::memcpy(&j1, &mesh_renderer.temp_vertices[i * stride + 13], sizeof(float));
                    std::memcpy(&j2, &mesh_renderer.temp_vertices[i * stride + 14], sizeof(float));
                    std::memcpy(&j3, &mesh_renderer.temp_vertices[i * stride + 15], sizeof(float));
                    bv.joints = glm::vec4(static_cast<float>(j0), static_cast<float>(j1), static_cast<float>(j2), static_cast<float>(j3));
                    
                    if (stride >= 20) {
                        bv.tangent = glm::vec3(
                            mesh_renderer.temp_vertices[i * stride + 16],
                            mesh_renderer.temp_vertices[i * stride + 17],
                            mesh_renderer.temp_vertices[i * stride + 18]
                        );
                    } else {
                        bv.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                    }
                    // dmesh v2: per-vertex color at [20-23], multiply with item.color
                    if (stride >= 24) {
                        bv.color = item.color * glm::vec4(
                            mesh_renderer.temp_vertices[i * stride + 20],
                            mesh_renderer.temp_vertices[i * stride + 21],
                            mesh_renderer.temp_vertices[i * stride + 22],
                            mesh_renderer.temp_vertices[i * stride + 23]
                        );
                    }
                } else {
                    normal = has_lua_normals
                        ? glm::vec3(mesh_renderer.temp_normals[i * 3 + 0], mesh_renderer.temp_normals[i * 3 + 1], mesh_renderer.temp_normals[i * 3 + 2])
                        : local_normals[i];
                    bv.uv = has_lua_uvs
                        ? glm::vec2(mesh_renderer.temp_uvs[i * 2 + 0], mesh_renderer.temp_uvs[i * 2 + 1])
                        : glm::vec2(0.0f, 0.0f);
                    bv.weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    bv.joints = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
                    bv.tangent = has_lua_tangents
                        ? glm::vec3(mesh_renderer.temp_tangents[i * 3 + 0], mesh_renderer.temp_tangents[i * 3 + 1], mesh_renderer.temp_tangents[i * 3 + 2])
                        : glm::vec3(1.0f, 0.0f, 0.0f);
                }
                
                if (glm::length(normal) < 1e-6f) {
                    normal = glm::vec3(0.0f, 0.0f, 1.0f);
                } else {
                    normal = glm::normalize(normal);
                }
                const glm::mat3 normal_matrix = glm::inverseTranspose(glm::mat3(mesh_model));
                bv.normal = (item.skinned || can_instance) ? normal : glm::normalize(normal_matrix * normal);
                item.vertices.push_back(bv);
            }
            item.indices = mesh_renderer.temp_indices;
            item.debug_world_bounds_min = world_min;
            item.debug_world_bounds_max = world_max;

            if (emit_vse1522_depth_diag) {
#ifdef DSE_VSE_1522_DIAG
                static int vse1522_diag_frame = 0;
                static int vse1522_diag_logs = 0;
                if (vse1522_diag_logs < 12) {
                    DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][MeshRenderSystem] seq={} label={} entity={} skinned={} mesh_path={} depth_test={} depth_write={} vertices={} indices={} world_min=({},{},{}) world_max=({},{},{}) local_stride={} material_double_sided={} sorting=({}, {}) skinned_ndc_z_min={} skinned_ndc_z_max={} skinned_sampled_count={} note=skinned_bounds_are_bind_mesh_model_bounds",
                                   vse1522_diag_frame,
                                   item.debug_label,
                                   static_cast<unsigned int>(entity),
                                   item.skinned,
                                   mesh_renderer.mesh_path,
                                   item.depth_test_enabled,
                                   item.depth_write_enabled,
                                   item.vertices.size(),
                                   item.indices.size(),
                                   world_min.x,
                                   world_min.y,
                                   world_min.z,
                                   world_max.x,
                                   world_max.y,
                                   world_max.z,
                                   stride,
                                   item.material_double_sided,
                                   item.sorting_layer,
                                   item.order_in_layer,
                                   item.skinned ? skinned_ndc_z_min : 0.0f,
                                   item.skinned ? skinned_ndc_z_max : 0.0f,
                                   skinned_valid_count);
                    ++vse1522_diag_logs;
                }
                ++vse1522_diag_frame;
#endif // DSE_VSE_1522_DIAG
            }
        }

        if (!item.vertices.empty() && !item.indices.empty()) {
            if (mesh_renderer.is_static && !static_batches_built_) {
                static_batch_builder_.Add(item);
            } else {
                batch_items.push_back(item);
                if (can_instance) {
                    instancing_map[inst_key] = batch_items.size() - 1;
                }
            }
        }
    }

    // 静态合批：首帧构建完成后缓存，后续帧复用
    if (!static_batches_built_ && !static_batch_builder_.empty()) {
        static_batch_items_ = static_batch_builder_.Build();
        static_batch_builder_.Clear();
        static_batches_built_ = true;
    }

    // 分离不透明与透明绘制项
    transparent_items_.clear();
    std::vector<MeshDrawItem> opaque_items;
    opaque_items.reserve(batch_items.size());
    for (auto& item : batch_items) {
        const bool is_transparent =
            item.blend_mode != static_cast<unsigned int>(MaterialBlendMode::Opaque)
            || item.color.a < 0.999f;
        if (is_transparent) {
            transparent_items_.push_back(std::move(item));
        } else {
            opaque_items.push_back(std::move(item));
        }
    }

    if (!opaque_items.empty()) {
        std::sort(opaque_items.begin(), opaque_items.end(), [](const MeshDrawItem& a, const MeshDrawItem& b) {
            if (a.sorting_layer != b.sorting_layer) {
                return a.sorting_layer < b.sorting_layer;
            }
            if (a.order_in_layer != b.order_in_layer) {
                return a.order_in_layer < b.order_in_layer;
            }
            return MakeSortKey(a) < MakeSortKey(b);
        });
        cmd_buffer.DrawMeshBatch(opaque_items);
    }

    // 静态合批结果：已按 MakeSortKey 排序，直接提交（零拷贝引用），刷新本帧光照
    if (!static_batch_items_.empty()) {
        for (auto& sb_item : static_batch_items_) {
            sb_item.point_lights = point_lights;
            sb_item.spot_lights = spot_lights;
            if (has_light) {
                sb_item.lighting_enabled = true;
                sb_item.light_direction = light_data.direction;
                sb_item.light_color = light_data.color;
                sb_item.light_intensity = light_data.intensity;
                sb_item.ambient_intensity = light_data.ambient_intensity + skylight_intensity;
                sb_item.shadow_strength = light_data.shadow_strength;
            } else {
                sb_item.ambient_intensity = skylight_intensity;
            }
        }
        cmd_buffer.DrawMeshBatch(static_batch_items_);
    }
}

void MeshRenderSystem::RenderTransparent(World& world, CommandBuffer& cmd_buffer, int wboit_mode) {
    if (transparent_items_.empty()) return;

    for (auto& item : transparent_items_) {
        item.wboit_mode = wboit_mode;
    }
    cmd_buffer.DrawMeshBatch(transparent_items_);
}

int MeshRenderSystem::PrepareGPUScene(World& world, dse::render::RenderPassContext& ctx) {
    if (!ctx.gpu_driven_enabled) return 0;

    auto* rhi = ctx.rhi_device;
    if (!rhi) return 0;

    auto view = world.registry().view<TransformComponent, MeshRendererComponent>();

    gpu_draw_cmds_.clear();
    gpu_instances_.clear();
    gpu_aabbs_.clear();

    int cmd_index = 0;
    bool new_mesh_added = false;

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& mesh_renderer = view.get<MeshRendererComponent>(entity);

        if (!mesh_renderer.visible) continue;
        if (mesh_renderer.temp_vertices.empty() || mesh_renderer.temp_indices.empty()) continue;
        if (!mesh_renderer.local_bounds_valid) continue;

        // 排除蒙皮和透明
        if (world.registry().all_of<Animator3DComponent>(entity)) {
            const auto& animator = world.registry().get<Animator3DComponent>(entity);
            if (animator.enabled && !animator.final_bone_matrices.empty()) continue;
        }
        if (mesh_renderer.color.a < 0.999f) continue;

        const glm::mat4 model = transform.local_to_world;

        // 计算世界 AABB
        const glm::vec3& bmin = mesh_renderer.local_bounds_min;
        const glm::vec3& bmax = mesh_renderer.local_bounds_max;
        glm::vec3 w_min(std::numeric_limits<float>::max());
        glm::vec3 w_max(std::numeric_limits<float>::lowest());
        for (int ci = 0; ci < 8; ++ci) {
            glm::vec3 corner = glm::vec3(model * glm::vec4(
                (ci & 1) ? bmax.x : bmin.x,
                (ci & 2) ? bmax.y : bmin.y,
                (ci & 4) ? bmax.z : bmin.z, 1.0f));
            w_min = glm::min(w_min, corner);
            w_max = glm::max(w_max, corner);
        }
        gpu_aabbs_.push_back({glm::vec4(w_min, 0.0f), glm::vec4(w_max, 0.0f)});

        // 注册 mesh 到 mega buffer（去重：相同 mesh_path 只注册一次）
        const std::string& mesh_key = mesh_renderer.mesh_path;
        auto reg_it = mesh_registry_.find(mesh_key);
        if (reg_it == mesh_registry_.end()) {
            // 新 mesh：转换为 BatchVertex 格式追加到 mega buffer
            const bool is_dmesh = mesh_renderer.mesh_path.find(".dmesh") != std::string::npos;
            const int stride = is_dmesh ? mesh_renderer.dmesh_vertex_stride : 3;
            if (stride <= 0) continue;
            const size_t vertex_count = mesh_renderer.temp_vertices.size() / stride;

            dse::render::MeshBatchEntry entry{};
            entry.base_vertex = static_cast<int32_t>(mega_vbo_vertex_count_);
            entry.first_index = mega_ibo_index_count_;
            entry.index_count = static_cast<uint32_t>(mesh_renderer.temp_indices.size());
            entry.vertex_count = static_cast<uint32_t>(vertex_count);

            // 追加顶点数据（BatchVertex 格式：23 floats per vertex）
            for (size_t i = 0; i < vertex_count; ++i) {
                glm::vec3 pos(mesh_renderer.temp_vertices[i * stride],
                              mesh_renderer.temp_vertices[i * stride + 1],
                              mesh_renderer.temp_vertices[i * stride + 2]);
                glm::vec3 normal(0.0f, 0.0f, 1.0f);
                glm::vec2 uv(0.0f);
                glm::vec3 tangent(1.0f, 0.0f, 0.0f);
                glm::vec4 weights(0.0f);
                glm::vec4 joints(0.0f);

                if (is_dmesh && stride >= 8) {
                    normal = glm::vec3(mesh_renderer.temp_vertices[i * stride + 3],
                                       mesh_renderer.temp_vertices[i * stride + 4],
                                       mesh_renderer.temp_vertices[i * stride + 5]);
                    uv = glm::vec2(mesh_renderer.temp_vertices[i * stride + 6],
                                   mesh_renderer.temp_vertices[i * stride + 7]);
                }
                if (is_dmesh && stride >= 16) {
                    weights = glm::vec4(mesh_renderer.temp_vertices[i * stride + 8],
                                        mesh_renderer.temp_vertices[i * stride + 9],
                                        mesh_renderer.temp_vertices[i * stride + 10],
                                        mesh_renderer.temp_vertices[i * stride + 11]);
                }
                if (is_dmesh && stride >= 20) {
                    tangent = glm::vec3(mesh_renderer.temp_vertices[i * stride + 16],
                                        mesh_renderer.temp_vertices[i * stride + 17],
                                        mesh_renderer.temp_vertices[i * stride + 18]);
                }

                // pos(3) + color(4) + uv(2) + normal(3) + tangent(3) + weights(4) + joints(4)
                glm::vec4 color = mesh_renderer.color;
                mega_vbo_data_.push_back(pos.x); mega_vbo_data_.push_back(pos.y); mega_vbo_data_.push_back(pos.z);
                mega_vbo_data_.push_back(color.r); mega_vbo_data_.push_back(color.g);
                mega_vbo_data_.push_back(color.b); mega_vbo_data_.push_back(color.a);
                mega_vbo_data_.push_back(uv.x); mega_vbo_data_.push_back(uv.y);
                mega_vbo_data_.push_back(normal.x); mega_vbo_data_.push_back(normal.y); mega_vbo_data_.push_back(normal.z);
                mega_vbo_data_.push_back(tangent.x); mega_vbo_data_.push_back(tangent.y); mega_vbo_data_.push_back(tangent.z);
                mega_vbo_data_.push_back(weights.x); mega_vbo_data_.push_back(weights.y);
                mega_vbo_data_.push_back(weights.z); mega_vbo_data_.push_back(weights.w);
                mega_vbo_data_.push_back(joints.x); mega_vbo_data_.push_back(joints.y);
                mega_vbo_data_.push_back(joints.z); mega_vbo_data_.push_back(joints.w);
            }

            // 追加索引数据（转换为 uint32_t）
            for (unsigned short idx : mesh_renderer.temp_indices) {
                mega_ibo_data_.push_back(static_cast<uint32_t>(idx));
            }

            mega_vbo_vertex_count_ += entry.vertex_count;
            mega_ibo_index_count_ += entry.index_count;
            mesh_registry_[mesh_key] = entry;
            reg_it = mesh_registry_.find(mesh_key);
            new_mesh_added = true;
        }

        const auto& entry = reg_it->second;

        // DrawElementsIndirectCommand — instance_count=1 初始（GPU cull 会写 0 表示剔除）
        DrawElementsIndirectCommand cmd{};
        cmd.count = entry.index_count;
        cmd.instance_count = 1;
        cmd.first_index = entry.first_index;
        cmd.base_vertex = entry.base_vertex;
        cmd.base_instance = static_cast<uint32_t>(cmd_index);
        gpu_draw_cmds_.push_back(cmd);

        // GPUInstanceData
        dse::render::GPUInstanceData inst{};
        inst.model = model;
        inst.material_id = 0;
        inst.draw_cmd_id = static_cast<uint32_t>(cmd_index);
        inst.pad[0] = 0;
        inst.pad[1] = 0;
        gpu_instances_.push_back(inst);

        ++cmd_index;
    }

    // 上传/更新 Mega VBO/IBO
    if (new_mesh_added && mega_vbo_vertex_count_ > 0) {
        mega_buffer_dirty_ = true;
    }
    if (mega_buffer_dirty_ && mega_vbo_vertex_count_ > 0) {
        const size_t vbo_bytes = mega_vbo_data_.size() * sizeof(float);
        const size_t ibo_bytes = mega_ibo_data_.size() * sizeof(uint32_t);
        if (mega_vao_ == 0) {
            mega_vao_ = rhi->CreateMegaVAO(vbo_bytes, ibo_bytes, mega_vbo_, mega_ibo_);
        } else {
            // 需要扩容时重建
            rhi->DeleteMegaVAO(mega_vao_, mega_vbo_, mega_ibo_);
            mega_vao_ = rhi->CreateMegaVAO(vbo_bytes, ibo_bytes, mega_vbo_, mega_ibo_);
        }
        if (mega_vao_ != 0) {
            rhi->UpdateMegaVBO(mega_vbo_, 0, vbo_bytes, mega_vbo_data_.data());
            rhi->UpdateMegaIBO(mega_ibo_, 0, ibo_bytes, mega_ibo_data_.data());
        }
        mega_buffer_dirty_ = false;
    }
    ctx.gpu_mega_vao = mega_vao_;

    if (cmd_index == 0) {
        ctx.gpu_indirect_draw_count = 0;
        ctx.gpu_total_instances = 0;
        return 0;
    }

    // 上传 AABB SSBO（复用 Hi-Z AABB binding point 0）
    if (ctx.hiz_aabb_ssbo != 0) {
        const size_t aabb_size = gpu_aabbs_.size() * sizeof(HiZAABB);
        rhi->UpdateSSBO(ctx.hiz_aabb_ssbo, 0, aabb_size, gpu_aabbs_.data());
    }

    const size_t required_count = static_cast<size_t>(cmd_index);

    // 上传 DrawCommands 到 SSBO（binding 6，供 compute shader 读写）
    {
        const size_t cmd_size = required_count * sizeof(DrawElementsIndirectCommand);
        if (ctx.gpu_draw_cmd_ssbo != 0 && required_count > gpu_draw_cmd_capacity_) {
            rhi->DeleteSSBO(ctx.gpu_draw_cmd_ssbo);
            ctx.gpu_draw_cmd_ssbo = 0;
        }
        if (ctx.gpu_draw_cmd_ssbo == 0) {
            const size_t alloc_count = std::max(required_count, static_cast<size_t>(128));
            ctx.gpu_draw_cmd_ssbo = rhi->CreateSSBO(alloc_count * sizeof(DrawElementsIndirectCommand), nullptr);
            gpu_draw_cmd_capacity_ = alloc_count;
        }
        rhi->UpdateSSBO(ctx.gpu_draw_cmd_ssbo, 0, cmd_size, gpu_draw_cmds_.data());
    }

    // 上传 GPUInstanceData SSBO（binding 5）
    {
        const size_t inst_size = required_count * sizeof(dse::render::GPUInstanceData);
        if (ctx.gpu_instance_ssbo != 0 && required_count > gpu_instance_capacity_) {
            rhi->DeleteSSBO(ctx.gpu_instance_ssbo);
            ctx.gpu_instance_ssbo = 0;
        }
        if (ctx.gpu_instance_ssbo == 0) {
            const size_t alloc_count = std::max(required_count, static_cast<size_t>(128));
            ctx.gpu_instance_ssbo = rhi->CreateSSBO(alloc_count * sizeof(dse::render::GPUInstanceData), nullptr);
            gpu_instance_capacity_ = alloc_count;
        }
        rhi->UpdateSSBO(ctx.gpu_instance_ssbo, 0, inst_size, gpu_instances_.data());
    }

    ctx.gpu_indirect_draw_count = cmd_index;
    ctx.gpu_total_instances = cmd_index;
    ctx.hiz_object_count = cmd_index;

    return cmd_index;
}

void MeshRenderSystem::CleanupGPUResources(RhiDevice* rhi) {
    if (!rhi) return;
    if (mega_vao_ != 0) {
        rhi->DeleteMegaVAO(mega_vao_, mega_vbo_, mega_ibo_);
        mega_vao_ = 0;
        mega_vbo_ = 0;
        mega_ibo_ = 0;
    }
    mega_vbo_data_.clear();
    mega_ibo_data_.clear();
    mega_vbo_vertex_count_ = 0;
    mega_ibo_index_count_ = 0;
    mesh_registry_.clear();
    gpu_draw_cmd_capacity_ = 0;
    gpu_instance_capacity_ = 0;
}

} // namespace gameplay3d
} // namespace dse


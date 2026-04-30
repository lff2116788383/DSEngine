#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_inverse.hpp>
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
                    
                    constexpr int kDmeshVertexFloatStride = 20;
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
                    update_bounding_box(mesh_renderer.temp_vertices, kDmeshVertexFloatStride);
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
        if (point_lights.size() >= 4) break; // MAX_POINT_LIGHTS = 4
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

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& mesh_renderer = view.get<MeshRendererComponent>(entity);
        
        EnsureMeshPathDataLoaded(asset_manager, world, entity, mesh_renderer);
        if (!mesh_renderer.visible) continue;
        
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
                mesh_renderer.albedo_texture_handle,
                mesh_renderer.normal_texture_handle,
                mesh_renderer.metallic_roughness_texture_handle,
                mesh_renderer.emissive_texture_handle,
                mesh_renderer.occlusion_texture_handle
            };
        const MaterialAsset::ScalarOverrides resolved_scalars = prefer_material_instance
            ? material_instance->GetScalarOverrides()
            : MaterialAsset::ScalarOverrides{
                mesh_renderer.metallic,
                mesh_renderer.roughness,
                mesh_renderer.ao,
                mesh_renderer.normal_strength,
                mesh_renderer.material_alpha_cutoff
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
        item.debug_label = ClassifyVse1522MeshForDiagnostics(mesh_renderer);
        item.model = glm::mat4(1.0f);
        item.blend_mode = static_cast<unsigned int>(resolved_blend_mode);
        item.sorting_layer = mesh_renderer.sorting_layer;
        item.order_in_layer = mesh_renderer.order_in_layer;
        item.lighting_enabled = resolved_shader_variant != "MESH_UNLIT";
        
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

        if (!mesh_renderer.temp_vertices.empty() && !mesh_renderer.temp_indices.empty()) {
            const bool emit_vse1522_depth_diag = ShouldLogVse1522MeshDiagnostics(mesh_renderer);
            const bool is_dmesh_format = mesh_renderer.mesh_path.find(".dmesh") != std::string::npos;
            const std::size_t vertex_count_from_pos3 = mesh_renderer.temp_vertices.size() / 3;
            const bool has_lua_uvs = !is_dmesh_format && vertex_count_from_pos3 > 0 && mesh_renderer.temp_uvs.size() == vertex_count_from_pos3 * 2;
            const bool has_lua_normals = !is_dmesh_format && vertex_count_from_pos3 > 0 && mesh_renderer.temp_normals.size() == vertex_count_from_pos3 * 3;
            const bool has_lua_tangents = !is_dmesh_format && vertex_count_from_pos3 > 0 && mesh_renderer.temp_tangents.size() == vertex_count_from_pos3 * 3;
            size_t stride = is_dmesh_format ? 20 : 3;
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
                bv.pos = item.skinned ? local_positions[i] : world_pos3;
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
                bv.normal = item.skinned ? normal : glm::normalize(normal_matrix * normal);
                item.vertices.push_back(bv);
            }
            item.indices = mesh_renderer.temp_indices;
            item.debug_world_bounds_min = world_min;
            item.debug_world_bounds_max = world_max;
            if (emit_vse1522_depth_diag) {
                static int vse1522_diag_frame = 0;
                static int vse1522_diag_logs = 0;
                if (vse1522_diag_logs < 12) {
                    DEBUG_LOG_INFO("[3D][VSE15.22][DepthDiag][MeshRenderSystem] seq={} label={} entity={} skinned={} mesh_path={} depth_test={} depth_write={} vertices={} indices={} world_min=({},{},{}) world_max=({},{},{}) local_stride={} material_double_sided={} sorting=({}, {}) note=skinned_bounds_are_bind_mesh_model_bounds",
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
                                   item.order_in_layer);
                    ++vse1522_diag_logs;
                }
                ++vse1522_diag_frame;
            }
        }

        if (!item.vertices.empty() && !item.indices.empty()) {
            batch_items.push_back(item);
        }
    }

    if (!batch_items.empty()) {
        std::sort(batch_items.begin(), batch_items.end(), [](const MeshDrawItem& a, const MeshDrawItem& b) {
            if (a.sorting_layer != b.sorting_layer) {
                return a.sorting_layer < b.sorting_layer;
            }
            if (a.order_in_layer != b.order_in_layer) {
                return a.order_in_layer < b.order_in_layer;
            }
            return a.model[3].z < b.model[3].z;
        });
        cmd_buffer.DrawMeshBatch(batch_items);
    }
}

} // namespace gameplay3d
} // namespace dse


#include "modules/gameplay_3d/destruction/fracture_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include "engine/physics/physics3d/i_physics3d_system.h"
#include <glm/gtc/quaternion.hpp>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>

// Minimal JSON parsing for fracture descriptor
// Format:
// {
//   "source_mesh": "path/to/original.dmesh",
//   "fragments": [
//     { "mesh": "path/to/frag_00.dmesh", "offset": [x, y, z], "volume": 1.0 },
//     ...
//   ]
// }

namespace {

/// Trim whitespace from both ends
std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n\"");
    size_t end = s.find_last_not_of(" \t\r\n\"");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

/// Extract value for a given key from a simple JSON-like line
std::string ExtractValue(const std::string& line, const std::string& key) {
    size_t pos = line.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = line.find(':', pos);
    if (pos == std::string::npos) return "";
    return Trim(line.substr(pos + 1));
}

/// Parse [x, y, z] array from string
glm::vec3 ParseVec3(const std::string& s) {
    glm::vec3 v(0.0f);
    size_t start = s.find('[');
    size_t end = s.find(']');
    if (start == std::string::npos || end == std::string::npos) return v;
    std::string inner = s.substr(start + 1, end - start - 1);

    // Split by comma
    size_t p1 = inner.find(',');
    if (p1 == std::string::npos) return v;
    size_t p2 = inner.find(',', p1 + 1);
    if (p2 == std::string::npos) return v;

    v.x = std::stof(Trim(inner.substr(0, p1)));
    v.y = std::stof(Trim(inner.substr(p1 + 1, p2 - p1 - 1)));
    v.z = std::stof(Trim(inner.substr(p2 + 1)));
    return v;
}

/// Parse float value, stripping trailing comma/brackets
float ParseFloat(const std::string& s) {
    std::string cleaned;
    for (char c : s) {
        if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E')
            cleaned += c;
    }
    if (cleaned.empty()) return 1.0f;
    return std::stof(cleaned);
}

} // namespace

namespace dse {
namespace gameplay3d {

void FractureSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void FractureSystem::SetPhysics3D(physics3d::IPhysics3DSystem* physics3d) {
    physics3d_ = physics3d;
}

std::shared_ptr<FractureAsset> FractureSystem::LoadFractureAsset(const std::string& path) {
    // Check cache first
    auto it = fracture_asset_cache_.find(path);
    if (it != fracture_asset_cache_.end()) {
        return it->second;
    }

    // Load file
    std::vector<uint8_t> file_data;
    bool loaded = false;
    if (asset_manager_) {
        loaded = asset_manager_->LoadFileToMemory(path, file_data);
    }
    if (!loaded) {
        // Try direct file read
        std::string resolved_path = path;
        if (asset_manager_) {
            std::string r = asset_manager_->ResolveAssetPath(path);
            if (!r.empty()) resolved_path = r;
        }
        std::ifstream file(resolved_path, std::ios::binary);
        if (file.is_open()) {
            file_data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            loaded = true;
        }
    }

    if (!loaded || file_data.empty()) {
        DEBUG_LOG_ERROR("[FractureSystem] Failed to load fracture asset: {}", path);
        return nullptr;
    }

    // Parse JSON
    auto asset = std::make_shared<FractureAsset>();
    std::string content(file_data.begin(), file_data.end());
    std::istringstream stream(content);
    std::string line;

    bool in_fragments = false;
    FragmentDescriptor current_frag;
    bool has_frag = false;

    while (std::getline(stream, line)) {
        std::string trimmed = Trim(line);

        // Source mesh
        if (trimmed.find("source_mesh") != std::string::npos) {
            size_t colon = trimmed.find(':');
            if (colon != std::string::npos) {
                std::string val = trimmed.substr(colon + 1);
                // Remove trailing comma
                size_t comma = val.find_last_of(',');
                if (comma != std::string::npos) val.erase(comma);
                asset->source_mesh = Trim(val);
            }
        }

        // Start of fragments array
        if (trimmed.find("fragments") != std::string::npos && trimmed.find('[') != std::string::npos) {
            in_fragments = true;
            continue;
        }

        if (in_fragments) {
            // End of fragments array
            if (trimmed.find(']') != std::string::npos && trimmed.find('[') == std::string::npos) {
                if (has_frag) {
                    asset->fragments.push_back(current_frag);
                    has_frag = false;
                }
                in_fragments = false;
                continue;
            }

            // Start of fragment object
            if (trimmed == "{") {
                if (has_frag) {
                    asset->fragments.push_back(current_frag);
                }
                current_frag = FragmentDescriptor();
                has_frag = true;
                continue;
            }

            // End of fragment object
            if (trimmed == "}" || trimmed == "},") {
                continue;
            }

            // Fragment fields
            if (has_frag) {
                if (trimmed.find("mesh") != std::string::npos && trimmed.find("source_mesh") == std::string::npos) {
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string val = trimmed.substr(colon + 1);
                        size_t comma = val.find_last_of(',');
                        if (comma != std::string::npos) val.erase(comma);
                        current_frag.mesh_path = Trim(val);
                    }
                }
                if (trimmed.find("offset") != std::string::npos) {
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        current_frag.local_offset = ParseVec3(trimmed.substr(colon + 1));
                    }
                }
                if (trimmed.find("volume") != std::string::npos) {
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        current_frag.volume = ParseFloat(trimmed.substr(colon + 1));
                    }
                }
            }
        }
    }

    DEBUG_LOG_INFO("[FractureSystem] Loaded fracture asset '{}' with {} fragments",
                   path, asset->fragments.size());

    fracture_asset_cache_[path] = asset;
    return asset;
}

void FractureSystem::ApplyDamage(World& world, entt::entity entity, float damage,
                                  const glm::vec3& impact_point, const glm::vec3& impact_dir) {
    if (!world.registry().valid(entity)) return;
    if (!world.registry().all_of<FractureComponent>(entity)) return;

    auto& fc = world.registry().get<FractureComponent>(entity);
    if (fc.is_fractured) return;

    fc.impact_point = impact_point;
    fc.impact_direction = impact_dir;

    switch (fc.trigger_mode) {
        case FractureTriggerMode::ImpactForce:
            if (damage >= fc.break_force) {
                fc.fracture_requested = true;
            }
            break;
        case FractureTriggerMode::DamageAccumulation:
            fc.health -= damage;
            if (fc.health <= 0.0f) {
                fc.health = 0.0f;
                fc.fracture_requested = true;
            }
            break;
    }
}

void FractureSystem::TriggerFracture(World& world, entt::entity entity,
                                      const glm::vec3& impact_point, const glm::vec3& impact_dir) {
    if (!world.registry().valid(entity)) return;
    if (!world.registry().all_of<FractureComponent>(entity)) return;

    auto& fc = world.registry().get<FractureComponent>(entity);
    if (fc.is_fractured) return;

    fc.impact_point = impact_point;
    fc.impact_direction = impact_dir;
    fc.fracture_requested = true;
}

void FractureSystem::Update(World& world, float delta_time) {
    // 1. Process pending fracture requests
    auto fracture_view = world.registry().view<FractureComponent>();
    // Collect entities to fracture (avoid modifying registry during iteration)
    std::vector<entt::entity> to_fracture;
    for (auto entity : fracture_view) {
        auto& fc = fracture_view.get<FractureComponent>(entity);
        if (fc.fracture_requested && !fc.is_fractured) {
            to_fracture.push_back(entity);
        }
    }

    for (auto entity : to_fracture) {
        SpawnFragments(world, entity);
    }

    // 2. Update fragment lifecycle
    UpdateFragmentLifecycle(world, delta_time);
}

std::shared_ptr<FractureAsset> FractureSystem::ComputeRuntimeVoronoi(
    World& world, entt::entity entity,
    uint32_t fragment_count, uint32_t seed,
    bool cluster_near_impact, const glm::vec3& impact_point) {

    // 获取 mesh 顶点数据
    if (!world.registry().all_of<MeshRendererComponent>(entity)) {
        DEBUG_LOG_ERROR("[FractureSystem] RuntimeVoronoi: 实体 {} 无 MeshRendererComponent",
                       static_cast<uint32_t>(entity));
        return nullptr;
    }
    auto& mr = world.registry().get<MeshRendererComponent>(entity);
    if (mr.temp_vertices.empty() || mr.temp_indices.empty()) {
        DEBUG_LOG_ERROR("[FractureSystem] RuntimeVoronoi: 实体 {} 的 mesh 数据未加载",
                       static_cast<uint32_t>(entity));
        return nullptr;
    }

    bool is_dmesh = mr.mesh_path.find(".dmesh") != std::string::npos;
    size_t stride = is_dmesh ? static_cast<size_t>(mr.dmesh_vertex_stride) : 3;
    size_t vcount = mr.temp_vertices.size() / stride;
    if (vcount == 0 || fragment_count == 0) return nullptr;

    // 提取局部空间顶点位置
    std::vector<glm::vec3> positions(vcount);
    glm::vec3 bbox_min(std::numeric_limits<float>::max());
    glm::vec3 bbox_max(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < vcount; ++i) {
        positions[i] = glm::vec3(
            mr.temp_vertices[i * stride + 0],
            mr.temp_vertices[i * stride + 1],
            mr.temp_vertices[i * stride + 2]);
        bbox_min = glm::min(bbox_min, positions[i]);
        bbox_max = glm::max(bbox_max, positions[i]);
    }
    glm::vec3 bbox_size = bbox_max - bbox_min;

    // 生成 Voronoi 种子点
    uint32_t actual_seed = (seed != 0) ? seed :
        static_cast<uint32_t>(impact_point.x * 73856093.0f +
                              impact_point.y * 19349663.0f +
                              impact_point.z * 83492791.0f);
    std::mt19937 rng(actual_seed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::vector<glm::vec3> seeds(fragment_count);
    // 将冲击点转换到局部空间
    glm::vec3 local_impact = impact_point;
    if (world.registry().all_of<TransformComponent>(entity)) {
        auto& t = world.registry().get<TransformComponent>(entity);
        glm::mat4 inv_model = glm::inverse(
            glm::translate(glm::mat4(1.0f), t.position) *
            glm::mat4_cast(t.rotation) *
            glm::scale(glm::mat4(1.0f), t.scale));
        local_impact = glm::vec3(inv_model * glm::vec4(impact_point, 1.0f));
    }

    for (uint32_t s = 0; s < fragment_count; ++s) {
        glm::vec3 pt;
        if (cluster_near_impact && s < fragment_count / 2) {
            // 前半数种子集中在冲击点附近（半径为 bbox 尺寸的 30%）
            float spread = glm::length(bbox_size) * 0.3f;
            pt = local_impact + glm::vec3(
                (dist01(rng) - 0.5f) * spread,
                (dist01(rng) - 0.5f) * spread,
                (dist01(rng) - 0.5f) * spread);
            pt = glm::clamp(pt, bbox_min, bbox_max);
        } else {
            // 其余种子均匀分布在 bbox 内
            pt = bbox_min + glm::vec3(
                dist01(rng) * bbox_size.x,
                dist01(rng) * bbox_size.y,
                dist01(rng) * bbox_size.z);
        }
        seeds[s] = pt;
    }

    // 将每个顶点分配到最近的种子 → 碎片标签
    std::vector<uint32_t> vertex_labels(vcount, 0);
    for (size_t i = 0; i < vcount; ++i) {
        float best_dist = std::numeric_limits<float>::max();
        for (uint32_t s = 0; s < fragment_count; ++s) {
            float d = glm::distance(positions[i], seeds[s]);
            if (d < best_dist) {
                best_dist = d;
                vertex_labels[i] = s;
            }
        }
    }

    // 按碎片标签分组三角形，构建碎片
    auto asset = std::make_shared<FractureAsset>();
    asset->source_mesh = mr.mesh_path;

    for (uint32_t frag_id = 0; frag_id < fragment_count; ++frag_id) {
        // 找出属于该碎片的顶点
        std::vector<uint32_t> frag_vert_indices;
        std::vector<bool> vert_mask(vcount, false);
        for (size_t i = 0; i < vcount; ++i) {
            if (vertex_labels[i] == frag_id) {
                vert_mask[i] = true;
            }
        }

        // 找出三个顶点都属于该碎片的三角形
        std::vector<uint32_t> frag_tri_indices;
        for (size_t t = 0; t + 2 < mr.temp_indices.size(); t += 3) {
            uint32_t i0 = mr.temp_indices[t + 0];
            uint32_t i1 = mr.temp_indices[t + 1];
            uint32_t i2 = mr.temp_indices[t + 2];
            if (i0 < vcount && i1 < vcount && i2 < vcount &&
                vert_mask[i0] && vert_mask[i1] && vert_mask[i2]) {
                frag_tri_indices.push_back(mr.temp_indices[t + 0]);
                frag_tri_indices.push_back(mr.temp_indices[t + 1]);
                frag_tri_indices.push_back(mr.temp_indices[t + 2]);
            }
        }

        if (frag_tri_indices.empty()) continue;

        // 收集实际用到的顶点，计算质心
        std::unordered_map<uint32_t, uint32_t> old_to_new;
        std::vector<uint32_t> used_verts;
        for (auto idx : frag_tri_indices) {
            if (old_to_new.find(idx) == old_to_new.end()) {
                old_to_new[idx] = static_cast<uint32_t>(used_verts.size());
                used_verts.push_back(idx);
            }
        }

        // 计算碎片质心
        glm::vec3 centroid(0.0f);
        for (auto vi : used_verts) {
            centroid += positions[vi];
        }
        centroid /= static_cast<float>(used_verts.size());

        // 计算碎片 AABB 体积
        glm::vec3 frag_min(std::numeric_limits<float>::max());
        glm::vec3 frag_max(std::numeric_limits<float>::lowest());
        for (auto vi : used_verts) {
            frag_min = glm::min(frag_min, positions[vi]);
            frag_max = glm::max(frag_max, positions[vi]);
        }
        glm::vec3 frag_size = frag_max - frag_min;
        float volume = frag_size.x * frag_size.y * frag_size.z;
        if (volume < 1e-6f) volume = 1e-6f;

        // 构建碎片 mesh 数据（质心居中）—— 存入 MeshRendererComponent 的 temp_vertices/temp_indices
        // 这里不写 .dmesh 文件，而是直接缓存在内存中
        std::vector<float> frag_vertices;
        frag_vertices.resize(used_verts.size() * stride);
        for (size_t vi_idx = 0; vi_idx < used_verts.size(); ++vi_idx) {
            size_t src_vi = used_verts[vi_idx];
            // 复制全部顶点属性
            for (size_t j = 0; j < stride; ++j) {
                frag_vertices[vi_idx * stride + j] = mr.temp_vertices[src_vi * stride + j];
            }
            // 位置相对质心居中
            frag_vertices[vi_idx * stride + 0] -= centroid.x;
            frag_vertices[vi_idx * stride + 1] -= centroid.y;
            frag_vertices[vi_idx * stride + 2] -= centroid.z;
        }

        std::vector<uint32_t> frag_remapped_indices;
        frag_remapped_indices.reserve(frag_tri_indices.size());
        for (auto idx : frag_tri_indices) {
            frag_remapped_indices.push_back(old_to_new[idx]);
        }

        // 用唯一键缓存碎片数据
        // 仅当源 mesh 是 dmesh 格式（stride>=20）时才加 .dmesh 后缀，
        // 否则让 MeshRenderSystem 走非 dmesh 路径（自动计算法线/UV）
        std::string cache_key = "__runtime_voronoi_" +
            std::to_string(static_cast<uint32_t>(entity)) + "_frag_" +
            std::to_string(frag_id) + (is_dmesh ? ".dmesh" : "");

        FragmentDescriptor desc;
        desc.mesh_path = cache_key;
        desc.local_offset = centroid;
        desc.volume = volume;

        // 将碎片顶点数据存入缓存 map，供 SpawnFragments 使用
        desc.runtime_vertices = std::move(frag_vertices);
        desc.runtime_indices = std::move(frag_remapped_indices);
        desc.runtime_vertex_stride = static_cast<int>(stride);

        asset->fragments.push_back(std::move(desc));
    }

    DEBUG_LOG_INFO("[FractureSystem] 实时 Voronoi 切分实体 {} → {} 个碎片",
                   static_cast<uint32_t>(entity), asset->fragments.size());
    return asset;
}

void FractureSystem::SpawnFragments(World& world, entt::entity source_entity) {
    auto& fc = world.registry().get<FractureComponent>(source_entity);

    // 根据碎片来源模式加载/计算碎片数据
    if (!fc.cached_asset) {
        if (fc.source == FractureSource::RuntimeVoronoi) {
            fc.cached_asset = ComputeRuntimeVoronoi(
                world, source_entity,
                fc.runtime_fragment_count, fc.runtime_seed,
                fc.cluster_near_impact, fc.impact_point);
        } else {
            fc.cached_asset = LoadFractureAsset(fc.fracture_asset_path);
        }
    }
    if (!fc.cached_asset || fc.cached_asset->fragments.empty()) {
        DEBUG_LOG_ERROR("[FractureSystem] 实体 {} 无碎片数据", static_cast<uint32_t>(source_entity));
        fc.fracture_requested = false;
        return;
    }

    // Get source transform
    glm::vec3 source_pos(0.0f);
    glm::quat source_rot(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 source_scale(1.0f);
    if (world.registry().all_of<TransformComponent>(source_entity)) {
        const auto& t = world.registry().get<TransformComponent>(source_entity);
        source_pos = t.position;
        source_rot = t.rotation;
        source_scale = t.scale;
    }

    // Get source material settings for inheritance
    std::string shader_variant = fc.fragment_shader_variant;
    glm::vec4 color(1.0f);
    float metallic = 0.0f, roughness = 0.5f, ao = 1.0f;
    unsigned int albedo_tex = 0, normal_tex = 0;
    if (fc.inherit_material && world.registry().all_of<MeshRendererComponent>(source_entity)) {
        const auto& mr = world.registry().get<MeshRendererComponent>(source_entity);
        shader_variant = mr.shader_variant;
        color = mr.color;
        metallic = mr.metallic;
        roughness = mr.roughness;
        ao = mr.ao;
        albedo_tex = mr.albedo_texture_handle;
        normal_tex = mr.normal_texture_handle;
    }

    // Calculate total volume for mass distribution
    float total_volume = 0.0f;
    for (const auto& frag : fc.cached_asset->fragments) {
        total_volume += frag.volume;
    }
    if (total_volume <= 0.0f) total_volume = 1.0f;

    // Get source mass (if it had a rigid body)
    float source_mass = 1.0f;
    if (world.registry().all_of<RigidBody3DComponent>(source_entity)) {
        source_mass = world.registry().get<RigidBody3DComponent>(source_entity).mass;
    }

    const auto& fragments = fc.cached_asset->fragments;

    // RNG for CPU fallback physics angular velocity
    std::mt19937 rng(static_cast<uint32_t>(source_entity) * 31337u + 42u);

    for (size_t i = 0; i < fragments.size(); ++i) {
        const auto& frag_desc = fragments[i];

        auto frag_entity = world.registry().create();

        // Transform: source position + rotated local offset
        glm::vec3 rotated_offset = source_rot * (frag_desc.local_offset * source_scale);
        TransformComponent tc;
        tc.position = source_pos + rotated_offset;
        tc.rotation = source_rot;
        tc.scale = source_scale;
        tc.dirty = true;
        world.registry().emplace<TransformComponent>(frag_entity, tc);

        // Mesh renderer
        MeshRendererComponent frag_mr;
        frag_mr.shader_variant = shader_variant;
        frag_mr.color = color;
        frag_mr.metallic = metallic;
        frag_mr.roughness = roughness;
        frag_mr.ao = ao;
        frag_mr.albedo_texture_handle = albedo_tex;
        frag_mr.normal_texture_handle = normal_tex;
        frag_mr.visible = true;
        frag_mr.receive_shadow = false; // 碎片间深度极接近，receive_shadow 会导致 shadow acne 闪烁
        frag_mr.material_double_sided = true; // Voronoi 碎片是开放 mesh，需要双面渲染避免旋转时闪烁

        if (frag_desc.runtime_vertex_stride > 0 && !frag_desc.runtime_vertices.empty()) {
            // 运行时 Voronoi 碎片：直接使用内存中的顶点/索引数据
            frag_mr.mesh_path = frag_desc.mesh_path; // 缓存键（非真实文件）
            frag_mr.temp_vertices = frag_desc.runtime_vertices;
            frag_mr.temp_indices = frag_desc.runtime_indices;
            frag_mr.dmesh_vertex_stride = frag_desc.runtime_vertex_stride;
        } else {
            // 预切分碎片：通过 mesh_path 加载 .dmesh 文件
            frag_mr.mesh_path = frag_desc.mesh_path;
        }
        world.registry().emplace<MeshRendererComponent>(frag_entity, frag_mr);

        // Rigid body (dynamic)
        RigidBody3DComponent rb;
        rb.type = RigidBody3DType::Dynamic;
        rb.mass = (frag_desc.volume / total_volume) * source_mass * fc.fragment_mass_scale;
        if (rb.mass < 0.01f) rb.mass = 0.01f;
        rb.use_gravity = true;
        rb.drag = 0.1f;
        rb.angular_drag = 0.5f;
        world.registry().emplace<RigidBody3DComponent>(frag_entity, rb);

        // Box collider (simple approximation)
        BoxCollider3DComponent box;
        // Approximate fragment size from volume (cube root)
        float approx_size = std::cbrt(frag_desc.volume) * 0.5f;
        if (approx_size < 0.05f) approx_size = 0.05f;
        box.size = glm::vec3(approx_size);
        // Inherit material from source entity's collider (Task 2)
        if (world.registry().valid(source_entity) && world.registry().all_of<BoxCollider3DComponent>(source_entity)) {
            auto& src_box = world.registry().get<BoxCollider3DComponent>(source_entity);
            box.friction = src_box.friction;
            box.bounciness = src_box.bounciness;
        } else {
            box.friction = 0.5f;
            box.bounciness = 0.0f;
        }
        world.registry().emplace<BoxCollider3DComponent>(frag_entity, box);

        // Fragment tag for lifecycle management
        FragmentTagComponent tag;
        tag.source_entity_id = static_cast<uint32_t>(source_entity);
        tag.elapsed = 0.0f;
        tag.lifetime = fc.fragment_lifetime;
        tag.fade_duration = fc.fragment_fade_duration;
        tag.initial_alpha = color.a;
        // Apply explosion impulse — compute before emplace so we can set velocity
        glm::vec3 frag_velocity(0.0f);
        glm::vec3 frag_angular(0.0f);
        bool use_cpu_physics = false;
        if (fc.explosion_force > 0.0f) {
            glm::vec3 dir = tc.position - (source_pos + fc.impact_point * 0.5f);
            float dist = glm::length(dir);
            if (dist > 0.001f) {
                dir /= dist;
            } else {
                float angle = static_cast<float>(i) / static_cast<float>(fragments.size()) * 6.28318f;
                dir = glm::vec3(std::cos(angle), 0.5f, std::sin(angle));
            }
            glm::vec3 impulse = dir * fc.explosion_force * rb.mass;
            impulse.y += fc.explosion_force * rb.mass * 0.3f;
#ifdef DSE_HAS_PHYSICS3D
            if (physics3d_) {
                auto& emplace_rb = world.registry().get<RigidBody3DComponent>(frag_entity);
                emplace_rb.pending_impulse = impulse;
                emplace_rb.has_pending_impulse = true;
            } else
#endif
            {
                frag_velocity = impulse / rb.mass;
                std::uniform_real_distribution<float> ang_dist(-5.0f, 5.0f);
                frag_angular = glm::vec3(ang_dist(rng), ang_dist(rng), ang_dist(rng));
                use_cpu_physics = true;
            }
        }

        tag.velocity = frag_velocity;
        tag.angular_velocity = frag_angular;
        tag.cpu_physics = use_cpu_physics;
        world.registry().emplace<FragmentTagComponent>(frag_entity, tag);
    }

    // Mark source as fractured
    fc.is_fractured = true;
    fc.fracture_requested = false;

    // Hide original mesh (don't destroy entity — game logic may still reference it)
    if (world.registry().all_of<MeshRendererComponent>(source_entity)) {
        world.registry().get<MeshRendererComponent>(source_entity).visible = false;
    }
    // 移除 BoundingBox 防止视锥剔除系统覆盖 visible=false
    if (world.registry().all_of<BoundingBoxComponent>(source_entity)) {
        world.registry().remove<BoundingBoxComponent>(source_entity);
    }
    // Disable source rigid body by making it static and removing collider interaction
    if (world.registry().all_of<RigidBody3DComponent>(source_entity)) {
        auto& rb = world.registry().get<RigidBody3DComponent>(source_entity);
        rb.type = RigidBody3DType::Static;
        // Destroy existing PhysX actor so it gets recreated as Static
        // (it was originally created as Dynamic)
        if (rb.runtime_body) {
#ifdef DSE_HAS_PHYSICS3D
            if (physics3d_) {
                physics3d_->RemoveActor(source_entity);
            }
#endif
            rb.runtime_body = nullptr;
        }
    }

    DEBUG_LOG_INFO("[FractureSystem] Fractured entity {} into {} fragments",
                   static_cast<uint32_t>(source_entity), fragments.size());
}

void FractureSystem::UpdateFragmentLifecycle(World& world, float delta_time) {
    auto frag_view = world.registry().view<FragmentTagComponent>();
    std::vector<entt::entity> to_destroy;

    for (auto entity : frag_view) {
        auto& tag = frag_view.get<FragmentTagComponent>(entity);
        tag.elapsed += delta_time;

        float total_life = tag.lifetime + tag.fade_duration;

        if (tag.elapsed >= total_life) {
            // Fragment expired — schedule for destruction
            to_destroy.push_back(entity);
            continue;
        }

        // CPU fallback physics: gravity + velocity + rotation + floor collision
        if (tag.cpu_physics && world.registry().all_of<TransformComponent>(entity)) {
            auto& t = world.registry().get<TransformComponent>(entity);
            // Gravity
            tag.velocity.y -= 9.81f * delta_time;
            // Drag
            tag.velocity *= (1.0f - 0.5f * delta_time);
            // Integrate position
            t.position += tag.velocity * delta_time;
            // Integrate rotation (simplified)
            if (glm::length(tag.angular_velocity) > 0.01f) {
                float angle = glm::length(tag.angular_velocity) * delta_time;
                glm::vec3 axis = glm::normalize(tag.angular_velocity);
                t.rotation = glm::angleAxis(angle, axis) * t.rotation;
            }
            // Floor collision (y = 0)
            if (t.position.y < 0.0f) {
                t.position.y = 0.0f;
                tag.velocity.y = -tag.velocity.y * 0.3f; // Bounce with damping
                tag.velocity.x *= 0.8f; // Friction
                tag.velocity.z *= 0.8f;
                tag.angular_velocity *= 0.7f;
                // Stop bouncing if very slow
                if (std::abs(tag.velocity.y) < 0.5f) {
                    tag.velocity.y = 0.0f;
                    tag.angular_velocity = glm::vec3(0.0f);
                }
            }
            t.dirty = true;
        }

        // Fade phase
        if (tag.elapsed > tag.lifetime) {
            float fade_progress = (tag.elapsed - tag.lifetime) / tag.fade_duration;
            fade_progress = glm::clamp(fade_progress, 0.0f, 1.0f);
            float alpha = tag.initial_alpha * (1.0f - fade_progress);

            if (world.registry().all_of<MeshRendererComponent>(entity)) {
                auto& mr = world.registry().get<MeshRendererComponent>(entity);
                mr.color.a = alpha;
                // Hide completely when nearly transparent
                if (alpha < 0.01f) {
                    mr.visible = false;
                }
            }
        }
    }

    // Destroy expired fragments
    for (auto entity : to_destroy) {
        world.registry().destroy(entity);
    }
}

} // namespace gameplay3d
} // namespace dse

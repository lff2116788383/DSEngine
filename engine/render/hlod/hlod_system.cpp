/**
 * @file hlod_system.cpp
 * @brief HLOD 系统实现：离线构建 + 运行时切换
 */

#include "engine/render/hlod/hlod_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/base/debug.h"
#include <algorithm>
#include <fstream>
#include <cstring>
#include <cmath>
#include <numeric>
#include <filesystem>

namespace dse {
namespace render {

using dse::MeshRendererComponent;
using dse::asset::compiler::MeshHeader;
using dse::asset::compiler::SubMeshDesc;
using dse::asset::compiler::VertexAttribute;

// ─── HLODBuilder: 离线构建 ──────────────────────────────────────────────────

namespace {

/// 空间聚类：按 k-d 划分将 mesh 分组到 cluster_radius 内
struct SpatialBin {
    std::vector<uint32_t> mesh_indices;
    glm::vec3 center{0.0f};
};

std::vector<SpatialBin> SpatialCluster(const std::vector<HLODBuildMesh>& meshes,
                                        float cluster_radius) {
    // 计算每个 mesh 的中心
    std::vector<glm::vec3> centers(meshes.size());
    for (size_t i = 0; i < meshes.size(); ++i) {
        glm::vec3 sum{0.0f};
        for (const auto& p : meshes[i].positions) {
            glm::vec4 wp = meshes[i].transform * glm::vec4(p, 1.0f);
            sum += glm::vec3(wp);
        }
        if (!meshes[i].positions.empty()) {
            centers[i] = sum / static_cast<float>(meshes[i].positions.size());
        }
    }

    // 贪心聚类
    std::vector<bool> assigned(meshes.size(), false);
    std::vector<SpatialBin> bins;

    for (size_t i = 0; i < meshes.size(); ++i) {
        if (assigned[i]) continue;

        SpatialBin bin;
        bin.center = centers[i];
        bin.mesh_indices.push_back(static_cast<uint32_t>(i));
        assigned[i] = true;

        // 收集半径内未分配的 mesh
        for (size_t j = i + 1; j < meshes.size(); ++j) {
            if (assigned[j]) continue;
            float dist = glm::length(centers[j] - bin.center);
            if (dist <= cluster_radius) {
                bin.mesh_indices.push_back(static_cast<uint32_t>(j));
                assigned[j] = true;
            }
        }

        // 更新 bin 中心
        glm::vec3 avg{0.0f};
        for (uint32_t idx : bin.mesh_indices) avg += centers[idx];
        bin.center = avg / static_cast<float>(bin.mesh_indices.size());
        bins.push_back(std::move(bin));
    }

    return bins;
}

/// 将多个 mesh 合并为单个 mesh
HLODBuildMesh MergeMeshes(const std::vector<HLODBuildMesh>& meshes,
                           const std::vector<uint32_t>& indices) {
    HLODBuildMesh merged;
    merged.transform = glm::mat4(1.0f);

    for (uint32_t idx : indices) {
        const auto& src = meshes[idx];
        uint32_t base_vertex = static_cast<uint32_t>(merged.positions.size());

        for (const auto& p : src.positions) {
            glm::vec4 wp = src.transform * glm::vec4(p, 1.0f);
            merged.positions.push_back(glm::vec3(wp));
        }
        for (const auto& n : src.normals) {
            glm::vec3 wn = glm::normalize(glm::mat3(src.transform) * n);
            merged.normals.push_back(wn);
        }
        for (const auto& uv : src.texcoords) {
            merged.texcoords.push_back(uv);
        }
        for (uint32_t i : src.indices) {
            merged.indices.push_back(i + base_vertex);
        }
    }

    // 补齐 normals/texcoords 如果某些 mesh 没有
    while (merged.normals.size() < merged.positions.size()) {
        merged.normals.push_back(glm::vec3(0.0f, 1.0f, 0.0f));
    }
    while (merged.texcoords.size() < merged.positions.size()) {
        merged.texcoords.push_back(glm::vec2(0.0f));
    }

    return merged;
}

/// 计算 AABB
void ComputeBounds(const std::vector<glm::vec3>& positions,
                   glm::vec3& out_center, glm::vec3& out_extents) {
    if (positions.empty()) {
        out_center = glm::vec3(0.0f);
        out_extents = glm::vec3(0.0f);
        return;
    }
    glm::vec3 mn = positions[0], mx = positions[0];
    for (const auto& p : positions) {
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }
    out_center = (mn + mx) * 0.5f;
    out_extents = (mx - mn) * 0.5f;
}

/// 将 QEM 减面结果写出为 .dmesh（v1 格式，与 AssetBuilder importer 一致：
/// MeshHeader + SubMeshDesc[] + 80B/顶点交错布局 P|N|UV|W|J|T + uint32 索引）。
/// 返回是否写盘成功。
bool WriteProxyDmesh(const std::string& path, const dse::mesh::DecimationResult& mesh) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) return false;

    MeshHeader header;
    header.version = 1;
    header.attribute_mask = static_cast<uint32_t>(
        VertexAttribute::Position | VertexAttribute::Normal |
        VertexAttribute::Tangent | VertexAttribute::TexCoord |
        VertexAttribute::Joints | VertexAttribute::Weights);
    header.vertex_count = static_cast<uint32_t>(mesh.positions.size());
    header.index_count = static_cast<uint32_t>(mesh.indices.size());
    header.submesh_count = 1;

    glm::vec3 bmin(std::numeric_limits<float>::max());
    glm::vec3 bmax(std::numeric_limits<float>::lowest());
    for (const auto& p : mesh.positions) {
        bmin = glm::min(bmin, p);
        bmax = glm::max(bmax, p);
    }

    SubMeshDesc sub{};
    sub.index_count = header.index_count;
    sub.base_vertex = 0;
    sub.material_id = 0;
    sub.bounding_box_min = bmin;
    sub.bounding_box_max = bmax;

    // 布局与 RuntimeVertex（importer.cpp）一致：pos/normal/texcoord/weights/joints/tangent
    constexpr size_t kVertexBytes = 80;  // v1 无 color：12+12+8+16+16+16
    uint64_t offset = sizeof(MeshHeader);
    header.submesh_data_offset = offset;
    offset += sizeof(SubMeshDesc);
    header.vertex_data_offset = offset;
    offset += static_cast<uint64_t>(mesh.positions.size()) * kVertexBytes;
    header.index_data_offset = offset;

    out.write(reinterpret_cast<const char*>(&header), sizeof(MeshHeader));
    out.write(reinterpret_cast<const char*>(&sub), sizeof(SubMeshDesc));

    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        const glm::vec3 pos = mesh.positions[i];
        const glm::vec3 nrm = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec2 uv  = i < mesh.texcoords.size() ? mesh.texcoords[i] : glm::vec2(0.0f);
        const glm::vec4 weights(1.0f, 0.0f, 0.0f, 0.0f);
        const glm::ivec4 joints(0, 0, 0, 0);
        const glm::vec4 tangent(1.0f, 0.0f, 0.0f, 1.0f);
        out.write(reinterpret_cast<const char*>(&pos), 12);
        out.write(reinterpret_cast<const char*>(&nrm), 12);
        out.write(reinterpret_cast<const char*>(&uv), 8);
        out.write(reinterpret_cast<const char*>(&weights), 16);
        out.write(reinterpret_cast<const char*>(&joints), 16);
        out.write(reinterpret_cast<const char*>(&tangent), 16);
    }
    out.write(reinterpret_cast<const char*>(mesh.indices.data()),
              mesh.indices.size() * sizeof(uint32_t));

    return out.good();
}

} // anonymous namespace

HLODBuildResult HLODBuilder::Build(const std::vector<HLODBuildMesh>& meshes,
                                    const HLODBuildConfig& config) {
    HLODBuildResult result;
    if (meshes.empty()) {
        result.success = true;
        return result;
    }

    // Step 1: 空间聚类
    auto bins = SpatialCluster(meshes, config.cluster_radius);

    // Step 2: 为每个 bin 构建 HLOD 层级
    for (const auto& bin : bins) {
        HLODCluster cluster;
        cluster.name = "cluster_" + std::to_string(result.clusters.size());

        // 记录源实体（通过 entity_index）
        for (uint32_t idx : bin.mesh_indices) {
            cluster.source_entities.push_back(Entity{static_cast<entt::entity>(meshes[idx].entity_index)});
        }

        // 合并 mesh
        HLODBuildMesh merged = MergeMeshes(meshes, bin.mesh_indices);
        ComputeBounds(merged.positions, cluster.bounds_center, cluster.bounds_extents);

        // 为每一级生成简化 proxy
        float current_ratio = 1.0f;
        float current_distance = config.base_distance;

        // 用 QEM 减面器对合并网格一次性生成各级 LOD 几何（累计比率逐级递减）。
        // 此前只计算 target_tris 写入路径字符串，从不生成几何（技术债 B-4）。
        dse::mesh::LodGenerationResult lod_result;
        if (merged.positions.size() >= 3 && merged.indices.size() >= 3) {
            dse::mesh::DecimationInput dec_in{};
            dec_in.positions = merged.positions.data();
            dec_in.normals = merged.normals.data();
            dec_in.texcoords = merged.texcoords.data();
            dec_in.vertex_count = static_cast<uint32_t>(merged.positions.size());
            dec_in.indices = merged.indices.data();
            dec_in.index_count = static_cast<uint32_t>(merged.indices.size());

            dse::mesh::LodGenerationConfig lod_cfg;
            float acc = 1.0f;
            for (uint32_t l = 0; l < config.hlod_levels; ++l) {
                acc *= config.simplify_ratio;
                lod_cfg.level_ratios.push_back(acc);
            }
            lod_result = dse::mesh::MeshDecimator().GenerateLods(dec_in, lod_cfg);
        }

        for (uint32_t level = 0; level < config.hlod_levels; ++level) {
            current_ratio *= config.simplify_ratio;

            HLODProxy proxy;
            proxy.switch_distance = current_distance;
            proxy.bounds_center = cluster.bounds_center;
            proxy.bounds_extents = cluster.bounds_extents;

            uint32_t target_tris = static_cast<uint32_t>(
                (merged.indices.size() / 3) * current_ratio);
            if (target_tris < 4) target_tris = 4;

            proxy.triangle_count = target_tris;
            proxy.mesh_path = "hlod/" + cluster.name + "_lod" + std::to_string(level) + ".dmesh";
            proxy.material_path = "hlod/" + cluster.name + "_mat.dmat";

            // 减面成功且几何非空：写入真实几何；否则回退合并原始几何
            // （QEM 对孤立方块等退化网格可能折叠到 0 三角形——每折叠删 2 个
            // 共享面，孤立组件不足时归零；回退保证每级都有有效代理）。
            const dse::mesh::DecimationResult* dec = nullptr;
            if (lod_result.success && level < lod_result.levels.size() &&
                lod_result.levels[level].success &&
                !lod_result.levels[level].positions.empty()) {
                dec = &lod_result.levels[level];
                proxy.positions = dec->positions;
                proxy.normals = dec->normals;
                proxy.texcoords = dec->texcoords;
                proxy.indices = dec->indices;
                proxy.triangle_count = dec->result_triangle_count;
            } else {
                proxy.positions = merged.positions;
                proxy.normals = merged.normals;
                proxy.texcoords = merged.texcoords;
                proxy.indices = merged.indices;
                proxy.triangle_count = static_cast<uint32_t>(merged.indices.size() / 3);
            }

            // 落盘 .dmesh（输出根非空时）
            if (!config.output_dir.empty() && !proxy.indices.empty()) {
                dse::mesh::DecimationResult disk_mesh;
                if (dec) {
                    disk_mesh = *dec;
                } else {
                    disk_mesh.positions = proxy.positions;
                    disk_mesh.normals = proxy.normals;
                    disk_mesh.texcoords = proxy.texcoords;
                    disk_mesh.indices = proxy.indices;
                    disk_mesh.result_triangle_count = proxy.triangle_count;
                    disk_mesh.success = true;
                }
                std::filesystem::path out_path =
                    std::filesystem::path(config.output_dir) / proxy.mesh_path;
                std::error_code ec;
                std::filesystem::create_directories(out_path.parent_path(), ec);
                if (!WriteProxyDmesh(out_path.string(), disk_mesh)) {
                    DEBUG_LOG_WARN("[HLOD] Failed to write proxy mesh: {}", out_path.string());
                }
            }

            cluster.levels.push_back(proxy);
            current_distance *= config.level_distance_multiplier;
        }

        result.clusters.push_back(std::move(cluster));
    }

    result.success = true;
    return result;
}

// ─── 序列化 ─────────────────────────────────────────────────────────────────

static constexpr char kHLODMagic[4] = {'D', 'H', 'L', 'D'};
static constexpr uint32_t kHLODVersion = 1;

bool HLODBuilder::SaveToFile(const std::vector<HLODCluster>& clusters,
                              const std::string& output_path) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(kHLODMagic, 4);
    file.write(reinterpret_cast<const char*>(&kHLODVersion), 4);

    uint32_t cluster_count = static_cast<uint32_t>(clusters.size());
    file.write(reinterpret_cast<const char*>(&cluster_count), 4);

    for (const auto& cluster : clusters) {
        // Name (length-prefixed)
        uint32_t name_len = static_cast<uint32_t>(cluster.name.size());
        file.write(reinterpret_cast<const char*>(&name_len), 4);
        file.write(cluster.name.data(), name_len);

        // Bounds
        file.write(reinterpret_cast<const char*>(&cluster.bounds_center), 12);
        file.write(reinterpret_cast<const char*>(&cluster.bounds_extents), 12);

        // Source entity count
        uint32_t src_count = static_cast<uint32_t>(cluster.source_entities.size());
        file.write(reinterpret_cast<const char*>(&src_count), 4);
        for (auto e : cluster.source_entities) {
            uint32_t eidx = static_cast<uint32_t>(e);
            file.write(reinterpret_cast<const char*>(&eidx), 4);
        }

        // Levels
        uint32_t level_count = static_cast<uint32_t>(cluster.levels.size());
        file.write(reinterpret_cast<const char*>(&level_count), 4);
        for (const auto& lvl : cluster.levels) {
            uint32_t mesh_len = static_cast<uint32_t>(lvl.mesh_path.size());
            file.write(reinterpret_cast<const char*>(&mesh_len), 4);
            file.write(lvl.mesh_path.data(), mesh_len);

            uint32_t mat_len = static_cast<uint32_t>(lvl.material_path.size());
            file.write(reinterpret_cast<const char*>(&mat_len), 4);
            file.write(lvl.material_path.data(), mat_len);

            file.write(reinterpret_cast<const char*>(&lvl.switch_distance), 4);
            file.write(reinterpret_cast<const char*>(&lvl.triangle_count), 4);
            file.write(reinterpret_cast<const char*>(&lvl.bounds_center), 12);
            file.write(reinterpret_cast<const char*>(&lvl.bounds_extents), 12);
        }
    }

    return file.good();
}

bool HLODBuilder::LoadFromFile(const std::string& path,
                                std::vector<HLODCluster>& out_clusters) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, kHLODMagic, 4) != 0) return false;

    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), 4);
    if (version != kHLODVersion) return false;

    uint32_t cluster_count;
    file.read(reinterpret_cast<char*>(&cluster_count), 4);

    out_clusters.resize(cluster_count);
    for (uint32_t c = 0; c < cluster_count; ++c) {
        auto& cluster = out_clusters[c];

        uint32_t name_len;
        file.read(reinterpret_cast<char*>(&name_len), 4);
        cluster.name.resize(name_len);
        file.read(cluster.name.data(), name_len);

        file.read(reinterpret_cast<char*>(&cluster.bounds_center), 12);
        file.read(reinterpret_cast<char*>(&cluster.bounds_extents), 12);

        uint32_t src_count;
        file.read(reinterpret_cast<char*>(&src_count), 4);
        cluster.source_entities.resize(src_count);
        for (uint32_t i = 0; i < src_count; ++i) {
            uint32_t eidx;
            file.read(reinterpret_cast<char*>(&eidx), 4);
            cluster.source_entities[i] = Entity{static_cast<entt::entity>(eidx)};
        }

        uint32_t level_count;
        file.read(reinterpret_cast<char*>(&level_count), 4);
        cluster.levels.resize(level_count);
        for (uint32_t l = 0; l < level_count; ++l) {
            auto& lvl = cluster.levels[l];

            uint32_t mesh_len;
            file.read(reinterpret_cast<char*>(&mesh_len), 4);
            lvl.mesh_path.resize(mesh_len);
            file.read(lvl.mesh_path.data(), mesh_len);

            uint32_t mat_len;
            file.read(reinterpret_cast<char*>(&mat_len), 4);
            lvl.material_path.resize(mat_len);
            file.read(lvl.material_path.data(), mat_len);

            file.read(reinterpret_cast<char*>(&lvl.switch_distance), 4);
            file.read(reinterpret_cast<char*>(&lvl.triangle_count), 4);
            file.read(reinterpret_cast<char*>(&lvl.bounds_center), 12);
            file.read(reinterpret_cast<char*>(&lvl.bounds_extents), 12);
        }
    }

    return file.good();
}

// ─── HLODSystem: 运行时 ────────────────────────────────────────────────────

bool HLODSystem::Init(::World& world, const std::string& hlod_data_path) {
    if (hlod_data_path.empty()) return false;

    if (!HLODBuilder::LoadFromFile(hlod_data_path, clusters_)) {
        return false;
    }

    // 为每个 cluster 创建隐藏的 proxy 实体
    auto& registry = world.registry();
    for (auto& cluster : clusters_) {
        Entity proxy = Entity{registry.create()};
        registry.emplace<TransformComponent>(static_cast<entt::entity>(proxy));
        auto& tf = registry.get<TransformComponent>(static_cast<entt::entity>(proxy));
        tf.position = cluster.bounds_center;

        // 初始隐藏 proxy
        MeshRendererComponent mr;
        mr.visible = false;
        if (!cluster.levels.empty()) {
            mr.mesh_path = cluster.levels[0].mesh_path;
        }
        registry.emplace<MeshRendererComponent>(static_cast<entt::entity>(proxy), mr);

        cluster.proxy_entity = proxy;
        cluster.active_level = -1;
    }

    initialized_ = true;
    return true;
}

void HLODSystem::Update(::World& world, const glm::vec3& camera_pos) {
    if (!initialized_) return;

    for (auto& cluster : clusters_) {
        // 计算相机到簇中心的距离
        float dist = glm::length(camera_pos - cluster.bounds_center);
        dist *= distance_scale_;

        // 决定目标层级
        int target_level = -1; // -1 = 原始物体
        for (int i = static_cast<int>(cluster.levels.size()) - 1; i >= 0; --i) {
            float threshold = cluster.levels[i].switch_distance;
            // 考虑 hysteresis：升级（更精细）需要更近，降级（更简化）需要更远
            if (cluster.active_level > i) {
                threshold *= (1.0f + hysteresis_); // 从粗糙切到精细需要更近
            } else if (cluster.active_level < i) {
                threshold *= (1.0f - hysteresis_); // 从精细切到粗糙需要更远
            }

            if (dist >= threshold) {
                target_level = i;
                break;
            }
        }

        if (target_level != cluster.active_level) {
            SwitchClusterLevel(world, cluster, target_level);
        }
    }
}

void HLODSystem::SwitchClusterLevel(::World& world, HLODCluster& cluster, int new_level) {
    auto& registry = world.registry();

    if (new_level == -1) {
        // 显示原始物体，隐藏 proxy
        for (auto e : cluster.source_entities) {
            if (registry.valid(static_cast<entt::entity>(e))) {
                if (registry.all_of<MeshRendererComponent>(static_cast<entt::entity>(e))) {
                    registry.get<MeshRendererComponent>(static_cast<entt::entity>(e)).visible = true;
                }
                if (registry.all_of<HLODMemberComponent>(static_cast<entt::entity>(e))) {
                    registry.get<HLODMemberComponent>(static_cast<entt::entity>(e)).hidden_by_hlod = false;
                }
            }
        }
        // 隐藏 proxy 实体
        if (registry.valid(static_cast<entt::entity>(cluster.proxy_entity))) {
            if (registry.all_of<MeshRendererComponent>(static_cast<entt::entity>(cluster.proxy_entity))) {
                registry.get<MeshRendererComponent>(static_cast<entt::entity>(cluster.proxy_entity)).visible = false;
            }
        }
    } else {
        // 隐藏原始物体，显示 proxy
        for (auto e : cluster.source_entities) {
            if (registry.valid(static_cast<entt::entity>(e))) {
                if (registry.all_of<MeshRendererComponent>(static_cast<entt::entity>(e))) {
                    registry.get<MeshRendererComponent>(static_cast<entt::entity>(e)).visible = false;
                }
                if (registry.all_of<HLODMemberComponent>(static_cast<entt::entity>(e))) {
                    registry.get<HLODMemberComponent>(static_cast<entt::entity>(e)).hidden_by_hlod = true;
                }
            }
        }
        // 显示 proxy 并切换 mesh
        if (registry.valid(static_cast<entt::entity>(cluster.proxy_entity))) {
            if (registry.all_of<MeshRendererComponent>(static_cast<entt::entity>(cluster.proxy_entity))) {
                auto& mr = registry.get<MeshRendererComponent>(static_cast<entt::entity>(cluster.proxy_entity));
                mr.visible = true;
                mr.mesh_path = cluster.levels[new_level].mesh_path;
                mr.mesh_handle_override = 0; // 触发重新加载
            }
        }
    }

    cluster.active_level = new_level;
}

void HLODSystem::Shutdown(::World& world) {
    auto& registry = world.registry();

    // 恢复所有原始物体可见性
    for (auto& cluster : clusters_) {
        for (auto e : cluster.source_entities) {
            if (registry.valid(static_cast<entt::entity>(e))) {
                if (registry.all_of<MeshRendererComponent>(static_cast<entt::entity>(e))) {
                    registry.get<MeshRendererComponent>(static_cast<entt::entity>(e)).visible = true;
                }
            }
        }
        // 删除 proxy 实体
        if (registry.valid(static_cast<entt::entity>(cluster.proxy_entity))) {
            registry.destroy(static_cast<entt::entity>(cluster.proxy_entity));
        }
    }

    clusters_.clear();
    initialized_ = false;
}

size_t HLODSystem::ActiveProxyCount() const {
    size_t count = 0;
    for (const auto& cluster : clusters_) {
        if (cluster.active_level >= 0) ++count;
    }
    return count;
}

} // namespace render
} // namespace dse

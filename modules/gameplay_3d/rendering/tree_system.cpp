#include "modules/gameplay_3d/rendering/tree_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dse {
namespace gameplay3d {

// ============================================================
// 生命周期
// ============================================================

void TreeSystem::Init(RhiDevice* rhi_device) {
    rhi_ = rhi_device;
    DEBUG_LOG_INFO("[TreeSystem] Initialized");
}

void TreeSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void TreeSystem::Shutdown(World& /*world*/) {
    // 释放前向 pass 的 GPU 资源（须在 rhi_ 仍有效时）。
    if (rhi_) {
        for (auto& [path, entry] : mesh_cache_) {
            if (entry.tmpl.vertex_buffer) rhi_->DeleteGpuBuffer(entry.tmpl.vertex_buffer);
            if (entry.tmpl.index_buffer) rhi_->DeleteGpuBuffer(entry.tmpl.index_buffer);
        }
        mesh_renderer_.Shutdown(*rhi_);
    }
    entity_caches_.clear();
    mesh_cache_.clear();
    rhi_ = nullptr;
    asset_manager_ = nullptr;
}

// 懒建一份共享局部空间模板 GPU 缓冲（BatchVertex → MeshVertex，索引保持 32 位），供前向
// pass 的 DrawSharedTemplateInstanced 复用（一份模板顶点 + 每实例 model 矩阵）。
bool TreeSystem::EnsureTemplateBuilt(MeshCacheEntry& entry) {
    if (entry.gpu_template_built) {
        return entry.tmpl.vertex_buffer && entry.tmpl.index_buffer;
    }
    entry.gpu_template_built = true;  // 只尝试一次（失败也不反复重试）
    if (!rhi_ || entry.vertices.empty() || entry.indices.empty()) return false;

    std::vector<dse::render::MeshVertex> verts;
    verts.reserve(entry.vertices.size());
    for (const auto& bv : entry.vertices) {
        dse::render::MeshVertex mv;
        mv.position = bv.pos;
        mv.color = bv.color;
        mv.uv = bv.uv;
        mv.normal = bv.normal;
        mv.tangent = bv.tangent;
        verts.push_back(mv);
    }
    entry.tmpl.vertex_buffer = dse::render::MeshRenderer::BuildShadedLocalVertexBuffer(*rhi_, verts);

    dse::render::GpuBufferDesc ib_desc;
    ib_desc.size = entry.indices.size() * sizeof(uint32_t);
    ib_desc.usage = dse::render::GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = false;
    entry.tmpl.index_buffer = rhi_->CreateGpuBuffer(ib_desc, entry.indices.data());
    entry.tmpl.index_type = IndexType::UInt32;
    entry.index_count = static_cast<uint32_t>(entry.indices.size());
    return entry.tmpl.vertex_buffer && entry.tmpl.index_buffer;
}

// ============================================================
// Mesh 加载缓存
// ============================================================

bool TreeSystem::EnsureMeshLoaded(const std::string& mesh_path) {
    if (mesh_path.empty()) return false;
    if (mesh_cache_.find(mesh_path) != mesh_cache_.end()) return true;
    if (!asset_manager_) return false;

    auto dmesh = asset_manager_->LoadDmesh(mesh_path);
    if (!dmesh || dmesh->GetData().empty()) {
        DEBUG_LOG_ERROR("[TreeSystem] Failed to load mesh: {}", mesh_path);
        return false;
    }

    const uint8_t* data = dmesh->GetData().data();
    const auto* header = reinterpret_cast<const dse::asset::compiler::MeshHeader*>(data);
    if (header->magic[0] != 'D' || header->magic[1] != 'S' ||
        header->magic[2] != 'E' || header->magic[3] != 'M') {
        DEBUG_LOG_ERROR("[TreeSystem] Invalid dmesh magic: {}", mesh_path);
        return false;
    }

    if (header->submesh_count == 0 || header->vertex_count == 0) {
        DEBUG_LOG_ERROR("[TreeSystem] Empty dmesh: {}", mesh_path);
        return false;
    }

    const int stride = (header->version >= 2) ? 24 : 20;
    const float* vertices = reinterpret_cast<const float*>(data + header->vertex_data_offset);
    const uint32_t* indices = reinterpret_cast<const uint32_t*>(data + header->index_data_offset);
    const auto* submeshes = reinterpret_cast<const dse::asset::compiler::SubMeshDesc*>(data + header->submesh_data_offset);

    MeshCacheEntry entry;
    entry.vertices.reserve(header->vertex_count);

    for (uint32_t i = 0; i < header->vertex_count; ++i) {
        const float* v = vertices + static_cast<size_t>(i) * stride;
        BatchVertex bv;
        bv.pos = glm::vec3(v[0], v[1], v[2]);
        bv.normal = glm::vec3(v[3], v[4], v[5]);
        if (glm::length(bv.normal) < 1e-6f)
            bv.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        else
            bv.normal = glm::normalize(bv.normal);
        bv.uv = glm::vec2(v[6], v[7]);
        bv.weights = glm::vec4(v[8], v[9], v[10], v[11]);

        int j0, j1, j2, j3;
        std::memcpy(&j0, &v[12], sizeof(float));
        std::memcpy(&j1, &v[13], sizeof(float));
        std::memcpy(&j2, &v[14], sizeof(float));
        std::memcpy(&j3, &v[15], sizeof(float));
        bv.joints = glm::vec4(
            static_cast<float>(j0), static_cast<float>(j1),
            static_cast<float>(j2), static_cast<float>(j3));

        if (stride >= 20)
            bv.tangent = glm::vec3(v[16], v[17], v[18]);
        else
            bv.tangent = glm::vec3(1.0f, 0.0f, 0.0f);

        if (stride >= 24)
            bv.color = glm::vec4(v[20], v[21], v[22], v[23]);
        else
            bv.color = glm::vec4(1.0f);

        entry.vertices.push_back(bv);
    }

    for (uint32_t si = 0; si < header->submesh_count; ++si) {
        const auto& sub = submeshes[si];
        for (uint32_t i = 0; i < sub.index_count; ++i) {
            uint32_t resolved = sub.base_vertex + indices[sub.index_start + i];
            if (resolved >= header->vertex_count) {
                DEBUG_LOG_ERROR("[TreeSystem] Index out of range in dmesh: {}", mesh_path);
                return false;
            }
            entry.indices.push_back(resolved);
        }
    }

    DEBUG_LOG_INFO("[TreeSystem] Mesh loaded: {} (verts={}, indices={})",
                   mesh_path, entry.vertices.size(), entry.indices.size());
    mesh_cache_[mesh_path] = std::move(entry);
    return true;
}

// ============================================================
// Chunk 空间辅助
// ============================================================

uint64_t TreeSystem::ChunkKey(int cx, int cz) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cz));
}

void TreeSystem::ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out_planes[6]) {
    for (int i = 0; i < 4; ++i) {
        out_planes[0][i] = vp[i][3] + vp[i][0]; // left
        out_planes[1][i] = vp[i][3] - vp[i][0]; // right
        out_planes[2][i] = vp[i][3] + vp[i][1]; // bottom
        out_planes[3][i] = vp[i][3] - vp[i][1]; // top
        out_planes[4][i] = vp[i][3] + vp[i][2]; // near
        out_planes[5][i] = vp[i][3] - vp[i][2]; // far
    }
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(out_planes[i]));
        if (len > 1e-6f) out_planes[i] /= len;
    }
}

bool TreeSystem::IsAABBInFrustum(const glm::vec4 planes[6],
                                  const glm::vec3& aabb_min,
                                  const glm::vec3& aabb_max) {
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p(
            planes[i].x > 0.0f ? aabb_max.x : aabb_min.x,
            planes[i].y > 0.0f ? aabb_max.y : aabb_min.y,
            planes[i].z > 0.0f ? aabb_max.z : aabb_min.z);
        if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0.0f)
            return false;
    }
    return true;
}

// ============================================================
// Chunk 实例生成
// ============================================================

void TreeSystem::GenerateChunkInstances(const TreeComponent& tree,
                                         const TerrainComponent* terrain,
                                         const TransformComponent* terrain_tf,
                                         const TransformComponent& tree_tf,
                                         int cx, int cz,
                                         TreeChunkData& out) {
    out.layouts.clear();
    out.valid = true;

    const float chunk = tree.chunk_size;
    const float x0 = static_cast<float>(cx) * chunk;
    const float z0 = static_cast<float>(cz) * chunk;

    const float area = chunk * chunk;
    const int count = std::max(1, static_cast<int>(area * tree.density));

    // Deterministic RNG based on seed + chunk coords
    uint32_t h = tree.seed ^ (static_cast<uint32_t>(cx) * 73856093u) ^ (static_cast<uint32_t>(cz) * 19349663u);

    float min_y = 1e9f, max_y = -1e9f;
    out.layouts.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        h ^= (h << 13); h ^= (h >> 17); h ^= (h << 5);
        float rx = static_cast<float>(h & 0xFFFF) / 65535.0f;
        h ^= (h << 13); h ^= (h >> 17); h ^= (h << 5);
        float rz = static_cast<float>(h & 0xFFFF) / 65535.0f;
        h ^= (h << 13); h ^= (h >> 17); h ^= (h << 5);
        float r_yaw = static_cast<float>(h & 0xFFFF) / 65535.0f;
        h ^= (h << 13); h ^= (h >> 17); h ^= (h << 5);
        float r_scale = static_cast<float>(h & 0xFFFF) / 65535.0f;

        float wx = x0 + rx * chunk + tree_tf.position.x;
        float wz = z0 + rz * chunk + tree_tf.position.z;
        float wy = tree_tf.position.y;

        if (terrain && terrain_tf) {
            float sampled = dse::SampleTerrainHeight(*terrain, *terrain_tf, wx, wz);
            if (sampled != 0.0f || !terrain->height_data.empty())
                wy = sampled;
        }

        TreeInstanceLayout inst;
        inst.position = glm::vec3(wx, wy, wz);
        inst.yaw = tree.random_rotation ? (r_yaw * 6.2831853f) : 0.0f;
        inst.scale = tree.min_scale + r_scale * (tree.max_scale - tree.min_scale);
        out.layouts.push_back(inst);

        float estimated_height = inst.scale * 5.0f;
        min_y = std::min(min_y, wy);
        max_y = std::max(max_y, wy + estimated_height);
    }

    out.aabb_min = glm::vec3(x0 + tree_tf.position.x, min_y, z0 + tree_tf.position.z);
    out.aabb_max = glm::vec3(x0 + chunk + tree_tf.position.x, max_y, z0 + chunk + tree_tf.position.z);
}

// ============================================================
// Update: 增量 chunk 缓存维护
// ============================================================

void TreeSystem::Update(World& world, float /*delta_time*/) {
    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();
    glm::vec3 camera_pos(0.0f);
    bool has_camera = false;
    for (auto cam_entity : camera_view) {
        const auto& cam = camera_view.get<Camera3DComponent>(cam_entity);
        if (!cam.enabled) continue;
        const auto& cam_t = camera_view.get<TransformComponent>(cam_entity);
        camera_pos = glm::vec3(cam_t.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        has_camera = true;
        break;
    }
    if (!has_camera) return;

    const TerrainComponent* terrain = nullptr;
    const TransformComponent* terrain_tf = nullptr;
    {
        auto terrain_view = world.registry().view<TerrainComponent, TransformComponent>();
        for (auto te : terrain_view) {
            const auto& tc = terrain_view.get<TerrainComponent>(te);
            if (tc.enabled) {
                terrain = &tc;
                terrain_tf = &terrain_view.get<TransformComponent>(te);
                break;
            }
        }
    }

    auto tree_view = world.registry().view<TreeComponent, TransformComponent>();
    for (auto entity : tree_view) {
        auto& tree = tree_view.get<TreeComponent>(entity);
        if (!tree.enabled) continue;

        const auto& tf = tree_view.get<TransformComponent>(entity);
        uint32_t eid = static_cast<uint32_t>(entity);
        auto& cache = entity_caches_[eid];

        const float radius = tree.spawn_radius;
        const float cs = tree.chunk_size;

        int cx_min = static_cast<int>(std::floor((camera_pos.x - tf.position.x - radius) / cs));
        int cx_max = static_cast<int>(std::floor((camera_pos.x - tf.position.x + radius) / cs));
        int cz_min = static_cast<int>(std::floor((camera_pos.z - tf.position.z - radius) / cs));
        int cz_max = static_cast<int>(std::floor((camera_pos.z - tf.position.z + radius) / cs));

        std::unordered_map<uint64_t, bool> active_keys;
        active_keys.reserve(static_cast<size_t>((cx_max - cx_min + 1) * (cz_max - cz_min + 1)));

        for (int x = cx_min; x <= cx_max; ++x) {
            for (int z = cz_min; z <= cz_max; ++z) {
                float ccx = (static_cast<float>(x) + 0.5f) * cs + tf.position.x;
                float ccz = (static_cast<float>(z) + 0.5f) * cs + tf.position.z;
                float dx = ccx - camera_pos.x;
                float dz = ccz - camera_pos.z;
                if (dx * dx + dz * dz > radius * radius) continue;

                uint64_t key = ChunkKey(x, z);
                active_keys[key] = true;

                if (cache.chunks.find(key) == cache.chunks.end()) {
                    TreeChunkData& cd = cache.chunks[key];
                    GenerateChunkInstances(tree, terrain, terrain_tf, tf, x, z, cd);
                }
            }
        }

        for (auto it = cache.chunks.begin(); it != cache.chunks.end();) {
            if (active_keys.find(it->first) == active_keys.end()) {
                it = cache.chunks.erase(it);
            } else {
                ++it;
            }
        }

        int total = 0;
        for (const auto& [k, cd] : cache.chunks) {
            total += static_cast<int>(cd.layouts.size());
        }
        tree.cached_instance_count_ = total;
        cache.last_camera_pos = camera_pos;
    }
}

// ============================================================
// 渲染
// ============================================================

void TreeSystem::Render(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                        const glm::vec3& camera_offset, bool depth_only) {
    // Opaque 彩色通道：depth_only=false → MeshRenderer::DrawSharedTemplateInstanced；
    // PreZ 深度预通道：depth_only=true → MeshRenderer::DrawDepthOnlySharedTemplateInstanced。
    RenderInternal(world, cmd_buffer, frame, depth_only, /*shadow_pass=*/false, camera_offset);
}

void TreeSystem::RenderShadow(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                              const glm::vec3& camera_offset) {
    RenderInternal(world, cmd_buffer, frame, /*depth_only=*/true, /*shadow_pass=*/true, camera_offset);
}

void TreeSystem::RenderInternal(World& world, CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame,
                                 bool depth_only, bool shadow_pass, const glm::vec3& camera_offset) {
    if (!rhi_) return;

    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();
    glm::mat4 view_matrix(1.0f);
    glm::mat4 proj_matrix(1.0f);
    glm::vec3 camera_pos(0.0f);
    bool has_camera = false;

    for (auto cam_entity : camera_view) {
        const auto& cam = camera_view.get<Camera3DComponent>(cam_entity);
        if (!cam.enabled) continue;
        const auto& cam_t = camera_view.get<TransformComponent>(cam_entity);
        camera_pos = glm::vec3(cam_t.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 front = cam_t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = cam_t.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        view_matrix = glm::lookAt(camera_pos, camera_pos + front, up);
        proj_matrix = glm::perspective(glm::radians(cam.fov),
                                        cam.aspect_ratio > 0.0f ? cam.aspect_ratio : (16.0f / 9.0f),
                                        cam.near_clip, cam.far_clip);
        has_camera = true;
        break;
    }
    if (!has_camera) return;

    glm::mat4 vp = proj_matrix * view_matrix;
    glm::vec4 frustum_planes[6];
    ExtractFrustumPlanes(vp, frustum_planes);

    // 前向 pass 绘制使用 command buffer 的 view/proj（与 DrawMeshBatch 执行器同源，含投影修正），
    // 而非上方仅用于视锥剔除的本地相机矩阵。
    const glm::mat4 draw_view = frame.view;
    const glm::mat4 draw_proj = frame.projection;
    const glm::vec3 draw_cam_pos = glm::vec3(glm::inverse(draw_view)[3]);

    // 获取方向光参数
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength_val = 0.35f;
    {
        auto light_view = world.registry().view<DirectionalLight3DComponent>();
        for (auto le : light_view) {
            const auto& light = light_view.get<DirectionalLight3DComponent>(le);
            if (light.enabled) {
                light_dir = light.direction;
                light_color = light.color;
                light_intensity = light.intensity;
                ambient_intensity = light.ambient_intensity;
                shadow_strength_val = light.shadow_strength;
                break;
            }
        }
    }

    auto tree_view = world.registry().view<TreeComponent, TransformComponent>();
    for (auto entity : tree_view) {
        const auto& tree = tree_view.get<TreeComponent>(entity);
        if (!tree.enabled || tree.mesh_path.empty()) continue;

        if (!EnsureMeshLoaded(tree.mesh_path)) continue;

        uint32_t eid = static_cast<uint32_t>(entity);
        auto cache_it = entity_caches_.find(eid);
        if (cache_it == entity_caches_.end()) continue;

        auto& mesh_entry = mesh_cache_[tree.mesh_path];
        float max_dist = shadow_pass ? tree.shadow_distance : tree.cull_distance;

        std::vector<glm::mat4> transforms;
        transforms.reserve(static_cast<size_t>(tree.cached_instance_count_));

        for (const auto& [key, chunk] : cache_it->second.chunks) {
            if (!chunk.valid || chunk.layouts.empty()) continue;
            if (!IsAABBInFrustum(frustum_planes,
                                  chunk.aabb_min - camera_offset,
                                  chunk.aabb_max - camera_offset))
                continue;

            for (const auto& inst : chunk.layouts) {
                float dx = inst.position.x - camera_pos.x;
                float dz = inst.position.z - camera_pos.z;
                float dist = std::sqrt(dx * dx + dz * dz);
                if (dist > max_dist) continue;

                glm::mat4 m(1.0f);
                m = glm::translate(m, inst.position - camera_offset);
                m = glm::rotate(m, inst.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                m = glm::scale(m, glm::vec3(inst.scale));
                transforms.push_back(m);
            }
        }

        if (transforms.empty()) continue;

        if (depth_only) {
            // 深度 pass（PreZ / Shadow）：迁移到 MeshRenderer::DrawDepthOnlySharedTemplateInstanced
            //（与彩色前向 pass 同一份共享局部空间模板 + 每实例 model；ForwardInstancedDepth + 植被风，
            // 风场与剔除变换一致 → 阴影/深度不与彩色错位）。
            if (!EnsureTemplateBuilt(mesh_entry)) continue;
            mesh_renderer_.DrawDepthOnlySharedTemplateInstanced(
                cmd_buffer, *rhi_, mesh_entry.tmpl, mesh_entry.index_count, 0u,
                transforms, draw_view, draw_proj, /*foliage=*/true);
            continue;
        }

        // 彩色前向 pass（Opaque）：迁移到 MeshRenderer::DrawSharedTemplateInstanced（共享局部空间
        // 模板 + 每实例 model 矩阵；foliage 顶点风弯曲沿用 device 全局风场）。
        if (!EnsureTemplateBuilt(mesh_entry)) continue;

        dse::render::ShadedMaterial material;
        material.albedo = glm::vec3(1.0f);
        material.metallic = 0.0f;
        material.roughness = 0.85f;
        material.ao = 1.0f;
        material.double_sided = false;
        material.shading_mode = 0;
        material.receive_shadow = true;
        material.shadow_strength = shadow_strength_val;
        material.foliage = true;

        dse::render::DirectionalLight light;
        light.direction = light_dir;
        light.color = light_color;
        light.intensity = light_intensity;
        light.ambient = ambient_intensity;
        light.enabled = true;

        mesh_renderer_.DrawSharedTemplateInstanced(
            cmd_buffer, *rhi_, mesh_entry.tmpl, mesh_entry.index_count, 0u,
            transforms, draw_view, draw_proj, draw_cam_pos, material, light);
    }
}

} // namespace gameplay3d
} // namespace dse

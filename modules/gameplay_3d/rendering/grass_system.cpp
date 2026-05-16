#include "modules/gameplay_3d/rendering/grass_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace dse {
namespace gameplay3d {

// ============================================================
// Halton 低差异序列 + hash
// ============================================================

static float Halton(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    int i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

static uint32_t HashCombine(uint32_t seed, int v) {
    uint32_t h = static_cast<uint32_t>(v);
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = ((h >> 16) ^ h) * 0x45d9f3b;
    h = (h >> 16) ^ h;
    return seed ^ (h + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

// ============================================================
// 风场：从静态布局 + 风参数 → 最终 model matrix
// ============================================================

/// 将 GrassInstanceLayout 转换为含风场旋转的 model matrix
/// 风场原理: 模型空间 Y=0 在草叶基部，绕基部旋转使尖端位移最大
static glm::mat4 BuildWindMatrix(const GrassInstanceLayout& layout,
                                  const glm::vec2& wind_dir,
                                  float wind_speed,
                                  float wind_strength,
                                  float wind_turbulence,
                                  float time) {
    // 风相位 = 基于位置的空间频率 + 时间
    float phase = layout.wind_phase + time * wind_speed;
    float bend = std::sin(phase) * wind_strength;
    // 湍流：高频扰动
    float turb = std::sin(phase * 3.7f + layout.wind_phase * 2.3f) * wind_turbulence;
    float total_bend = bend + turb;
    // 限制最大弯曲角度 ±25°
    total_bend = std::max(-0.436f, std::min(0.436f, total_bend));

    // 风向分解为绕 X 和绕 Z 的旋转
    // wind_dir = (wx, wz)，绕 Z 轴旋转使草叶沿 X 弯曲，绕 X 轴使草叶沿 Z 弯曲
    float rx = -total_bend * wind_dir.y;  // 绕 X 轴旋转
    float rz = total_bend * wind_dir.x;   // 绕 Z 轴旋转

    float cx = std::cos(rx), sx = std::sin(rx);
    float cz = std::cos(rz), sz = std::sin(rz);
    float cy = std::cos(layout.yaw), sy = std::sin(layout.yaw);

    float w = layout.width;
    float h = layout.height;

    // 构建: translate(pos) * rotateX(rx) * rotateZ(rz) * rotateY(yaw) * scale(w, h, w)
    // 手动展开避免 glm 矩阵乘法开销
    // R_combined = Rx * Rz * Ry
    // Rx = [[1,0,0],[0,cx,-sx],[0,sx,cx]]
    // Rz = [[cz,-sz,0],[sz,cz,0],[0,0,1]]
    // Ry = [[cy,0,sy],[0,1,0],[-sy,0,cy]]
    // R = Rx * Rz * Ry, then scale columns by (w, h, w)

    // Rz * Ry
    float a00 = cz * cy;   float a01 = -sz;  float a02 = cz * sy;
    float a10 = sz * cy;   float a11 = cz;   float a12 = sz * sy;
    float a20 = -sy;       float a21 = 0.0f; float a22 = cy;

    // Rx * (Rz * Ry)
    float r00 = a00;                     float r01 = a01;                    float r02 = a02;
    float r10 = cx * a10 - sx * a20;     float r11 = cx * a11 - sx * a21;   float r12 = cx * a12 - sx * a22;
    float r20 = sx * a10 + cx * a20;     float r21 = sx * a11 + cx * a21;   float r22 = sx * a12 + cx * a22;

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(r00 * w, r10 * w, r20 * w, 0.0f);
    m[1] = glm::vec4(r01 * h, r11 * h, r21 * h, 0.0f);
    m[2] = glm::vec4(r02 * w, r12 * w, r22 * w, 0.0f);
    m[3] = glm::vec4(layout.position, 1.0f);
    return m;
}

// ============================================================
// 生命周期
// ============================================================

void GrassSystem::Init(RhiDevice* rhi_device) {
    rhi_ = rhi_device;
    BuildBladeMesh();
    BuildBillboardMesh();
    DEBUG_LOG_INFO("[GrassSystem] Initialized. blade_verts={}, billboard_verts={}",
                   blade_vertices_.size(), billboard_vertices_.size());
}

void GrassSystem::Shutdown(World& world) {
    (void)world;
    blade_vertices_.clear();
    blade_indices_.clear();
    billboard_vertices_.clear();
    billboard_indices_.clear();
    entity_caches_.clear();
    rhi_ = nullptr;
}

// ============================================================
// 程序化 Mesh 生成
// ============================================================

void GrassSystem::BuildBladeMesh() {
    // 4 段三角带: 5 层 × 2 列 = 10 顶点, 8 三角形
    // 模型空间: X ∈ [-0.5, 0.5], Y ∈ [0, 1], Z = 0
    const int segments = 4;
    const int vert_count = (segments + 1) * 2;
    blade_vertices_.resize(static_cast<size_t>(vert_count));

    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float half_width = 0.5f * (1.0f - t * 0.8f);

        BatchVertex& vl = blade_vertices_[static_cast<size_t>(i * 2)];
        vl.pos = glm::vec3(-half_width, t, 0.0f);
        vl.color = glm::vec4(1.0f);
        vl.uv = glm::vec2(0.0f, t);
        vl.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        vl.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vl.weights = glm::vec4(0.0f);
        vl.joints = glm::vec4(0.0f);

        BatchVertex& vr = blade_vertices_[static_cast<size_t>(i * 2 + 1)];
        vr.pos = glm::vec3(half_width, t, 0.0f);
        vr.color = glm::vec4(1.0f);
        vr.uv = glm::vec2(1.0f, t);
        vr.normal = glm::vec3(0.0f, 0.0f, 1.0f);
        vr.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        vr.weights = glm::vec4(0.0f);
        vr.joints = glm::vec4(0.0f);
    }

    blade_indices_.reserve(static_cast<size_t>(segments * 6));
    for (int i = 0; i < segments; ++i) {
        unsigned short bl = static_cast<unsigned short>(i * 2);
        unsigned short br = bl + 1;
        unsigned short tl = bl + 2;
        unsigned short tr = bl + 3;
        blade_indices_.push_back(bl); blade_indices_.push_back(br); blade_indices_.push_back(tl);
        blade_indices_.push_back(br); blade_indices_.push_back(tr); blade_indices_.push_back(tl);
    }
}

void GrassSystem::BuildBillboardMesh() {
    // 交叉十字形: 2 个正交 quad, 8 顶点, 4 三角形
    billboard_vertices_.resize(8);

    auto make_quad = [&](int base, const glm::vec3& right) {
        glm::vec3 half_r = right * 0.5f;
        glm::vec3 n = glm::normalize(glm::cross(right, glm::vec3(0.0f, 1.0f, 0.0f)));

        billboard_vertices_[static_cast<size_t>(base + 0)].pos = -half_r;
        billboard_vertices_[static_cast<size_t>(base + 0)].uv = glm::vec2(0.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 1)].pos = half_r;
        billboard_vertices_[static_cast<size_t>(base + 1)].uv = glm::vec2(1.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 2)].pos = half_r + glm::vec3(0.0f, 1.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 2)].uv = glm::vec2(1.0f, 1.0f);
        billboard_vertices_[static_cast<size_t>(base + 3)].pos = -half_r + glm::vec3(0.0f, 1.0f, 0.0f);
        billboard_vertices_[static_cast<size_t>(base + 3)].uv = glm::vec2(0.0f, 1.0f);

        for (int j = 0; j < 4; ++j) {
            auto& v = billboard_vertices_[static_cast<size_t>(base + j)];
            v.color = glm::vec4(1.0f);
            v.normal = n;
            v.tangent = glm::normalize(right);
            v.weights = glm::vec4(0.0f);
            v.joints = glm::vec4(0.0f);
        }
    };

    make_quad(0, glm::vec3(1.0f, 0.0f, 0.0f));
    make_quad(4, glm::vec3(0.0f, 0.0f, 1.0f));

    billboard_indices_ = {
        0, 1, 2,  0, 2, 3,
        4, 5, 6,  4, 6, 7
    };
}

// ============================================================
// Chunk 空间辅助
// ============================================================

uint64_t GrassSystem::ChunkKey(int cx, int cz) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cz));
}

void GrassSystem::ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out_planes[6]) {
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

bool GrassSystem::IsAABBInFrustum(const glm::vec4 planes[6],
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
// Chunk 实例生成（静态布局，不含风场）
// ============================================================

void GrassSystem::GenerateChunkInstances(const GrassComponent& grass,
                                          const TerrainComponent* terrain,
                                          const TransformComponent* terrain_transform,
                                          const TransformComponent& grass_transform,
                                          int chunk_x, int chunk_z,
                                          GrassChunkData& out) {
    (void)grass_transform;
    out.layouts.clear();
    out.valid = true;

    const float cs = grass.chunk_size;
    const float world_min_x = static_cast<float>(chunk_x) * cs;
    const float world_min_z = static_cast<float>(chunk_z) * cs;

    const float chunk_area = cs * cs;
    const int blade_count = static_cast<int>(grass.density * chunk_area);
    if (blade_count <= 0) { out.valid = false; return; }

    uint32_t chunk_seed = HashCombine(grass.seed, chunk_x);
    chunk_seed = HashCombine(chunk_seed, chunk_z);

    out.aabb_min = glm::vec3(world_min_x, 0.0f, world_min_z);
    out.aabb_max = glm::vec3(world_min_x + cs, grass.blade_height * 1.5f, world_min_z + cs);

    float min_y = 1e9f, max_y = -1e9f;

    // 风方向归一化提到循环外避免重复计算
    glm::vec2 wind_norm_cached = glm::length(grass.wind_direction) > 1e-6f
                                  ? glm::normalize(grass.wind_direction)
                                  : glm::vec2(1.0f, 0.0f);

    out.layouts.reserve(static_cast<size_t>(blade_count));

    for (int i = 0; i < blade_count; ++i) {
        int seq = static_cast<int>(chunk_seed) + i + 1;
        float hx = Halton(seq, 2);
        float hz = Halton(seq, 3);
        float wx = world_min_x + hx * cs;
        float wz = world_min_z + hz * cs;

        float wy = 0.0f;
        if (terrain && terrain_transform) {
            wy = dse::SampleTerrainHeight(*terrain, *terrain_transform, wx, wz);
        }

        float height_var = Halton(seq, 5);
        float height_scale = 1.0f + (height_var * 2.0f - 1.0f) * grass.blade_height_variation;

        GrassInstanceLayout layout;
        layout.position = glm::vec3(wx, wy, wz);
        layout.yaw = Halton(seq, 7) * 6.283185f;
        layout.width = grass.blade_width;
        layout.height = grass.blade_height * height_scale;
        layout.wind_phase = glm::dot(glm::vec2(wx, wz), wind_norm_cached) * 0.5f;

        out.layouts.push_back(layout);

        min_y = std::min(min_y, wy);
        max_y = std::max(max_y, wy + layout.height);
    }

    if (!out.layouts.empty()) {
        out.aabb_min.y = min_y;
        out.aabb_max.y = max_y;
    }
}

// ============================================================
// Update: 增量 chunk 缓存维护
// ============================================================

void GrassSystem::Update(World& world, float delta_time) {
    accumulated_time_ += static_cast<double>(delta_time);

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
    const TransformComponent* terrain_transform = nullptr;
    {
        auto terrain_view = world.registry().view<TerrainComponent, TransformComponent>();
        for (auto te : terrain_view) {
            const auto& tc = terrain_view.get<TerrainComponent>(te);
            if (tc.enabled) {
                terrain = &tc;
                terrain_transform = &terrain_view.get<TransformComponent>(te);
                break;
            }
        }
    }

    auto grass_view = world.registry().view<GrassComponent, TransformComponent>();
    for (auto entity : grass_view) {
        auto& grass = grass_view.get<GrassComponent>(entity);
        if (!grass.enabled) continue;

        const auto& grass_transform = grass_view.get<TransformComponent>(entity);
        uint32_t eid = static_cast<uint32_t>(entity);
        auto& cache = entity_caches_[eid];

        const float radius = grass.spawn_radius;
        const float cs = grass.chunk_size;

        int cx_min = static_cast<int>(std::floor((camera_pos.x - radius) / cs));
        int cx_max = static_cast<int>(std::floor((camera_pos.x + radius) / cs));
        int cz_min = static_cast<int>(std::floor((camera_pos.z - radius) / cs));
        int cz_max = static_cast<int>(std::floor((camera_pos.z + radius) / cs));

        std::unordered_map<uint64_t, bool> active_keys;
        active_keys.reserve(static_cast<size_t>((cx_max - cx_min + 1) * (cz_max - cz_min + 1)));

        for (int cx = cx_min; cx <= cx_max; ++cx) {
            for (int cz = cz_min; cz <= cz_max; ++cz) {
                float chunk_center_x = (static_cast<float>(cx) + 0.5f) * cs;
                float chunk_center_z = (static_cast<float>(cz) + 0.5f) * cs;
                float dx = chunk_center_x - camera_pos.x;
                float dz = chunk_center_z - camera_pos.z;
                if (dx * dx + dz * dz > radius * radius) continue;

                uint64_t key = ChunkKey(cx, cz);
                active_keys[key] = true;

                if (cache.chunks.find(key) == cache.chunks.end()) {
                    GrassChunkData& cd = cache.chunks[key];
                    GenerateChunkInstances(grass, terrain, terrain_transform,
                                           grass_transform, cx, cz, cd);
                }
            }
        }

        for (auto it = cache.chunks.begin(); it != cache.chunks.end(); ) {
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
        grass.cached_instance_count_ = total;
        cache.last_camera_pos = camera_pos;
    }
}

// ============================================================
// 渲染
// ============================================================

void GrassSystem::Render(World& world, CommandBuffer& cmd_buffer) {
    RenderInternal(world, cmd_buffer, false);
}

void GrassSystem::RenderShadow(World& world, CommandBuffer& cmd_buffer) {
    RenderInternal(world, cmd_buffer, true);
}

void GrassSystem::RenderInternal(World& world, CommandBuffer& cmd_buffer,
                                  bool shadow_pass) {
    if (blade_vertices_.empty()) return;

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

    // 方向光
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

    const float current_time = static_cast<float>(std::fmod(accumulated_time_, 10000.0));

    auto grass_view = world.registry().view<GrassComponent, TransformComponent>();
    for (auto entity : grass_view) {
        const auto& grass = grass_view.get<GrassComponent>(entity);
        if (!grass.enabled) continue;

        uint32_t eid = static_cast<uint32_t>(entity);
        auto cache_it = entity_caches_.find(eid);
        if (cache_it == entity_caches_.end()) continue;
        const auto& cache = cache_it->second;

        glm::vec2 wind_norm = glm::length(grass.wind_direction) > 1e-6f
                              ? glm::normalize(grass.wind_direction)
                              : glm::vec2(1.0f, 0.0f);

        std::vector<glm::mat4> lod0_instances;
        std::vector<glm::mat4> lod1_instances;
        lod0_instances.reserve(static_cast<size_t>(grass.cached_instance_count_));

        for (const auto& [key, cd] : cache.chunks) {
            if (!cd.valid || cd.layouts.empty()) continue;

            if (!IsAABBInFrustum(frustum_planes, cd.aabb_min, cd.aabb_max))
                continue;

            float ccx = (cd.aabb_min.x + cd.aabb_max.x) * 0.5f;
            float ccz = (cd.aabb_min.z + cd.aabb_max.z) * 0.5f;
            float dx = ccx - camera_pos.x;
            float dz = ccz - camera_pos.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (shadow_pass) {
                if (!grass.cast_shadow || dist > grass.shadow_distance) continue;
                for (const auto& layout : cd.layouts) {
                    lod0_instances.push_back(BuildWindMatrix(
                        layout, wind_norm, grass.wind_speed,
                        grass.wind_strength, grass.wind_turbulence, current_time));
                }
            } else {
                if (dist < grass.lod_near) {
                    for (const auto& layout : cd.layouts) {
                        lod0_instances.push_back(BuildWindMatrix(
                            layout, wind_norm, grass.wind_speed,
                            grass.wind_strength, grass.wind_turbulence, current_time));
                    }
                } else if (dist < grass.lod_far) {
                    for (const auto& layout : cd.layouts) {
                        lod1_instances.push_back(BuildWindMatrix(
                            layout, wind_norm, grass.wind_speed,
                            grass.wind_strength, grass.wind_turbulence, current_time));
                    }
                }
            }
        }

        auto fill_item = [&](MeshDrawItem& item) {
            item.lighting_enabled = true;
            item.shading_mode = 0;
            item.material_albedo = grass.base_color;
            item.material_metallic = 0.0f;
            item.material_roughness = 0.85f;
            item.material_ao = 1.0f;
            item.material_double_sided = true;
            item.receive_shadow = true;
            item.depth_test_enabled = true;
            item.depth_write_enabled = true;
            item.texture_handle = grass.albedo_texture;
            item.light_direction = light_dir;
            item.light_color = light_color;
            item.light_intensity = light_intensity;
            item.ambient_intensity = ambient_intensity;
            item.shadow_strength = shadow_strength_val;
        };

        // 提交 instanced draw —— 注意 executor 阈值是 size()>1，
        // 单实例时退化为 item.model 走非 instanced 路径
        auto submit_batch = [&](std::vector<glm::mat4>& instances,
                               const std::vector<BatchVertex>& verts,
                               const std::vector<unsigned short>& idxs) {
            if (instances.empty() || verts.empty()) return;
            MeshDrawItem item;
            item.vertices = verts;
            item.indices = idxs;
            if (instances.size() == 1) {
                item.model = instances[0];
            } else {
                item.model = glm::mat4(1.0f);
                item.instance_transforms = std::move(instances);
            }
            fill_item(item);
            std::vector<MeshDrawItem> items = {std::move(item)};
            cmd_buffer.DrawMeshBatch(items);
        };

        submit_batch(lod0_instances, blade_vertices_, blade_indices_);
        if (!shadow_pass) {
            submit_batch(lod1_instances, billboard_vertices_, billboard_indices_);
        }
    }
}

} // namespace gameplay3d
} // namespace dse

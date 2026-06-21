#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_snow.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/debug.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <cstdint>

namespace dse {
namespace gameplay3d {

namespace {
// BatchVertex 模板 → GpuMeshVertex 局部空间常驻 VB（MeshRenderer 前向 pass 共享模板，B2b-6）。
dse::render::BufferHandle BuildTerrainShadedVbo(RhiDevice& rhi,
                                                const std::vector<BatchVertex>& vertices) {
    if (vertices.empty()) return dse::render::BufferHandle{};
    std::vector<dse::render::MeshVertex> verts;
    verts.reserve(vertices.size());
    for (const auto& bv : vertices) {
        dse::render::MeshVertex mv;
        mv.position = bv.pos;
        mv.color = bv.color;
        mv.uv = bv.uv;
        mv.normal = bv.normal;
        mv.tangent = bv.tangent;
        verts.push_back(mv);
    }
    return dse::render::MeshRenderer::BuildShadedLocalVertexBuffer(rhi, verts);
}
}  // namespace

// ============================================================
// 生命周期
// ============================================================

void TerrainSystem::Init(RhiDevice* rhi_device) {
    rhi_ = rhi_device;
}

void TerrainSystem::Shutdown(World& world) {
    if (!rhi_) return;
    ShutdownTiles(world);
    auto view = world.registry().view<TerrainComponent>();
    for (auto entity : view) {
        auto& terrain = view.get<TerrainComponent>(entity);
        DestroyTerrainGPU(terrain);
    }
    mesh_renderer_.Shutdown(*rhi_);
    rhi_ = nullptr;
}

// ============================================================
// GPU 资源管理
// ============================================================

void TerrainSystem::DestroyTerrainGPU(TerrainComponent& terrain) {
    if (terrain.vao && rhi_) {
        rhi_->DeleteStaticMeshVAO(terrain.vao, terrain.vbo, terrain.lod_ebos);
        terrain.vao = {};
        terrain.vbo = {};
        terrain.lod_ebos.clear();
        terrain.lod_index_counts.clear();
        terrain.ebo = {};
        terrain.index_count = 0;
    }
    if (terrain.shaded_vbo && rhi_) {
        rhi_->DeleteGpuBuffer(terrain.shaded_vbo);
        terrain.shaded_vbo = {};
    }
    if (terrain.splat_weight_texture != 0 && rhi_) {
        rhi_->DeleteTexture(terrain.splat_weight_texture);
        terrain.splat_weight_texture = 0;
        terrain.splat_dirty = true;  // 几何重建后需重新上传权重图
    }
}

void TerrainSystem::UploadSplatWeightMap(TerrainComponent& terrain) {
    if (!rhi_) return;
    if (!terrain.splat_dirty) return;

    const int w = terrain.resolution_x;
    const int h = terrain.resolution_z;
    const size_t expected = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;

    // 无数据或尺寸不匹配：释放旧纹理并退出（渲染时回退到预烘焙权重图路径）。
    if (w < 1 || h < 1 || terrain.splat_data.size() < expected) {
        if (terrain.splat_weight_texture != 0) {
            rhi_->DeleteTexture(terrain.splat_weight_texture);
            terrain.splat_weight_texture = 0;
        }
        terrain.splat_dirty = false;
        return;
    }

    // 逐顶点 float 权重(0..1) → RGBA8 权重图。
    std::vector<unsigned char> rgba8(expected);
    for (size_t i = 0; i < expected; ++i) {
        float v = terrain.splat_data[i];
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        rgba8[i] = static_cast<unsigned char>(v * 255.0f + 0.5f);
    }

    if (terrain.splat_weight_texture != 0) {
        rhi_->DeleteTexture(terrain.splat_weight_texture);
        terrain.splat_weight_texture = 0;
    }
    // 权重图：线性采样 + ClampToEdge（避免边缘 wrap 串色）。
    TextureSamplerDesc sampler;
    sampler.filter = TextureFilter::Linear;
    sampler.wrap = TextureWrap::ClampToEdge;
    terrain.splat_weight_texture = rhi_->CreateTexture2D(w, h, rgba8.data(), sampler);
    terrain.splat_dirty = false;
}

// ============================================================
// 网格重建（含中心差分法线 + skirt 裙边 + 多级 LOD EBO）
// ============================================================

void TerrainSystem::RebuildTerrain(TerrainComponent& terrain) {
    if (terrain.resolution_x < 2 || terrain.resolution_z < 2) return;
    if (!rhi_) return;

    DestroyTerrainGPU(terrain);

    const int rx = terrain.resolution_x;
    const int rz = terrain.resolution_z;
    const float dx = terrain.width / static_cast<float>(rx - 1);
    const float dz = terrain.depth / static_cast<float>(rz - 1);
    const float start_x = -terrain.width * 0.5f;
    const float start_z = -terrain.depth * 0.5f;

    if (terrain.height_data.empty()) {
        terrain.height_data.assign(static_cast<size_t>(rx * rz), 0.0f);
    }

    const auto h = [&](int x, int z) -> float {
        x = std::clamp(x, 0, rx - 1);
        z = std::clamp(z, 0, rz - 1);
        return terrain.height_data[static_cast<size_t>(z * rx + x)];
    };

    // --- 1) 生成顶点（主网格 + skirt） ---
    const int main_vert_count = rx * rz;
    const int skirt_vert_count = 2 * (rx + rz);
    const int total_vert_count = main_vert_count + skirt_vert_count;
    std::vector<BatchVertex> vertices(static_cast<size_t>(total_vert_count));

    const float skirt_drop = terrain.max_height * 0.5f + 10.0f;

    for (int z = 0; z < rz; ++z) {
        for (int x = 0; x < rx; ++x) {
            BatchVertex& v = vertices[static_cast<size_t>(z * rx + x)];
            v.pos = glm::vec3(start_x + x * dx, h(x, z), start_z + z * dz);
            v.color = glm::vec4(1.0f);
            v.uv = glm::vec2(
                static_cast<float>(x) / static_cast<float>(rx - 1),
                static_cast<float>(z) / static_cast<float>(rz - 1));

            // 中心差分法线
            float hL = h(x - 1, z), hR = h(x + 1, z);
            float hD = h(x, z - 1), hU = h(x, z + 1);
            glm::vec3 n(-((hR - hL) / (2.0f * dx)), 1.0f, -((hU - hD) / (2.0f * dz)));
            v.normal = glm::normalize(n);

            // 切线（沿 X 方向的 height gradient）
            glm::vec3 t(1.0f, (hR - hL) / (2.0f * dx), 0.0f);
            v.tangent = glm::normalize(t);

            v.weights = glm::vec4(0.0f);
            v.joints = glm::vec4(0.0f);
        }
    }

    // Skirt 裙边顶点：bottom edge, top edge, left edge, right edge
    int skirt_idx = main_vert_count;
    auto add_skirt_vert = [&](int gx, int gz) {
        BatchVertex& sv = vertices[static_cast<size_t>(skirt_idx++)];
        const BatchVertex& src = vertices[static_cast<size_t>(gz * rx + gx)];
        sv = src;
        sv.pos.y -= skirt_drop;
    };
    // bottom edge (z=0)
    for (int x = 0; x < rx; ++x) add_skirt_vert(x, 0);
    // top edge (z=rz-1)
    for (int x = 0; x < rx; ++x) add_skirt_vert(x, rz - 1);
    // left edge (x=0)
    for (int z = 0; z < rz; ++z) add_skirt_vert(0, z);
    // right edge (x=rx-1)
    for (int z = 0; z < rz; ++z) add_skirt_vert(rx - 1, z);

    // --- 2) 生成多级 LOD 索引 ---
    terrain.max_lod_levels = std::max(1, terrain.max_lod_levels);

    std::vector<std::vector<unsigned int>> lod_indices(static_cast<size_t>(terrain.max_lod_levels));

    // 安全上限：step = 2^lod 超出网格分辨率后无意义，且 lod>=31 会触发 int 溢出
    const int max_useful_lod = std::min(terrain.max_lod_levels,
                                         static_cast<int>(std::ceil(std::log2(static_cast<double>(std::max(rx, rz))))));
    const int actual_lod_count = std::max(1, max_useful_lod);
    lod_indices.resize(static_cast<size_t>(actual_lod_count));
    terrain.max_lod_levels = actual_lod_count;

    for (int lod = 0; lod < actual_lod_count; ++lod) {
        const int step = 1 << lod;
        auto& idx = lod_indices[static_cast<size_t>(lod)];
        idx.reserve(static_cast<size_t>((rx / step) * (rz / step) * 6));

        // 主网格三角形
        for (int z = 0; z + step < rz; z += step) {
            for (int x = 0; x + step < rx; x += step) {
                unsigned int tl = static_cast<unsigned int>(z * rx + x);
                unsigned int tr = static_cast<unsigned int>(z * rx + (x + step));
                unsigned int bl = static_cast<unsigned int>((z + step) * rx + x);
                unsigned int br = static_cast<unsigned int>((z + step) * rx + (x + step));
                idx.push_back(tl); idx.push_back(bl); idx.push_back(tr);
                idx.push_back(tr); idx.push_back(bl); idx.push_back(br);
            }
        }

        // Skirt 三角形（只在 LOD 0 添加，高 LOD 裙边不太可见）
        if (lod == 0) {
            const unsigned int sb = static_cast<unsigned int>(main_vert_count);          // bottom edge skirt start
            const unsigned int st = static_cast<unsigned int>(main_vert_count + rx);     // top edge skirt start
            const unsigned int sl = static_cast<unsigned int>(main_vert_count + 2 * rx); // left edge skirt start
            const unsigned int sr = static_cast<unsigned int>(main_vert_count + 2 * rx + rz); // right edge skirt start

            // bottom edge
            for (int x = 0; x + 1 < rx; ++x) {
                unsigned int a = static_cast<unsigned int>(x);
                unsigned int b = static_cast<unsigned int>(x + 1);
                idx.push_back(a); idx.push_back(sb + static_cast<unsigned int>(x)); idx.push_back(b);
                idx.push_back(b); idx.push_back(sb + static_cast<unsigned int>(x)); idx.push_back(sb + static_cast<unsigned int>(x + 1));
            }
            // top edge
            for (int x = 0; x + 1 < rx; ++x) {
                unsigned int a = static_cast<unsigned int>((rz - 1) * rx + x);
                unsigned int b = static_cast<unsigned int>((rz - 1) * rx + x + 1);
                idx.push_back(a); idx.push_back(b); idx.push_back(st + static_cast<unsigned int>(x));
                idx.push_back(b); idx.push_back(st + static_cast<unsigned int>(x + 1)); idx.push_back(st + static_cast<unsigned int>(x));
            }
            // left edge
            for (int z = 0; z + 1 < rz; ++z) {
                unsigned int a = static_cast<unsigned int>(z * rx);
                unsigned int b = static_cast<unsigned int>((z + 1) * rx);
                idx.push_back(a); idx.push_back(sl + static_cast<unsigned int>(z)); idx.push_back(b);
                idx.push_back(b); idx.push_back(sl + static_cast<unsigned int>(z)); idx.push_back(sl + static_cast<unsigned int>(z + 1));
            }
            // right edge
            for (int z = 0; z + 1 < rz; ++z) {
                unsigned int a = static_cast<unsigned int>(z * rx + rx - 1);
                unsigned int b = static_cast<unsigned int>((z + 1) * rx + rx - 1);
                idx.push_back(a); idx.push_back(b); idx.push_back(sr + static_cast<unsigned int>(z));
                idx.push_back(b); idx.push_back(sr + static_cast<unsigned int>(z + 1)); idx.push_back(sr + static_cast<unsigned int>(z));
            }
        }
    }

    // --- 3) 上传 GPU ---
    std::vector<const void*> ebo_datas;
    std::vector<size_t> ebo_sizes;
    terrain.lod_index_counts.resize(static_cast<size_t>(terrain.max_lod_levels));

    for (int lod = 0; lod < terrain.max_lod_levels; ++lod) {
        const auto& idx = lod_indices[static_cast<size_t>(lod)];
        ebo_datas.push_back(idx.data());
        ebo_sizes.push_back(idx.size() * sizeof(unsigned int));
        terrain.lod_index_counts[static_cast<size_t>(lod)] = static_cast<unsigned int>(idx.size());
    }

    terrain.vao = rhi_->CreateStaticMeshVAO(
        vertices.data(), vertices.size() * sizeof(BatchVertex),
        ebo_datas, ebo_sizes,
        terrain.vbo, terrain.lod_ebos);

    if (terrain.vao) {
        terrain.ebo = terrain.lod_ebos.empty() ? dse::render::BufferHandle{} : terrain.lod_ebos[0];
        terrain.index_count = terrain.lod_index_counts.empty() ? 0 : terrain.lod_index_counts[0];
    }

    // MeshRenderer 前向 pass 的共享局部空间模板 VB（复用 lod_ebos 作索引）。
    terrain.shaded_vbo = BuildTerrainShadedVbo(*rhi_, vertices);

    terrain.is_dirty = false;
    DEBUG_LOG_INFO("[TerrainSystem] Rebuilt terrain '{}x{}' \u2192 {} verts, {} LODs, vao={}",
                  rx, rz, total_vert_count, terrain.max_lod_levels, terrain.vao.raw());
}

// ============================================================
// 渲染
// ============================================================

void TerrainSystem::Render(World& world, CommandBuffer& cmd_buffer, const glm::vec3& camera_offset,
                           bool depth_only) {
    // Tiled terrain lifecycle update + render
    UpdateTiles(world);
    RenderTiles(world, cmd_buffer, camera_offset, depth_only);

    // Single-patch terrain (original path)
    auto view = world.registry().view<TerrainComponent, TransformComponent>();
    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();

    glm::vec3 camera_pos(0.0f);
    if (camera_view.begin() != camera_view.end()) {
        auto cam_entity = *camera_view.begin();
        camera_pos = glm::vec3(camera_view.get<TransformComponent>(cam_entity).local_to_world
                               * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // 前向 pass 绘制矩阵（与 DrawMeshBatch 执行器同源，含投影修正）。
    const glm::mat4 draw_view = cmd_buffer.GetViewMatrix();
    const glm::mat4 draw_proj = cmd_buffer.GetProjectionMatrix();
    const glm::vec3 draw_cam_pos = glm::vec3(glm::inverse(draw_view)[3]);

    // 方向光（深度/前向 pass 共用）。
    glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
    glm::vec3 light_color(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength_val = 0.35f;
    bool has_light = false;
    {
        auto light_view = world.registry().view<DirectionalLight3DComponent>();
        if (light_view.begin() != light_view.end()) {
            auto& light = light_view.get<DirectionalLight3DComponent>(*light_view.begin());
            if (light.enabled) {
                light_dir = light.direction;
                light_color = light.color;
                light_intensity = light.intensity;
                ambient_intensity = light.ambient_intensity;
                shadow_strength_val = light.shadow_strength;
                has_light = true;
            }
        }
    }

    for (auto entity : view) {
        auto& terrain = view.get<TerrainComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!terrain.enabled) continue;

        if (terrain.is_dirty) {
            RebuildTerrain(terrain);
            if (!world.registry().all_of<BoundingBoxComponent>(entity)) {
                world.registry().emplace<BoundingBoxComponent>(entity);
            }
            auto& bbox = world.registry().get<BoundingBoxComponent>(entity);
            bbox.min_extents = glm::vec3(-terrain.width * 0.5f, 0.0f, -terrain.depth * 0.5f);
            bbox.max_extents = glm::vec3(terrain.width * 0.5f, terrain.max_height, terrain.depth * 0.5f);
        }

        if (!terrain.visible) continue;
        if (!terrain.vao) continue;

        // LOD 选择
        if (terrain.use_dynamic_lod) {
            glm::vec3 terrain_center = glm::vec3(transform.local_to_world * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            float dist = glm::length(camera_pos - terrain_center);
            int desired_lod = static_cast<int>(dist / terrain.lod_distance_factor);
            terrain.current_lod = std::clamp(desired_lod, 0, terrain.max_lod_levels - 1);
        } else {
            terrain.current_lod = 0;
        }

        int lod = terrain.current_lod;
        if (lod < 0 || static_cast<size_t>(lod) >= terrain.lod_ebos.size()) lod = 0;

        glm::mat4 model = transform.local_to_world;
        model[3] -= glm::vec4(camera_offset, 0.0f);

        // Splatmap（脏时把 splat_data 上传为权重图，内部按 dirty 早退）。
        UploadSplatWeightMap(terrain);
        bool has_any_splat = false;
        for (int si = 0; si < 4; ++si) {
            if (terrain.splat_texture_handles[si] != 0) { has_any_splat = true; break; }
        }
        // 优先使用 splat_data 上传的权重图；无则回退到预烘焙的 terrain.texture_handle。
        const unsigned int splat_weight_handle = terrain.splat_weight_texture != 0
            ? terrain.splat_weight_texture : terrain.texture_handle;

        // Snow cover
        auto* snow = world.registry().try_get<SnowCoverComponent>(entity);
        const bool has_snow = snow && snow->enabled && snow->coverage > 0.001f;

        if (depth_only) {
            // 深度 pass（PreZ / Shadow）：迁移到 MeshRenderer::DrawDepthOnlySharedTemplateInstanced
            //（与彩色前向 pass 同一份局部空间模板 VB + LOD EBO + 单实例 model；ForwardInstancedDepth，
            // 仅写深度，splat/snow/纹理对深度无影响故不喂入）。
            if (!terrain.shaded_vbo) continue;
            dse::render::ExternalShadedMesh mesh;
            mesh.vertex_buffer = terrain.shaded_vbo;
            mesh.index_buffer = terrain.lod_ebos[static_cast<size_t>(lod)];
            mesh.index_type = IndexType::UInt32;
            std::vector<glm::mat4> models = { model };
            mesh_renderer_.DrawDepthOnlySharedTemplateInstanced(
                cmd_buffer, *rhi_, mesh, terrain.lod_index_counts[static_cast<size_t>(lod)], 0u,
                models, draw_view, draw_proj, /*foliage=*/false);
            continue;
        }

        // 彩色前向 pass（Opaque）：迁移到 MeshRenderer::DrawSharedTemplateInstanced（单实例；复用 LOD
        // EBO 作索引；splat/snow 经 ShadedMaterial 喂入共享 forward_shaded.frag）。
        if (!terrain.shaded_vbo) continue;

        dse::render::ShadedMaterial material;
        material.albedo = glm::vec3(0.5f, 0.7f, 0.3f);
        material.metallic = 0.0f;
        material.roughness = 0.9f;
        material.ao = 1.0f;
        material.double_sided = false;
        material.shading_mode = 0;
        material.receive_shadow = true;
        material.shadow_strength = shadow_strength_val;
        if (has_any_splat) {
            material.splat_enabled = true;
            material.splat_weight_map = splat_weight_handle;
            for (int si = 0; si < 4; ++si) {
                material.splat_layers[si] = terrain.splat_texture_handles[si];
            }
            material.splat_tiling = terrain.splat_tiling;
        } else {
            material.albedo_tex = terrain.texture_handle;
        }
        if (has_snow) {
            material.snow_coverage = snow->coverage;
            material.snow_albedo = snow->snow_albedo;
            material.snow_roughness = snow->snow_roughness;
            material.snow_normal_threshold = snow->normal_threshold;
            material.snow_edge_sharpness = snow->edge_sharpness;
        }

        dse::render::DirectionalLight light;
        light.direction = light_dir;
        light.color = light_color;
        light.intensity = light_intensity;
        light.ambient = ambient_intensity;
        light.enabled = has_light;

        dse::render::ExternalShadedMesh mesh;
        mesh.vertex_buffer = terrain.shaded_vbo;
        mesh.index_buffer = terrain.lod_ebos[static_cast<size_t>(lod)];
        mesh.index_type = IndexType::UInt32;

        std::vector<glm::mat4> models = { model };
        mesh_renderer_.DrawSharedTemplateInstanced(
            cmd_buffer, *rhi_, mesh, terrain.lod_index_counts[static_cast<size_t>(lod)], 0u,
            models, draw_view, draw_proj, draw_cam_pos, material, light);
    }
}

// ============================================================
// CPU 高度查询（双线性插值）
// ============================================================

float TerrainSystem::SampleHeight(const TerrainComponent& terrain,
                                   const TransformComponent& transform,
                                   float world_x, float world_z) {
    return dse::SampleTerrainHeight(terrain, transform, world_x, world_z);
}

// ============================================================
// Tiled Terrain — 生命周期管理
// ============================================================

void TerrainSystem::ShutdownTiles(World& world) {
    auto tile_view = world.registry().view<TerrainTileManagerComponent>();
    for (auto entity : tile_view) {
        auto& mgr = tile_view.get<TerrainTileManagerComponent>(entity);
        for (auto& [key, tile] : mgr.tiles) {
            DestroyTileMeshGPU(tile);
        }
        mgr.tiles.clear();
        mgr.loaded_tile_count = 0;
        mgr.visible_tile_count = 0;
    }
}

void TerrainSystem::DestroyTileMeshGPU(TerrainTileData& tile) {
    if (tile.vao && rhi_) {
        rhi_->DeleteStaticMeshVAO(tile.vao, tile.vbo, tile.lod_ebos);
        tile.vao = {};
        tile.vbo = {};
        tile.lod_ebos.clear();
        tile.lod_index_counts.clear();
        tile.index_count = 0;
    }
    if (tile.shaded_vbo && rhi_) {
        rhi_->DeleteGpuBuffer(tile.shaded_vbo);
        tile.shaded_vbo = {};
    }
}

// ============================================================
// Tiled Terrain — 程序化高度生成
// ============================================================

void TerrainSystem::GenerateProceduralTile(TerrainTileData& tile,
                                            const TerrainTileManagerComponent& mgr,
                                            int tx, int tz) {
    const int res = mgr.tile_resolution;
    tile.height_data.assign(static_cast<size_t>(res * res), mgr.procedural_base_height);
    tile.tile_x = tx;
    tile.tile_z = tz;
    tile.loaded = true;
    tile.gpu_dirty = true;
}

// ============================================================
// Tiled Terrain — 单 tile 网格构建（复用主地形顶点/索引布局）
// ============================================================

void TerrainSystem::BuildTileMesh(TerrainTileData& tile,
                                   const TerrainTileManagerComponent& mgr,
                                   int tile_x, int tile_z) {
    const int rx = mgr.tile_resolution;
    const int rz = mgr.tile_resolution;
    if (rx < 2 || rz < 2) return;
    if (!rhi_) return;

    DestroyTileMeshGPU(tile);

    const float tile_size = mgr.tile_world_size;
    const float dx = tile_size / static_cast<float>(rx - 1);
    const float dz = tile_size / static_cast<float>(rz - 1);

    // Tile 世界坐标原点为 tile 的左下角
    const float origin_x = static_cast<float>(tile_x) * tile_size;
    const float origin_z = static_cast<float>(tile_z) * tile_size;

    if (tile.height_data.empty()) {
        tile.height_data.assign(static_cast<size_t>(rx * rz), 0.0f);
    }

    const auto h = [&](int x, int z) -> float {
        x = std::clamp(x, 0, rx - 1);
        z = std::clamp(z, 0, rz - 1);
        return tile.height_data[static_cast<size_t>(z * rx + x)];
    };

    // --- 1) 顶点（主网格 + skirt） ---
    const int main_vert_count = rx * rz;
    const int skirt_vert_count = 2 * (rx + rz);
    const int total_vert_count = main_vert_count + skirt_vert_count;
    std::vector<BatchVertex> vertices(static_cast<size_t>(total_vert_count));

    const float skirt_drop = mgr.max_height * 0.5f + 10.0f;

    for (int z = 0; z < rz; ++z) {
        for (int x = 0; x < rx; ++x) {
            BatchVertex& v = vertices[static_cast<size_t>(z * rx + x)];
            v.pos = glm::vec3(origin_x + x * dx, h(x, z), origin_z + z * dz);
            v.color = glm::vec4(1.0f);
            v.uv = glm::vec2(
                static_cast<float>(x) / static_cast<float>(rx - 1),
                static_cast<float>(z) / static_cast<float>(rz - 1));

            float hL = h(x - 1, z), hR = h(x + 1, z);
            float hD = h(x, z - 1), hU = h(x, z + 1);
            glm::vec3 n(-((hR - hL) / (2.0f * dx)), 1.0f, -((hU - hD) / (2.0f * dz)));
            v.normal = glm::normalize(n);

            glm::vec3 t(1.0f, (hR - hL) / (2.0f * dx), 0.0f);
            v.tangent = glm::normalize(t);

            v.weights = glm::vec4(0.0f);
            v.joints = glm::vec4(0.0f);
        }
    }

    // Skirt 裙边
    int skirt_idx = main_vert_count;
    auto add_skirt_vert = [&](int gx, int gz) {
        BatchVertex& sv = vertices[static_cast<size_t>(skirt_idx++)];
        const BatchVertex& src = vertices[static_cast<size_t>(gz * rx + gx)];
        sv = src;
        sv.pos.y -= skirt_drop;
    };
    for (int x = 0; x < rx; ++x) add_skirt_vert(x, 0);
    for (int x = 0; x < rx; ++x) add_skirt_vert(x, rz - 1);
    for (int z = 0; z < rz; ++z) add_skirt_vert(0, z);
    for (int z = 0; z < rz; ++z) add_skirt_vert(rx - 1, z);

    // --- 2) 多级 LOD 索引 ---
    int actual_lod_count = std::max(1,
        std::min(mgr.max_lod_levels,
                 static_cast<int>(std::ceil(std::log2(static_cast<double>(std::max(rx, rz)))))));

    std::vector<std::vector<unsigned int>> lod_indices(static_cast<size_t>(actual_lod_count));

    for (int lod = 0; lod < actual_lod_count; ++lod) {
        const int step = 1 << lod;
        auto& idx = lod_indices[static_cast<size_t>(lod)];
        idx.reserve(static_cast<size_t>((rx / step) * (rz / step) * 6));

        for (int z = 0; z + step < rz; z += step) {
            for (int x = 0; x + step < rx; x += step) {
                unsigned int tl = static_cast<unsigned int>(z * rx + x);
                unsigned int tr = static_cast<unsigned int>(z * rx + (x + step));
                unsigned int bl = static_cast<unsigned int>((z + step) * rx + x);
                unsigned int br = static_cast<unsigned int>((z + step) * rx + (x + step));
                idx.push_back(tl); idx.push_back(bl); idx.push_back(tr);
                idx.push_back(tr); idx.push_back(bl); idx.push_back(br);
            }
        }

        // Skirt 仅在 LOD 0
        if (lod == 0) {
            const unsigned int sb = static_cast<unsigned int>(main_vert_count);
            const unsigned int st = static_cast<unsigned int>(main_vert_count + rx);
            const unsigned int sl = static_cast<unsigned int>(main_vert_count + 2 * rx);
            const unsigned int sr = static_cast<unsigned int>(main_vert_count + 2 * rx + rz);

            for (int x = 0; x + 1 < rx; ++x) {
                unsigned int a = static_cast<unsigned int>(x);
                unsigned int b = static_cast<unsigned int>(x + 1);
                idx.push_back(a); idx.push_back(sb + static_cast<unsigned int>(x)); idx.push_back(b);
                idx.push_back(b); idx.push_back(sb + static_cast<unsigned int>(x)); idx.push_back(sb + static_cast<unsigned int>(x + 1));
            }
            for (int x = 0; x + 1 < rx; ++x) {
                unsigned int a = static_cast<unsigned int>((rz - 1) * rx + x);
                unsigned int b = static_cast<unsigned int>((rz - 1) * rx + x + 1);
                idx.push_back(a); idx.push_back(b); idx.push_back(st + static_cast<unsigned int>(x));
                idx.push_back(b); idx.push_back(st + static_cast<unsigned int>(x + 1)); idx.push_back(st + static_cast<unsigned int>(x));
            }
            for (int z = 0; z + 1 < rz; ++z) {
                unsigned int a = static_cast<unsigned int>(z * rx);
                unsigned int b = static_cast<unsigned int>((z + 1) * rx);
                idx.push_back(a); idx.push_back(sl + static_cast<unsigned int>(z)); idx.push_back(b);
                idx.push_back(b); idx.push_back(sl + static_cast<unsigned int>(z)); idx.push_back(sl + static_cast<unsigned int>(z + 1));
            }
            for (int z = 0; z + 1 < rz; ++z) {
                unsigned int a = static_cast<unsigned int>(z * rx + rx - 1);
                unsigned int b = static_cast<unsigned int>((z + 1) * rx + rx - 1);
                idx.push_back(a); idx.push_back(b); idx.push_back(sr + static_cast<unsigned int>(z));
                idx.push_back(b); idx.push_back(sr + static_cast<unsigned int>(z + 1)); idx.push_back(sr + static_cast<unsigned int>(z));
            }
        }
    }

    // --- 3) 上传 GPU ---
    std::vector<const void*> ebo_datas;
    std::vector<size_t> ebo_sizes;
    tile.lod_index_counts.resize(static_cast<size_t>(actual_lod_count));

    for (int lod = 0; lod < actual_lod_count; ++lod) {
        const auto& idx = lod_indices[static_cast<size_t>(lod)];
        ebo_datas.push_back(idx.data());
        ebo_sizes.push_back(idx.size() * sizeof(unsigned int));
        tile.lod_index_counts[static_cast<size_t>(lod)] = static_cast<unsigned int>(idx.size());
    }

    tile.vao = rhi_->CreateStaticMeshVAO(
        vertices.data(), vertices.size() * sizeof(BatchVertex),
        ebo_datas, ebo_sizes,
        tile.vbo, tile.lod_ebos);

    if (tile.vao) {
        tile.index_count = tile.lod_index_counts.empty() ? 0 : tile.lod_index_counts[0];
    }

    tile.shaded_vbo = BuildTerrainShadedVbo(*rhi_, vertices);

    tile.gpu_dirty = false;
    DEBUG_LOG_INFO("[TerrainSystem] Built tile ({},{}) → {} verts, {} LODs, vao={}",
                   tile_x, tile_z, total_vert_count, actual_lod_count, tile.vao.raw());
}

// ============================================================
// Tiled Terrain — 每帧更新（加载/卸载/LOD）
// ============================================================

void TerrainSystem::UpdateTiles(World& world) {
    if (!rhi_) return;

    auto tile_view = world.registry().view<TerrainTileManagerComponent, TransformComponent>();
    if (tile_view.begin() == tile_view.end()) return;

    // 获取相机位置
    auto camera_view = world.registry().view<Camera3DComponent, TransformComponent>();
    glm::vec3 camera_pos(0.0f);
    if (camera_view.begin() != camera_view.end()) {
        auto cam_entity = *camera_view.begin();
        camera_pos = glm::vec3(camera_view.get<TransformComponent>(cam_entity).local_to_world
                               * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    }

    for (auto entity : tile_view) {
        auto& mgr = tile_view.get<TerrainTileManagerComponent>(entity);
        if (!mgr.enabled) continue;

        const float tile_size = mgr.tile_world_size;
        const float load_r = mgr.load_radius;
        const float unload_r = mgr.unload_radius;

        // 相机所在的 tile 坐标
        int cam_tx = static_cast<int>(std::floor(camera_pos.x / tile_size));
        int cam_tz = static_cast<int>(std::floor(camera_pos.z / tile_size));

        // 需要检查的 tile 范围
        int range = static_cast<int>(std::ceil(load_r / tile_size)) + 1;

        // --- 加载 / 创建范围内的 tiles ---
        for (int tz = cam_tz - range; tz <= cam_tz + range; ++tz) {
            for (int tx = cam_tx - range; tx <= cam_tx + range; ++tx) {
                // tile 中心点
                float cx = (static_cast<float>(tx) + 0.5f) * tile_size;
                float cz = (static_cast<float>(tz) + 0.5f) * tile_size;
                float dist = glm::length(glm::vec2(camera_pos.x - cx, camera_pos.z - cz));

                if (dist > load_r) continue;

                int64_t key = TerrainTileKey(tx, tz);
                auto it = mgr.tiles.find(key);
                if (it == mgr.tiles.end()) {
                    TerrainTileData tile;
                    tile.tile_x = tx;
                    tile.tile_z = tz;

                    if (mgr.use_procedural) {
                        GenerateProceduralTile(tile, mgr, tx, tz);
                    } else {
                        tile.height_data.assign(
                            static_cast<size_t>(mgr.tile_resolution * mgr.tile_resolution), 0.0f);
                        tile.loaded = true;
                        tile.gpu_dirty = true;
                    }

                    mgr.tiles.emplace(key, std::move(tile));
                    it = mgr.tiles.find(key);
                }

                auto& tile = it->second;
                if (tile.gpu_dirty && tile.loaded) {
                    BuildTileMesh(tile, mgr, tx, tz);
                }
            }
        }

        // --- 卸载超出范围的 tiles ---
        for (auto it = mgr.tiles.begin(); it != mgr.tiles.end(); ) {
            auto& tile = it->second;
            float cx = (static_cast<float>(tile.tile_x) + 0.5f) * tile_size;
            float cz = (static_cast<float>(tile.tile_z) + 0.5f) * tile_size;
            float dist = glm::length(glm::vec2(camera_pos.x - cx, camera_pos.z - cz));

            if (dist > unload_r) {
                DestroyTileMeshGPU(tile);
                it = mgr.tiles.erase(it);
            } else {
                ++it;
            }
        }

        // --- LOD 更新 + 统计 ---
        int loaded = 0;
        int visible = 0;
        for (auto& [key, tile] : mgr.tiles) {
            if (!tile.vao) continue;
            ++loaded;

            float cx = (static_cast<float>(tile.tile_x) + 0.5f) * tile_size;
            float cz = (static_cast<float>(tile.tile_z) + 0.5f) * tile_size;
            float dist = glm::length(glm::vec2(camera_pos.x - cx, camera_pos.z - cz));

            int desired_lod = static_cast<int>(dist / mgr.lod_distance_factor);
            int max_lod = static_cast<int>(tile.lod_ebos.size()) - 1;
            tile.current_lod = std::clamp(desired_lod, 0, std::max(0, max_lod));
            ++visible;
        }
        mgr.loaded_tile_count = loaded;
        mgr.visible_tile_count = visible;
    }
}

// ============================================================
// Tiled Terrain — 渲染
// ============================================================

void TerrainSystem::RenderTiles(World& world, CommandBuffer& cmd_buffer, const glm::vec3& camera_offset,
                                bool depth_only) {
    if (!rhi_) return;

    auto tile_view = world.registry().view<TerrainTileManagerComponent, TransformComponent>();
    if (tile_view.begin() == tile_view.end()) return;

    // 光照信息
    glm::vec3 light_dir(-0.4f, -1.0f, -0.3f);
    glm::vec3 light_color(1.0f);
    float light_intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength = 0.35f;
    bool has_light = false;

    auto light_view = world.registry().view<DirectionalLight3DComponent>();
    if (light_view.begin() != light_view.end()) {
        auto& light = light_view.get<DirectionalLight3DComponent>(*light_view.begin());
        if (light.enabled) {
            light_dir = light.direction;
            light_color = light.color;
            light_intensity = light.intensity;
            ambient_intensity = light.ambient_intensity;
            shadow_strength = light.shadow_strength;
            has_light = true;
        }
    }

    // 前向 pass 绘制矩阵 + 方向光结构（与 DrawMeshBatch 执行器同源）。
    const glm::mat4 draw_view = cmd_buffer.GetViewMatrix();
    const glm::mat4 draw_proj = cmd_buffer.GetProjectionMatrix();
    const glm::vec3 draw_cam_pos = glm::vec3(glm::inverse(draw_view)[3]);
    dse::render::DirectionalLight dir_light;
    dir_light.direction = light_dir;
    dir_light.color = light_color;
    dir_light.intensity = light_intensity;
    dir_light.ambient = ambient_intensity;
    dir_light.enabled = has_light;

    for (auto entity : tile_view) {
        auto& mgr = tile_view.get<TerrainTileManagerComponent>(entity);
        auto& transform = tile_view.get<TransformComponent>(entity);

        if (!mgr.enabled) continue;

        // Splatmap 检查
        bool has_any_splat = false;
        for (int si = 0; si < 4; ++si) {
            if (mgr.splat_texture_handles[si] != 0) { has_any_splat = true; break; }
        }

        // Snow cover
        auto* snow = world.registry().try_get<SnowCoverComponent>(entity);
        const bool has_snow = snow && snow->enabled && snow->coverage > 0.001f;

        for (auto& [key, tile] : mgr.tiles) {
            if (!tile.vao) continue;

            int lod = tile.current_lod;
            if (lod < 0 || static_cast<size_t>(lod) >= tile.lod_ebos.size()) lod = 0;

            glm::mat4 model = transform.local_to_world;
            model[3] -= glm::vec4(camera_offset, 0.0f);

            if (depth_only) {
                // 深度 pass（PreZ / Shadow）：迁移到 MeshRenderer::DrawDepthOnlySharedTemplateInstanced
                //（与彩色前向 pass 同一份局部空间模板 VB + LOD EBO + 单实例 model；ForwardInstancedDepth）。
                if (!tile.shaded_vbo) continue;
                dse::render::ExternalShadedMesh dmesh;
                dmesh.vertex_buffer = tile.shaded_vbo;
                dmesh.index_buffer = tile.lod_ebos[static_cast<size_t>(lod)];
                dmesh.index_type = IndexType::UInt32;
                std::vector<glm::mat4> dmodels = { model };
                mesh_renderer_.DrawDepthOnlySharedTemplateInstanced(
                    cmd_buffer, *rhi_, dmesh, tile.lod_index_counts[static_cast<size_t>(lod)], 0u,
                    dmodels, draw_view, draw_proj, /*foliage=*/false);
                continue;
            }

            // 彩色前向 pass（Opaque）：MeshRenderer::DrawSharedTemplateInstanced（单实例，复用 LOD EBO）。
            if (!tile.shaded_vbo) continue;

            dse::render::ShadedMaterial material;
            material.albedo = glm::vec3(0.5f, 0.7f, 0.3f);
            material.metallic = 0.0f;
            material.roughness = 0.9f;
            material.ao = 1.0f;
            material.double_sided = false;
            material.shading_mode = 0;
            material.receive_shadow = true;
            material.shadow_strength = shadow_strength;
            if (has_any_splat) {
                material.splat_enabled = true;
                material.splat_weight_map = mgr.base_texture_handle;
                for (int si = 0; si < 4; ++si) {
                    material.splat_layers[si] = mgr.splat_texture_handles[si];
                }
                material.splat_tiling = mgr.splat_tiling;
            } else {
                material.albedo_tex = mgr.base_texture_handle;
            }
            if (has_snow) {
                material.snow_coverage = snow->coverage;
                material.snow_albedo = snow->snow_albedo;
                material.snow_roughness = snow->snow_roughness;
                material.snow_normal_threshold = snow->normal_threshold;
                material.snow_edge_sharpness = snow->edge_sharpness;
            }

            dse::render::ExternalShadedMesh mesh;
            mesh.vertex_buffer = tile.shaded_vbo;
            mesh.index_buffer = tile.lod_ebos[static_cast<size_t>(lod)];
            mesh.index_type = IndexType::UInt32;

            std::vector<glm::mat4> models = { model };
            mesh_renderer_.DrawSharedTemplateInstanced(
                cmd_buffer, *rhi_, mesh, tile.lod_index_counts[static_cast<size_t>(lod)], 0u,
                models, draw_view, draw_proj, draw_cam_pos, material, dir_light);
        }
    }
}

} // namespace gameplay3d
} // namespace dse
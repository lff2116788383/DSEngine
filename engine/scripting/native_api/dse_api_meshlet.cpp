/**
 * @file dse_api_meshlet.cpp
 * @brief DSEngine C ABI - Meshlet — 使用 MeshletBuilder / MeshletCullPass + 句柄表
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/render/meshlet/meshlet_builder.h"
#include "engine/render/meshlet/meshlet_cull_pass.h"

using namespace dse;
using namespace dse_api_internal;


static std::unordered_map<uint32_t, std::unique_ptr<dse::render::MeshletMesh>> g_meshlet_meshes;
static std::unordered_map<uint32_t, std::unique_ptr<dse::render::MeshletCullPass>> g_meshlet_culls;
static uint32_t g_meshlet_next_mesh = 1;
static uint32_t g_meshlet_next_cull = 1;

extern "C" uint32_t dse_meshlet_build(const float* positions, int pos_count,
                                    const uint32_t* indices, int idx_count,
                                    uint32_t max_vertices, uint32_t max_triangles) {
    if (!positions || pos_count <= 0 || !indices || idx_count <= 0) return 0;
    std::vector<glm::vec3> verts(pos_count / 3);
    for (int i = 0; i < pos_count / 3; ++i)
        verts[i] = glm::vec3(positions[i*3], positions[i*3+1], positions[i*3+2]);
    std::vector<uint32_t> idx(indices, indices + idx_count);

    dse::render::MeshletBuildConfig config;
    config.max_vertices = max_vertices > 0 ? max_vertices : 64;
    config.max_triangles = max_triangles > 0 ? max_triangles : 124;

    dse::render::MeshletBuilder builder;
    auto result = builder.Build(verts, idx, config);

    uint32_t id = g_meshlet_next_mesh++;
    g_meshlet_meshes[id] = std::make_unique<dse::render::MeshletMesh>(std::move(result));
    return id;
}

extern "C" int dse_meshlet_serialize(uint32_t handle, const char* path) {
    auto it = g_meshlet_meshes.find(handle);
    if (it == g_meshlet_meshes.end() || !path) return 0;
    return dse::render::MeshletBuilder::Serialize(*it->second, path) ? 1 : 0;
}

extern "C" uint32_t dse_meshlet_deserialize(const char* path) {
    if (!path) return 0;
    auto mesh = std::make_unique<dse::render::MeshletMesh>();
    if (!dse::render::MeshletBuilder::Deserialize(path, *mesh)) return 0;
    uint32_t id = g_meshlet_next_mesh++;
    g_meshlet_meshes[id] = std::move(mesh);
    return id;
}

extern "C" void dse_meshlet_destroy(uint32_t handle) {
    g_meshlet_meshes.erase(handle);
}

extern "C" void dse_meshlet_get_info(uint32_t handle, int* out_meshlets, int* out_vertices,
                                    int* out_indices, int* out_meshlet_vertices) {
    auto it = g_meshlet_meshes.find(handle);
    if (it == g_meshlet_meshes.end()) {
        if (out_meshlets) *out_meshlets = 0;
        if (out_vertices) *out_vertices = 0;
        if (out_indices) *out_indices = 0;
        if (out_meshlet_vertices) *out_meshlet_vertices = 0;
        return;
    }
    const auto& m = *it->second;
    if (out_meshlets) *out_meshlets = static_cast<int>(m.meshlets.size());
    if (out_vertices) *out_vertices = static_cast<int>(m.positions.size());
    if (out_indices) *out_indices = static_cast<int>(m.global_indices.size());
    if (out_meshlet_vertices) *out_meshlet_vertices = static_cast<int>(m.meshlet_vertices.size());
}

extern "C" uint32_t dse_meshlet_cull_create(void) {
    uint32_t id = g_meshlet_next_cull++;
    g_meshlet_culls[id] = std::make_unique<dse::render::MeshletCullPass>();
    return id;
}

extern "C" void dse_meshlet_cull_destroy(uint32_t cull_handle) {
    g_meshlet_culls.erase(cull_handle);
}

extern "C" uint32_t dse_meshlet_cull_register(uint32_t cull_handle, uint32_t meshlet_handle) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end()) return 0;
    auto mit = g_meshlet_meshes.find(meshlet_handle);
    if (mit == g_meshlet_meshes.end()) return 0;
    return cit->second->RegisterMesh(*mit->second);
}

extern "C" void dse_meshlet_cull_unregister(uint32_t cull_handle, uint32_t reg_handle) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit != g_meshlet_culls.end()) cit->second->UnregisterMesh(reg_handle);
}

extern "C" void dse_meshlet_cull_begin_frame(uint32_t cull_handle) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit != g_meshlet_culls.end()) cit->second->BeginFrame();
}

extern "C" void dse_meshlet_cull_add_instance(uint32_t cull_handle, uint32_t reg_handle,
                                            const float* matrix16) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end() || !matrix16) return;
    glm::mat4 m(1.0f);
    const float* p = matrix16;
    for (int i = 0; i < 16; ++i) m[i / 4][i % 4] = p[i];
    cit->second->AddInstance(reg_handle, m);
}

extern "C" uint32_t dse_meshlet_cull_prepare(uint32_t cull_handle, const float* vp_matrix16,
                                           float cam_x, float cam_y, float cam_z) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end() || !vp_matrix16) return 0;
    glm::mat4 vp(1.0f);
    for (int i = 0; i < 16; ++i) vp[i / 4][i % 4] = vp_matrix16[i];
    return cit->second->PrepareGPUData(glm::mat4(1.0f), vp, glm::vec3(cam_x, cam_y, cam_z));
}

extern "C" uint32_t dse_meshlet_cull_execute_cpu(uint32_t cull_handle, const float* vp_matrix16,
                                                float cam_x, float cam_y, float cam_z,
                                                uint32_t flags) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end() || !vp_matrix16) return 0;
    glm::mat4 vp(1.0f);
    for (int i = 0; i < 16; ++i) vp[i / 4][i % 4] = vp_matrix16[i];
    dse::render::MeshletCullConfig config;
    config.enable_frustum_cull = (flags & 1) != 0;
    config.enable_occlusion_cull = (flags & 2) != 0;
    config.enable_cone_cull = (flags & 4) != 0;
    cit->second->CullCPU(vp, glm::vec3(cam_x, cam_y, cam_z), config);
    return cit->second->GetVisibleMeshletCount();
}

extern "C" void dse_meshlet_cull_stats(uint32_t cull_handle, int* out_total, int* out_visible,
                                      int* out_meshes, int* out_instances) {
    auto cit = g_meshlet_culls.find(cull_handle);
    if (cit == g_meshlet_culls.end()) {
        if (out_total) *out_total = 0;
        if (out_visible) *out_visible = 0;
        if (out_meshes) *out_meshes = 0;
        if (out_instances) *out_instances = 0;
        return;
    }
    if (out_total) *out_total = cit->second->GetTotalMeshletCount();
    if (out_visible) *out_visible = cit->second->GetVisibleMeshletCount();
    if (out_meshes) *out_meshes = cit->second->GetRegisteredMeshCount();
    if (out_instances) *out_instances = cit->second->GetInstanceCount();
}


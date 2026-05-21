#include "editor_navmesh_panel.h"
#include "editor_context.h"
#include "editor_icons.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/navigation/nav_mesh_system.h"
#include "engine/core/service_locator.h"
#include "engine/base/debug.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace dse::editor {

namespace {

// ─── NavMesh data model (editor-side representation) ─────────────────────────

struct NavMeshBakeSettings {
    float cell_size = 0.3f;
    float cell_height = 0.2f;
    float agent_height = 2.0f;
    float agent_radius = 0.6f;
    float agent_max_climb = 0.9f;
    float agent_max_slope = 45.0f;
    float region_min_size = 8.0f;
    float region_merge_size = 20.0f;
    float edge_max_len = 12.0f;
    float edge_max_error = 1.3f;
    int verts_per_poly = 6;
    float detail_sample_dist = 6.0f;
    float detail_sample_max_error = 1.0f;
    bool filter_low_hanging_obstacles = true;
    bool filter_ledge_spans = true;
    bool filter_walkable_low_height = true;
};

struct NavMeshTriangle {
    glm::vec3 v0, v1, v2;
};

struct NavMeshData {
    std::vector<NavMeshTriangle> triangles;
    glm::vec3 bounds_min{0};
    glm::vec3 bounds_max{0};
    bool valid = false;
    int vertex_count = 0;
    int triangle_count = 0;
    int poly_count = 0;
};

struct NavMeshEditorState {
    NavMeshBakeSettings settings;
    NavMeshData baked_data;
    bool show_overlay = true;
    bool show_bounds = true;
    bool baking = false;
    float bake_progress = 0.0f;
    ImVec4 overlay_color = ImVec4(0.0f, 0.7f, 0.4f, 0.3f);
    ImVec4 wireframe_color = ImVec4(0.0f, 1.0f, 0.6f, 0.8f);
};

NavMeshEditorState& GetState() {
    static NavMeshEditorState state;
    return state;
}

/// Build navmesh from scene geometry using Recast/Detour
void SimulateBake(NavMeshEditorState& state, entt::registry& reg) {
    state.baking = false;
    state.bake_progress = 0.0f;

#ifdef DSE_ENABLE_NAVMESH
    auto* nav_sys = dse::core::ServiceLocator::Instance().Get<dse::navigation::NavMeshSystem>();
    if (!nav_sys) {
        DEBUG_LOG_WARN("[NavMesh] NavMeshSystem not available");
        return;
    }

    // Collect all triangle data from scene meshes
    std::vector<float> verts;
    std::vector<int> tris;

    auto view = reg.view<TransformComponent, dse::MeshRendererComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e);
        auto& mesh = view.get<dse::MeshRendererComponent>(e);

        // Skip if mesh has no vertex data
        if (mesh.temp_vertices.empty() || mesh.temp_indices.empty()) continue;

        // Transform vertices to world space
        glm::mat4 model = tf.local_to_world;
        size_t vert_offset = verts.size() / 3;

        for (size_t i = 0; i < mesh.temp_vertices.size(); i += 3) {
            glm::vec4 pos(mesh.temp_vertices[i], mesh.temp_vertices[i+1],
                          mesh.temp_vertices[i+2], 1.0f);
            glm::vec4 world_pos = model * pos;
            verts.push_back(world_pos.x);
            verts.push_back(world_pos.y);
            verts.push_back(world_pos.z);
        }

        // Add triangle indices
        for (size_t i = 0; i < mesh.temp_indices.size(); ++i) {
            tris.push_back(static_cast<int>(vert_offset + mesh.temp_indices[i]));
        }
    }

    if (verts.empty() || tris.empty()) {
        DEBUG_LOG_WARN("[NavMesh] No geometry to bake navmesh from");
        return;
    }

    // Convert editor settings to NavMeshBuildConfig
    dse::navigation::NavMeshBuildConfig cfg;
    cfg.cell_size = state.settings.cell_size;
    cfg.cell_height = state.settings.cell_height;
    cfg.agent_height = state.settings.agent_height;
    cfg.agent_radius = state.settings.agent_radius;
    cfg.agent_max_climb = state.settings.agent_max_climb;
    cfg.agent_max_slope = state.settings.agent_max_slope;
    cfg.region_min_size = state.settings.region_min_size;
    cfg.region_merge_size = state.settings.region_merge_size;
    cfg.edge_max_len = state.settings.edge_max_len;
    cfg.edge_max_error = state.settings.edge_max_error;
    cfg.verts_per_poly = state.settings.verts_per_poly;
    cfg.detail_sample_dist = state.settings.detail_sample_dist;
    cfg.detail_sample_max_error = state.settings.detail_sample_max_error;

    // Bake navmesh
    if (!nav_sys->BakeFromTriangles(verts.data(), static_cast<int>(verts.size() / 3),
                                    tris.data(), static_cast<int>(tris.size() / 3), cfg)) {
        DEBUG_LOG_ERROR("[NavMesh] Bake failed");
        return;
    }

    // Update baked data for visualization
    state.baked_data.valid = true;
    state.baked_data.vertex_count = static_cast<int>(verts.size() / 3);
    state.baked_data.triangle_count = static_cast<int>(tris.size() / 3);
    state.baked_data.poly_count = 0; // TODO: Get poly count from nav mesh

    // Calculate bounds
    if (!verts.empty()) {
        glm::vec3 min(1e6f), max(-1e6f);
        for (size_t i = 0; i < verts.size(); i += 3) {
            min.x = std::min(min.x, verts[i]);
            min.y = std::min(min.y, verts[i+1]);
            min.z = std::min(min.z, verts[i+2]);
            max.x = std::max(max.x, verts[i]);
            max.y = std::max(max.y, verts[i+1]);
            max.z = std::max(max.z, verts[i+2]);
        }
        state.baked_data.bounds_min = min;
        state.baked_data.bounds_max = max;
    }

    DEBUG_LOG_INFO("[NavMesh] Bake completed: {} verts, {} tris",
                  state.baked_data.vertex_count, state.baked_data.triangle_count);
#else
    DEBUG_LOG_WARN("[NavMesh] NavMesh support not enabled (DSE_ENABLE_NAVMESH)");
#endif
}

ImVec2 WorldToScreen(const glm::vec3& wp,
                     const glm::mat4& view_mat,
                     const glm::mat4& proj_mat,
                     const glm::vec2& win_pos,
                     const glm::vec2& panel_size) {
    glm::vec4 clip = proj_mat * view_mat * glm::vec4(wp, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return ImVec2(-10000.0f, -10000.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    return ImVec2(win_pos.x + (ndc.x + 1.0f) * 0.5f * panel_size.x,
                  win_pos.y + (1.0f - ndc.y) * 0.5f * panel_size.y);
}

} // namespace

void DrawNavMeshPanel(EditorContext& ctx) {
    ImGui::Begin("NavMesh");

    auto& state = GetState();
    auto& s = state.settings;

    ImGui::Text(MDI_ICON_MAP_MARKER_PATH " NavMesh Configuration");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Agent", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Height", &s.agent_height, 0.5f, 5.0f, "%.1f");
        ImGui::SliderFloat("Radius", &s.agent_radius, 0.1f, 2.0f, "%.2f");
        ImGui::SliderFloat("Max Climb", &s.agent_max_climb, 0.1f, 2.0f, "%.2f");
        ImGui::SliderFloat("Max Slope", &s.agent_max_slope, 10.0f, 85.0f, "%.0f deg");
    }

    if (ImGui::CollapsingHeader("Rasterization", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Cell Size", &s.cell_size, 0.05f, 2.0f, "%.2f");
        ImGui::SliderFloat("Cell Height", &s.cell_height, 0.05f, 1.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Region")) {
        ImGui::SliderFloat("Min Size", &s.region_min_size, 0.0f, 50.0f, "%.0f");
        ImGui::SliderFloat("Merge Size", &s.region_merge_size, 0.0f, 50.0f, "%.0f");
    }

    if (ImGui::CollapsingHeader("Polygonization")) {
        ImGui::SliderFloat("Edge Max Len", &s.edge_max_len, 0.0f, 50.0f, "%.1f");
        ImGui::SliderFloat("Edge Max Error", &s.edge_max_error, 0.1f, 3.0f, "%.2f");
        ImGui::SliderInt("Verts/Poly", &s.verts_per_poly, 3, 12);
    }

    if (ImGui::CollapsingHeader("Detail")) {
        ImGui::SliderFloat("Sample Dist", &s.detail_sample_dist, 0.0f, 16.0f, "%.1f");
        ImGui::SliderFloat("Sample Error", &s.detail_sample_max_error, 0.0f, 4.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Filters")) {
        ImGui::Checkbox("Low Hanging Obstacles", &s.filter_low_hanging_obstacles);
        ImGui::Checkbox("Ledge Spans", &s.filter_ledge_spans);
        ImGui::Checkbox("Walkable Low Height", &s.filter_walkable_low_height);
    }

    ImGui::Separator();

    // Bake button
    if (ImGui::Button("Bake NavMesh", ImVec2(-1, 30))) {
        SimulateBake(state, ctx.registry);
    }

    // Bake results
    if (state.baked_data.valid) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "NavMesh baked successfully!");
        ImGui::Text("Vertices: %d", state.baked_data.vertex_count);
        ImGui::Text("Triangles: %d", state.baked_data.triangle_count);
        ImGui::Text("Polys: %d", state.baked_data.poly_count);
        ImGui::Text("Bounds: (%.1f,%.1f,%.1f) - (%.1f,%.1f,%.1f)",
                    state.baked_data.bounds_min.x, state.baked_data.bounds_min.y, state.baked_data.bounds_min.z,
                    state.baked_data.bounds_max.x, state.baked_data.bounds_max.y, state.baked_data.bounds_max.z);

        ImGui::Separator();
        ImGui::Checkbox("Show Overlay", &state.show_overlay);
        ImGui::SameLine();
        ImGui::Checkbox("Show Bounds", &state.show_bounds);
        ImGui::ColorEdit4("Overlay Color", (float*)&state.overlay_color, ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::ColorEdit4("Wire Color", (float*)&state.wireframe_color, ImGuiColorEditFlags_NoInputs);

        if (ImGui::Button("Clear NavMesh")) {
            state.baked_data = NavMeshData{};
        }
    } else {
        ImGui::TextDisabled("No NavMesh baked. Click 'Bake NavMesh' to generate.");
    }

    ImGui::End();
}

void DrawNavMeshOverlay(EditorContext& ctx,
                         const glm::vec2& window_pos,
                         const glm::vec2& panel_size,
                         const glm::mat4& view,
                         const glm::mat4& proj) {
    auto& state = GetState();
    if (!state.baked_data.valid || !state.show_overlay) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 fill_col = ImGui::ColorConvertFloat4ToU32(state.overlay_color);
    ImU32 wire_col = ImGui::ColorConvertFloat4ToU32(state.wireframe_color);

    // Limit drawing to avoid performance issues
    int max_tris = std::min(static_cast<int>(state.baked_data.triangles.size()), 2000);

    for (int i = 0; i < max_tris; i++) {
        auto& tri = state.baked_data.triangles[i];
        ImVec2 p0 = WorldToScreen(tri.v0, view, proj, window_pos, panel_size);
        ImVec2 p1 = WorldToScreen(tri.v1, view, proj, window_pos, panel_size);
        ImVec2 p2 = WorldToScreen(tri.v2, view, proj, window_pos, panel_size);

        // Skip off-screen triangles
        if (p0.x < -5000 || p1.x < -5000 || p2.x < -5000) continue;

        dl->AddTriangleFilled(p0, p1, p2, fill_col);
        dl->AddTriangle(p0, p1, p2, wire_col, 1.0f);
    }

    // Draw bounds
    if (state.show_bounds) {
        auto& bmin = state.baked_data.bounds_min;
        auto& bmax = state.baked_data.bounds_max;
        glm::vec3 corners[8] = {
            {bmin.x, bmin.y, bmin.z}, {bmax.x, bmin.y, bmin.z},
            {bmax.x, bmin.y, bmax.z}, {bmin.x, bmin.y, bmax.z},
            {bmin.x, bmax.y, bmin.z}, {bmax.x, bmax.y, bmin.z},
            {bmax.x, bmax.y, bmax.z}, {bmin.x, bmax.y, bmax.z},
        };
        ImVec2 pts[8];
        for (int j = 0; j < 8; j++) pts[j] = WorldToScreen(corners[j], view, proj, window_pos, panel_size);
        static const int edges[12][2] = {
            {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
        };
        for (auto& e : edges) {
            dl->AddLine(pts[e[0]], pts[e[1]], IM_COL32(255, 200, 0, 120), 1.0f);
        }
    }
}

} // namespace dse::editor

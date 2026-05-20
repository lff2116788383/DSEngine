#include "editor_navmesh_panel.h"
#include "editor_context.h"
#include "editor_icons.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
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

/// Simulate a navmesh bake from scene geometry (creates a simple grid-based navmesh
/// from walkable surfaces). In production this would use Recast/Detour.
void SimulateBake(NavMeshEditorState& state, entt::registry& reg) {
    state.baked_data = NavMeshData{};
    state.baking = false;

    // Collect AABB of all mesh-bearing entities
    glm::vec3 scene_min(1e6f), scene_max(-1e6f);
    int mesh_count = 0;

    auto view = reg.view<TransformComponent, dse::MeshRendererComponent>();
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e);
        glm::vec3 half = tf.scale * 0.5f;
        scene_min = glm::min(scene_min, tf.position - half);
        scene_max = glm::max(scene_max, tf.position + half);
        mesh_count++;
    }

    // Also include terrain entities
    auto tview = reg.view<TransformComponent, TerrainComponent>();
    for (auto e : tview) {
        auto& tf = tview.get<TransformComponent>(e);
        auto& terrain = tview.get<TerrainComponent>(e);
        glm::vec3 half(terrain.width * 0.5f, terrain.max_height, terrain.depth * 0.5f);
        scene_min = glm::min(scene_min, tf.position - half);
        scene_max = glm::max(scene_max, tf.position + half);
        mesh_count++;
    }

    if (mesh_count == 0) return;

    state.baked_data.bounds_min = scene_min;
    state.baked_data.bounds_max = scene_max;

    // Generate a simple grid navmesh on the Y=0 plane (or terrain Y)
    float cell = state.settings.cell_size;
    float y = scene_min.y + 0.01f; // Slightly above ground

    int nx = static_cast<int>((scene_max.x - scene_min.x) / cell);
    int nz = static_cast<int>((scene_max.z - scene_min.z) / cell);
    nx = std::clamp(nx, 1, 200);
    nz = std::clamp(nz, 1, 200);

    for (int iz = 0; iz < nz; iz++) {
        for (int ix = 0; ix < nx; ix++) {
            float x0 = scene_min.x + ix * cell;
            float z0 = scene_min.z + iz * cell;
            float x1 = x0 + cell;
            float z1 = z0 + cell;

            // Two triangles per cell
            NavMeshTriangle t1;
            t1.v0 = glm::vec3(x0, y, z0);
            t1.v1 = glm::vec3(x1, y, z0);
            t1.v2 = glm::vec3(x1, y, z1);
            state.baked_data.triangles.push_back(t1);

            NavMeshTriangle t2;
            t2.v0 = glm::vec3(x0, y, z0);
            t2.v1 = glm::vec3(x1, y, z1);
            t2.v2 = glm::vec3(x0, y, z1);
            state.baked_data.triangles.push_back(t2);
        }
    }

    state.baked_data.triangle_count = static_cast<int>(state.baked_data.triangles.size());
    state.baked_data.vertex_count = state.baked_data.triangle_count * 3;
    state.baked_data.poly_count = nx * nz;
    state.baked_data.valid = true;
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

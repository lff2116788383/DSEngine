#include "editor_terrain_panel.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>

namespace dse::editor {

TerrainEditorState& GetTerrainEditorState() {
    static TerrainEditorState state;
    return state;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static glm::vec2 WorldToScreen(const glm::vec3& world_pos,
                                const glm::mat4& view,
                                const glm::mat4& proj,
                                const glm::vec2& window_pos,
                                const glm::vec2& panel_size) {
    glm::vec4 clip = proj * view * glm::vec4(world_pos, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return glm::vec2(-10000.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float sx = window_pos.x + (ndc.x + 1.0f) * 0.5f * panel_size.x;
    float sy = window_pos.y + (1.0f - ndc.y) * 0.5f * panel_size.y;
    return glm::vec2(sx, sy);
}

static glm::vec3 ScreenToWorldOnTerrain(const glm::vec2& screen_pos,
                                         const glm::mat4& view,
                                         const glm::mat4& proj,
                                         const glm::vec2& window_pos,
                                         const glm::vec2& panel_size,
                                         float plane_y) {
    float nx = (screen_pos.x - window_pos.x) / panel_size.x * 2.0f - 1.0f;
    float ny = 1.0f - (screen_pos.y - window_pos.y) / panel_size.y * 2.0f;

    glm::mat4 inv_vp = glm::inverse(proj * view);
    glm::vec4 near_pt = inv_vp * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv_vp * glm::vec4(nx, ny,  1.0f, 1.0f);
    near_pt /= near_pt.w;
    far_pt  /= far_pt.w;

    glm::vec3 ray_origin(near_pt);
    glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt) - ray_origin);

    // Intersect with Y = plane_y horizontal plane (terrain is XZ)
    if (std::abs(ray_dir.y) < 1e-6f) return glm::vec3(0.0f);
    float t = (plane_y - ray_origin.y) / ray_dir.y;
    return ray_origin + ray_dir * t;
}

static bool WorldToTerrainGrid(const glm::vec3& world_pos,
                                const TerrainComponent& terrain,
                                const TransformComponent& tf,
                                float& out_gx, float& out_gz) {
    // Transform to terrain-local space
    float local_x = world_pos.x - tf.position.x;
    float local_z = world_pos.z - tf.position.z;

    // Terrain grid goes from -width/2 to +width/2
    float half_w = terrain.width * 0.5f;
    float half_d = terrain.depth * 0.5f;

    out_gx = (local_x + half_w) / terrain.width * static_cast<float>(terrain.resolution_x - 1);
    out_gz = (local_z + half_d) / terrain.depth * static_cast<float>(terrain.resolution_z - 1);

    return out_gx >= 0 && out_gx < terrain.resolution_x &&
           out_gz >= 0 && out_gz < terrain.resolution_z;
}

static float GaussianFalloff(float dist, float radius, float falloff) {
    if (dist >= radius) return 0.0f;
    float t = dist / radius;
    // Lerp between hard (1.0 until edge) and soft (gaussian)
    float hard = 1.0f;
    float soft = std::exp(-4.0f * t * t);
    return hard * (1.0f - falloff) + soft * falloff;
}

// ---------------------------------------------------------------------------
// Sculpting operations
// ---------------------------------------------------------------------------

static void ApplyBrush(TerrainComponent& terrain,
                        const TransformComponent& tf,
                        const glm::vec3& world_hit,
                        const TerrainEditorState& state,
                        float delta_time) {
    if (terrain.height_data.empty()) return;
    if (terrain.resolution_x < 2 || terrain.resolution_z < 2) return;

    float gx, gz;
    WorldToTerrainGrid(world_hit, terrain, tf, gx, gz);

    float dx_world = terrain.width / static_cast<float>(terrain.resolution_x - 1);
    float dz_world = terrain.depth / static_cast<float>(terrain.resolution_z - 1);

    // Radius in grid cells
    int radius_cells_x = static_cast<int>(std::ceil(state.brush_radius / dx_world));
    int radius_cells_z = static_cast<int>(std::ceil(state.brush_radius / dz_world));

    int center_x = static_cast<int>(std::round(gx));
    int center_z = static_cast<int>(std::round(gz));

    // For smooth: compute average in brush area first
    float avg_height = 0.0f;
    float total_weight = 0.0f;
    if (state.brush_mode == TerrainBrushMode::Smooth) {
        for (int z = center_z - radius_cells_z; z <= center_z + radius_cells_z; z++) {
            for (int x = center_x - radius_cells_x; x <= center_x + radius_cells_x; x++) {
                if (x < 0 || x >= terrain.resolution_x || z < 0 || z >= terrain.resolution_z) continue;
                float wx = static_cast<float>(x) * dx_world - terrain.width * 0.5f + tf.position.x;
                float wz = static_cast<float>(z) * dz_world - terrain.depth * 0.5f + tf.position.z;
                float dist = std::sqrt((wx - world_hit.x) * (wx - world_hit.x) +
                                       (wz - world_hit.z) * (wz - world_hit.z));
                float w = GaussianFalloff(dist, state.brush_radius, state.brush_falloff);
                if (w > 0.0f) {
                    avg_height += terrain.height_data[z * terrain.resolution_x + x] * w;
                    total_weight += w;
                }
            }
        }
        if (total_weight > 0.0f) avg_height /= total_weight;
    }

    float strength = state.brush_strength * delta_time * 30.0f; // Normalize to ~30fps

    for (int z = center_z - radius_cells_z; z <= center_z + radius_cells_z; z++) {
        for (int x = center_x - radius_cells_x; x <= center_x + radius_cells_x; x++) {
            if (x < 0 || x >= terrain.resolution_x || z < 0 || z >= terrain.resolution_z) continue;

            float wx = static_cast<float>(x) * dx_world - terrain.width * 0.5f + tf.position.x;
            float wz = static_cast<float>(z) * dz_world - terrain.depth * 0.5f + tf.position.z;
            float dist = std::sqrt((wx - world_hit.x) * (wx - world_hit.x) +
                                   (wz - world_hit.z) * (wz - world_hit.z));
            float w = GaussianFalloff(dist, state.brush_radius, state.brush_falloff);
            if (w <= 0.0f) continue;

            int idx = z * terrain.resolution_x + x;
            float& h = terrain.height_data[idx];

            switch (state.brush_mode) {
                case TerrainBrushMode::Raise:
                    h += w * strength;
                    break;
                case TerrainBrushMode::Lower:
                    h -= w * strength;
                    break;
                case TerrainBrushMode::Smooth:
                    h += (avg_height - h) * w * strength;
                    break;
                case TerrainBrushMode::Flatten:
                    h += (state.flatten_target_height - h) * w * strength;
                    break;
            }
            h = std::clamp(h, 0.0f, terrain.max_height);
        }
    }

    terrain.is_dirty = true;
}

// ---------------------------------------------------------------------------
// Panel drawing
// ---------------------------------------------------------------------------

void DrawTerrainEditorPanel(entt::registry& registry, entt::entity selected_entity) {
    ImGui::Begin("Terrain Brush");

    auto& state = GetTerrainEditorState();

    // Auto-select terrain entity
    if (selected_entity != entt::null &&
        registry.valid(selected_entity) &&
        registry.all_of<TerrainComponent>(selected_entity)) {
        state.active_terrain = selected_entity;
        state.editing_active = true;
    }

    if (state.active_terrain != entt::null &&
        registry.valid(state.active_terrain) &&
        registry.all_of<TerrainComponent>(state.active_terrain)) {

        auto& terrain = registry.get<TerrainComponent>(state.active_terrain);

        ImGui::Text("Terrain: %dx%d  Size: %.0fx%.0f",
                     terrain.resolution_x, terrain.resolution_z,
                     terrain.width, terrain.depth);
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect##terrain")) {
            state.active_terrain = entt::null;
            state.editing_active = false;
        }

        ImGui::Separator();
        ImGui::Text("Sculpt Tools");

        // Brush mode buttons
        struct ToolDef { TerrainBrushMode mode; const char* label; ImVec4 color; };
        ToolDef tools[] = {
            {TerrainBrushMode::Raise,   "Raise",   ImVec4(0.3f, 0.6f, 0.9f, 1.0f)},
            {TerrainBrushMode::Lower,   "Lower",   ImVec4(0.9f, 0.4f, 0.3f, 1.0f)},
            {TerrainBrushMode::Smooth,  "Smooth",  ImVec4(0.4f, 0.8f, 0.4f, 1.0f)},
            {TerrainBrushMode::Flatten, "Flatten", ImVec4(0.8f, 0.7f, 0.3f, 1.0f)},
        };
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine();
            bool active = (state.brush_mode == tools[i].mode);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, tools[i].color);
            if (ImGui::Button(tools[i].label, ImVec2(65, 24))) {
                state.brush_mode = tools[i].mode;
            }
            if (active) ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Text("Brush Settings");
        ImGui::SliderFloat("Radius", &state.brush_radius, 0.5f, 30.0f, "%.1f");
        ImGui::SliderFloat("Strength", &state.brush_strength, 0.01f, 2.0f, "%.2f");
        ImGui::SliderFloat("Falloff", &state.brush_falloff, 0.0f, 1.0f, "%.2f");

        if (state.brush_mode == TerrainBrushMode::Flatten) {
            ImGui::SliderFloat("Target H", &state.flatten_target_height, 0.0f, terrain.max_height, "%.1f");
            ImGui::SameLine();
            if (ImGui::SmallButton("Pick##flatten_pick")) {
                // Pick height from center of terrain under cursor (simplified)
                int cx = terrain.resolution_x / 2;
                int cz = terrain.resolution_z / 2;
                if (!terrain.height_data.empty()) {
                    state.flatten_target_height = terrain.height_data[cz * terrain.resolution_x + cx];
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Splat Layer: %d", state.active_splat_layer);
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine();
            char lbl[16]; snprintf(lbl, sizeof(lbl), "L%d", i);
            bool sel = (state.active_splat_layer == i);
            if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.6f, 0.8f, 1.0f));
            if (ImGui::Button(lbl, ImVec2(32, 24))) {
                state.active_splat_layer = i;
            }
            if (sel) ImGui::PopStyleColor();
        }
        ImGui::TextDisabled("(Splat painting requires splat map textures)");

        ImGui::Separator();
        if (ImGui::Button("Reset Heights to 0")) {
            std::fill(terrain.height_data.begin(), terrain.height_data.end(), 0.0f);
            terrain.is_dirty = true;
        }

    } else {
        ImGui::TextDisabled("Select a TerrainComponent entity to start sculpting.");
        state.editing_active = false;

        ImGui::Separator();
        ImGui::Text("Available Terrains:");
        auto tv = registry.view<TerrainComponent>();
        for (auto e : tv) {
            char buf[64]; snprintf(buf, sizeof(buf), "Entity %u", static_cast<unsigned>(e));
            if (ImGui::Selectable(buf)) {
                state.active_terrain = e;
                state.editing_active = true;
            }
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Viewport brush overlay
// ---------------------------------------------------------------------------

void DrawTerrainBrushOverlay(entt::registry& registry,
                             const glm::vec2& window_pos,
                             const glm::vec2& panel_size,
                             const glm::mat4& view,
                             const glm::mat4& proj) {
    auto& state = GetTerrainEditorState();
    if (!state.editing_active) return;
    if (state.active_terrain == entt::null) return;
    if (!registry.valid(state.active_terrain)) return;
    if (!registry.all_of<TerrainComponent, TransformComponent>(state.active_terrain)) return;

    auto& tf = registry.get<TransformComponent>(state.active_terrain);

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec3 hit = ScreenToWorldOnTerrain(glm::vec2(mouse.x, mouse.y), view, proj,
                                            window_pos, panel_size, tf.position.y);

    // Draw a projected circle on the Y=terrain_y plane
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int segments = 48;
    ImU32 brush_color = IM_COL32(255, 200, 50, 140);
    if (state.brush_mode == TerrainBrushMode::Lower) brush_color = IM_COL32(255, 80, 80, 140);
    else if (state.brush_mode == TerrainBrushMode::Smooth) brush_color = IM_COL32(80, 255, 80, 140);
    else if (state.brush_mode == TerrainBrushMode::Flatten) brush_color = IM_COL32(255, 255, 80, 140);

    ImVec2 pts[48];
    for (int i = 0; i < segments; i++) {
        float angle = static_cast<float>(i) / static_cast<float>(segments) * 6.28318530f;
        glm::vec3 wp = hit + glm::vec3(std::cos(angle) * state.brush_radius, 0.0f,
                                         std::sin(angle) * state.brush_radius);
        glm::vec2 sp = WorldToScreen(wp, view, proj, window_pos, panel_size);
        pts[i] = ImVec2(sp.x, sp.y);
    }
    dl->AddPolyline(pts, segments, brush_color, ImDrawFlags_Closed, 2.0f);

    // Inner falloff circle
    float inner_r = state.brush_radius * (1.0f - state.brush_falloff);
    if (inner_r > 0.1f) {
        ImVec2 inner_pts[48];
        for (int i = 0; i < segments; i++) {
            float angle = static_cast<float>(i) / static_cast<float>(segments) * 6.28318530f;
            glm::vec3 wp = hit + glm::vec3(std::cos(angle) * inner_r, 0.0f,
                                             std::sin(angle) * inner_r);
            glm::vec2 sp = WorldToScreen(wp, view, proj, window_pos, panel_size);
            inner_pts[i] = ImVec2(sp.x, sp.y);
        }
        dl->AddPolyline(inner_pts, segments, IM_COL32(255, 255, 255, 60), ImDrawFlags_Closed, 1.0f);
    }

    // Crosshair at center
    glm::vec2 center = WorldToScreen(hit, view, proj, window_pos, panel_size);
    dl->AddLine(ImVec2(center.x - 6, center.y), ImVec2(center.x + 6, center.y), brush_color, 1.0f);
    dl->AddLine(ImVec2(center.x, center.y - 6), ImVec2(center.x, center.y + 6), brush_color, 1.0f);
}

// ---------------------------------------------------------------------------
// Viewport sculpt handling
// ---------------------------------------------------------------------------

bool HandleTerrainViewportSculpt(entt::registry& registry,
                                 const glm::vec2& window_pos,
                                 const glm::vec2& panel_size,
                                 const glm::mat4& view,
                                 const glm::mat4& proj,
                                 float delta_time) {
    auto& state = GetTerrainEditorState();
    if (!state.editing_active) return false;
    if (state.active_terrain == entt::null) return false;
    if (!registry.valid(state.active_terrain)) return false;
    if (!registry.all_of<TerrainComponent, TransformComponent>(state.active_terrain)) return false;

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt) return false; // Alt = orbit

    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (!mouse_down) {
        state.painting = false;
        return false;
    }

    auto& terrain = registry.get<TerrainComponent>(state.active_terrain);
    auto& tf = registry.get<TransformComponent>(state.active_terrain);

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec3 hit = ScreenToWorldOnTerrain(glm::vec2(mouse.x, mouse.y), view, proj,
                                            window_pos, panel_size, tf.position.y);

    // Check if hit is within terrain bounds
    float half_w = terrain.width * 0.5f + state.brush_radius;
    float half_d = terrain.depth * 0.5f + state.brush_radius;
    float lx = hit.x - tf.position.x;
    float lz = hit.z - tf.position.z;
    if (std::abs(lx) > half_w || std::abs(lz) > half_d) return false;

    state.painting = true;
    ApplyBrush(terrain, tf, hit, state, delta_time);
    return true;
}

} // namespace dse::editor

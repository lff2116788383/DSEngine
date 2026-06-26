#include "editor_terrain_panel.h"
#include "editor_terrain_panel_core.h"
#include "editor_context.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "editor_icons.h"
#include "editor_undo.h"
#include "editor_shortcuts.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dse::editor {

TerrainEditorState& GetTerrainEditorState() {
    static TerrainEditorState state;
    return state;
}

// NOTE: WorldToScreen / ScreenToWorldOnTerrain / WorldToTerrainGrid / GaussianFalloff /
// ApplyBrush / EnsureSplatData / ApplySplatBrush are defined in editor_terrain_panel_core.cpp
// (pure math/data logic, headless-testable). This file keeps only ImGui panels + viewport wiring.

// ---------------------------------------------------------------------------
// Panel drawing
// ---------------------------------------------------------------------------

void DrawTerrainEditorPanel(EditorContext& ctx) {
    auto& registry = ctx.registry;
    auto selected_entity = ctx.selected_entity;
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
                if (state.last_brush_hit_valid && !terrain.height_data.empty()) {
                    auto& tf = registry.get<TransformComponent>(state.active_terrain);
                    float gx, gz;
                    if (WorldToTerrainGrid(state.last_brush_hit, terrain, tf, gx, gz)) {
                        int ix = std::clamp(static_cast<int>(std::round(gx)), 0, terrain.resolution_x - 1);
                        int iz = std::clamp(static_cast<int>(std::round(gz)), 0, terrain.resolution_z - 1);
                        state.flatten_target_height = terrain.height_data[iz * terrain.resolution_x + ix];
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Pick height from brush cursor position");
            }
        }

        ImGui::Separator();

        // Mode toggle: Sculpt vs Splat Paint
        {
            bool sculpt_active = !state.splat_paint_mode;
            bool splat_active = state.splat_paint_mode;
            if (sculpt_active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
            if (ImGui::Button("Sculpt", ImVec2(80, 24))) state.splat_paint_mode = false;
            if (sculpt_active) ImGui::PopStyleColor();
            ImGui::SameLine();
            if (splat_active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.5f, 0.2f, 1.0f));
            if (ImGui::Button("Splat Paint", ImVec2(80, 24))) state.splat_paint_mode = true;
            if (splat_active) ImGui::PopStyleColor();
        }

        if (state.splat_paint_mode) {
            ImGui::Separator();
            ImGui::Text("Splat Layer: %d", state.active_splat_layer);
            const ImVec4 layer_colors[4] = {
                ImVec4(0.8f, 0.3f, 0.3f, 1.0f),
                ImVec4(0.3f, 0.8f, 0.3f, 1.0f),
                ImVec4(0.3f, 0.3f, 0.8f, 1.0f),
                ImVec4(0.8f, 0.8f, 0.3f, 1.0f),
            };
            const ImU32 layer_swatch_colors[4] = {
                IM_COL32(200, 80, 80, 255),
                IM_COL32(80, 200, 80, 255),
                IM_COL32(80, 80, 200, 255),
                IM_COL32(200, 200, 80, 255),
            };
            const char* layer_default_names[4] = { "Base", "Layer 1", "Layer 2", "Layer 3" };

            // Layer selection cards with color swatch + texture name
            for (int i = 0; i < 4; i++) {
                bool sel = (state.active_splat_layer == i);
                ImVec2 card_min = ImGui::GetCursorScreenPos();
                float card_w = ImGui::GetContentRegionAvail().x;
                float card_h = 36.0f;

                if (sel) {
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(layer_colors[i].x * 0.3f, layer_colors[i].y * 0.3f, layer_colors[i].z * 0.3f, 0.5f));
                }

                char child_id[32]; snprintf(child_id, sizeof(child_id), "##splat_card_%d", i);
                ImGui::BeginChild(child_id, ImVec2(card_w, card_h), true);

                // Color swatch
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 swatch_min = ImGui::GetCursorScreenPos();
                ImVec2 swatch_max = ImVec2(swatch_min.x + 24.0f, swatch_min.y + 24.0f);
                dl->AddRectFilled(swatch_min, swatch_max, layer_swatch_colors[i], 3.0f);
                if (sel) {
                    dl->AddRect(swatch_min, swatch_max, IM_COL32(255, 255, 255, 255), 3.0f, 0, 2.0f);
                }
                ImGui::Dummy(ImVec2(28.0f, 24.0f));
                ImGui::SameLine();

                // Layer name + texture filename
                std::string display_name;
                if (!terrain.splat_texture_paths[i].empty()) {
                    auto pos = terrain.splat_texture_paths[i].find_last_of("/\\");
                    display_name = (pos != std::string::npos)
                        ? terrain.splat_texture_paths[i].substr(pos + 1)
                        : terrain.splat_texture_paths[i];
                } else {
                    display_name = layer_default_names[i];
                }

                ImGui::BeginGroup();
                ImGui::Text("L%d: %s", i, display_name.c_str());
                if (terrain.splat_texture_paths[i].empty()) {
                    ImGui::TextDisabled("(no texture)");
                } else {
                    ImGui::TextDisabled("%s", terrain.splat_texture_paths[i].c_str());
                }
                ImGui::EndGroup();

                // Make entire card clickable
                ImVec2 card_content_min = ImGui::GetWindowPos();
                ImVec2 card_content_max = ImVec2(card_content_min.x + card_w, card_content_min.y + card_h);
                if (ImGui::IsMouseHoveringRect(card_content_min, card_content_max) && ImGui::IsMouseClicked(0)) {
                    state.active_splat_layer = i;
                }

                ImGui::EndChild();
                if (sel) ImGui::PopStyleColor();
            }

            ImGui::SliderFloat("Opacity", &state.splat_brush_opacity, 0.01f, 1.0f, "%.2f");

            // Texture path input for active layer
            ImGui::Separator();
            ImGui::Text("Layer %d Texture:", state.active_splat_layer);
            char tex_buf[256] = "";
            std::strncpy(tex_buf, terrain.splat_texture_paths[state.active_splat_layer].c_str(), sizeof(tex_buf) - 1);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##splat_tex_path", tex_buf, sizeof(tex_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                terrain.splat_texture_paths[state.active_splat_layer] = tex_buf;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enter texture path, press Enter to apply");
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Reset Heights to 0")) {
            std::fill(terrain.height_data.begin(), terrain.height_data.end(), 0.0f);
            terrain.is_dirty = true;
        }
        if (state.splat_paint_mode && ImGui::Button("Reset Splat to Layer 0")) {
            EnsureSplatData(terrain);
            for (int i = 0; i < terrain.resolution_x * terrain.resolution_z; i++) {
                terrain.splat_data[i * 4 + 0] = 1.0f;
                terrain.splat_data[i * 4 + 1] = 0.0f;
                terrain.splat_data[i * 4 + 2] = 0.0f;
                terrain.splat_data[i * 4 + 3] = 0.0f;
            }
            terrain.splat_dirty = true;
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

    state.last_brush_hit = hit;
    state.last_brush_hit_valid = true;

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

    // Handle stroke end -> push undo
    if (!mouse_down && state.painting) {
        state.painting = false;
        auto& terrain = registry.get<TerrainComponent>(state.active_terrain);
        entt::entity ent = state.active_terrain;
        auto& undo_mgr = GetUndoRedoManager();

        if (state.splat_paint_mode) {
            // Splat undo
            if (state.splat_snapshot != terrain.splat_data) {
                std::vector<float> old_splat = state.splat_snapshot;
                std::vector<float> new_splat = terrain.splat_data;
                std::string merge_id = "terrain_splat_" + std::to_string(static_cast<uint32_t>(ent));
                auto cmd = std::make_unique<LambdaCommand>(
                    "Terrain Splat Paint",
                    [&reg = registry, ent, new_splat]() {
                        if (reg.valid(ent) && reg.all_of<TerrainComponent>(ent)) {
                            auto& t = reg.get<TerrainComponent>(ent);
                            t.splat_data = new_splat;
                            t.splat_dirty = true;
                        }
                    },
                    [&reg = registry, ent, old_splat]() {
                        if (reg.valid(ent) && reg.all_of<TerrainComponent>(ent)) {
                            auto& t = reg.get<TerrainComponent>(ent);
                            t.splat_data = old_splat;
                            t.splat_dirty = true;
                        }
                    },
                    merge_id
                );
                undo_mgr.Execute(std::move(cmd), true);
            }
        } else {
            // Height sculpt undo
            if (state.height_snapshot != terrain.height_data) {
                std::vector<float> old_heights = state.height_snapshot;
                std::vector<float> new_heights = terrain.height_data;
                std::string merge_id = "terrain_sculpt_" + std::to_string(static_cast<uint32_t>(ent));
                auto cmd = std::make_unique<LambdaCommand>(
                    "Terrain Sculpt",
                    [&reg = registry, ent, new_heights]() {
                        if (reg.valid(ent) && reg.all_of<TerrainComponent>(ent)) {
                            auto& t = reg.get<TerrainComponent>(ent);
                            t.height_data = new_heights;
                            t.is_dirty = true;
                        }
                    },
                    [&reg = registry, ent, old_heights]() {
                        if (reg.valid(ent) && reg.all_of<TerrainComponent>(ent)) {
                            auto& t = reg.get<TerrainComponent>(ent);
                            t.height_data = old_heights;
                            t.is_dirty = true;
                        }
                    },
                    merge_id
                );
                undo_mgr.Execute(std::move(cmd), true);
            }
        }
        return false;
    }

    if (!mouse_down) return false;

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

    // Snapshot on stroke start
    if (!state.painting) {
        state.painting = true;
        if (state.splat_paint_mode) {
            EnsureSplatData(terrain);
            state.splat_snapshot = terrain.splat_data;
        } else {
            state.height_snapshot = terrain.height_data;
        }
    }

    if (state.splat_paint_mode) {
        ApplySplatBrush(terrain, tf, hit, state, delta_time);
    } else {
        ApplyBrush(terrain, tf, hit, state, delta_time);
    }
    return true;
}

} // namespace dse::editor

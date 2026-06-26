#include "editor_tilemap_panel.h"
#include "editor_tilemap_panel_core.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/tilemap.h"
#include "editor_icons.h"
#include "editor_undo.h"
#include "editor_shortcuts.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <queue>
#include <cstdlib>

namespace dse::editor {

TilemapEditorState& GetTilemapEditorState() {
    static TilemapEditorState state;
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
    float sy = window_pos.y + (1.0f - ndc.y) * 0.5f * panel_size.y; // Y flipped
    return glm::vec2(sx, sy);
}

static glm::vec3 ScreenToWorld(const glm::vec2& screen_pos,
                                const glm::mat4& view,
                                const glm::mat4& proj,
                                const glm::vec2& window_pos,
                                const glm::vec2& panel_size,
                                float world_z) {
    float nx = (screen_pos.x - window_pos.x) / panel_size.x * 2.0f - 1.0f;
    float ny = 1.0f - (screen_pos.y - window_pos.y) / panel_size.y * 2.0f;

    glm::mat4 inv_vp = glm::inverse(proj * view);
    glm::vec4 near_pt = inv_vp * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv_vp * glm::vec4(nx, ny,  1.0f, 1.0f);
    near_pt /= near_pt.w;
    far_pt  /= far_pt.w;

    glm::vec3 ray_origin(near_pt);
    glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt) - ray_origin);

    // Intersect with Z = world_z plane
    if (std::abs(ray_dir.z) < 1e-6f) return glm::vec3(0.0f);
    float t = (world_z - ray_origin.z) / ray_dir.z;
    return ray_origin + ray_dir * t;
}

// NOTE: WorldToTilemapCell / FloodFillTiles / AutoTileResolve / AutoTileResolveNeighbours /
// BresenhamLine are defined in editor_tilemap_panel_core.cpp (pure grid/data logic,
// headless-testable). This file keeps only ImGui panels + viewport wiring + the trivial
// projection helpers (WorldToScreen/ScreenToWorld) used by the overlay.

// ---------------------------------------------------------------------------
// Panel drawing
// ---------------------------------------------------------------------------

void DrawTilemapEditorPanel(entt::registry& registry, entt::entity selected_entity) {
    ImGui::Begin("Tile Palette");

    auto& state = GetTilemapEditorState();

    // Auto-select tilemap entity when selected entity has TilemapComponent
    if (selected_entity != entt::null &&
        registry.valid(selected_entity) &&
        registry.all_of<TilemapComponent>(selected_entity)) {
        state.active_tilemap = selected_entity;
        state.editing_active = true;
    }

    // Show active tilemap info
    if (state.active_tilemap != entt::null &&
        registry.valid(state.active_tilemap) &&
        registry.all_of<TilemapComponent>(state.active_tilemap)) {

        auto& tm = registry.get<TilemapComponent>(state.active_tilemap);

        ImGui::Text("Tilemap: %dx%d  Tile: %.1f",
                     tm.width, tm.height, tm.tile_size);
        ImGui::SameLine();
        if (ImGui::SmallButton("Deselect")) {
            state.active_tilemap = entt::null;
            state.editing_active = false;
        }

        ImGui::Separator();

        // --- Brush tools ---
        auto ToolButton = [&](const char* label, TilemapBrushTool tool, ImVec4 color) {
            bool active = (state.active_tool == tool);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, color);
            if (ImGui::Button(label, ImVec2(55, 24))) state.active_tool = tool;
            if (active) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        ToolButton("Paint", TilemapBrushTool::Paint, ImVec4(0.3f, 0.55f, 0.8f, 1));
        ToolButton("Erase", TilemapBrushTool::Erase, ImVec4(0.8f, 0.3f, 0.3f, 1));
        ToolButton("Fill",  TilemapBrushTool::FloodFill, ImVec4(0.3f, 0.7f, 0.4f, 1));
        ToolButton("Line",  TilemapBrushTool::Line, ImVec4(0.6f, 0.5f, 0.2f, 1));
        ToolButton("Rect",  TilemapBrushTool::Rectangle, ImVec4(0.5f, 0.3f, 0.7f, 1));
        ImGui::NewLine();

        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("Brush Size", &state.brush_size, 1, 5);

        // Auto-tile toggle
        ImGui::Checkbox("Auto Tile", &state.auto_tile_rule.enabled);
        ImGui::SameLine();
        if (ImGui::SmallButton("Config##at")) {
            state.show_auto_tile_config = !state.show_auto_tile_config;
        }
        if (state.show_auto_tile_config) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(80);
            ImGui::InputInt("Base ID", &state.auto_tile_rule.base_tile_id);
            ImGui::Text("4-bit mask: U=1 R=2 D=4 L=8");
            for (int m = 0; m < 16; m++) {
                ImGui::PushID(m);
                ImGui::SetNextItemWidth(50);
                char mlabel[16]; snprintf(mlabel, sizeof(mlabel), "%X", m);
                ImGui::InputInt(mlabel, &state.auto_tile_rule.variant_tiles[m], 0);
                if ((m & 3) != 3) ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::Unindent();
        }

        ImGui::Separator();

        // --- Tile Palette Grid ---
        ImGui::Text("Tile ID: %d", state.selected_tile_id);

        const int palette_cols = tm.tileset_cols > 0 ? tm.tileset_cols : 8;
        const int palette_rows = tm.tileset_rows > 0 ? tm.tileset_rows : 8;
        const float cell_size = 28.0f;
        const float spacing = 2.0f;

        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Reserve space
        float grid_w = static_cast<float>(palette_cols) * (cell_size + spacing);
        float grid_h = static_cast<float>(palette_rows) * (cell_size + spacing);
        ImGui::Dummy(ImVec2(grid_w, grid_h));

        for (int ty = 0; ty < palette_rows; ty++) {
            for (int tx = 0; tx < palette_cols; tx++) {
                int tile_id = ty * palette_cols + tx + 1; // 1-based
                float px = origin.x + static_cast<float>(tx) * (cell_size + spacing);
                float py = origin.y + static_cast<float>(ty) * (cell_size + spacing);
                ImVec2 p0(px, py);
                ImVec2 p1(px + cell_size, py + cell_size);

                // Color-code tiles (hash for variety)
                unsigned int hash = static_cast<unsigned int>(tile_id) * 2654435761u;
                ImU32 col = IM_COL32(
                    60 + ((hash >> 0) & 0x7F),
                    60 + ((hash >> 8) & 0x7F),
                    60 + ((hash >> 16) & 0x7F),
                    255);
                dl->AddRectFilled(p0, p1, col);

                // Highlight selected
                if (tile_id == state.selected_tile_id) {
                    dl->AddRect(p0, p1, IM_COL32(255, 220, 50, 255), 0.0f, 0, 2.5f);
                }

                // Label
                char label[8];
                snprintf(label, sizeof(label), "%d", tile_id);
                dl->AddText(ImVec2(px + 2, py + 2), IM_COL32(255, 255, 255, 200), label);

                // Click to select
                ImGui::SetCursorScreenPos(p0);
                ImGui::InvisibleButton(("##tile_" + std::to_string(tile_id)).c_str(),
                                       ImVec2(cell_size, cell_size));
                if (ImGui::IsItemClicked()) {
                    state.selected_tile_id = tile_id;
                }
            }
        }

        ImGui::Separator();

        // --- Quick resize ---
        ImGui::Text("Resize Map");
        static int new_w = 0, new_h = 0;
        if (new_w == 0) { new_w = tm.width; new_h = tm.height; }
        ImGui::SetNextItemWidth(60); ImGui::InputInt("W##tilemap_w", &new_w, 0); ImGui::SameLine();
        ImGui::SetNextItemWidth(60); ImGui::InputInt("H##tilemap_h", &new_h, 0); ImGui::SameLine();
        if (ImGui::Button("Apply##resize_tm")) {
            new_w = (std::max)(1, new_w);
            new_h = (std::max)(1, new_h);
            std::vector<int> old_tiles = tm.tiles;
            int old_w = tm.width, old_h = tm.height;
            tm.width = new_w;
            tm.height = new_h;
            tm.tiles.assign(static_cast<size_t>(new_w * new_h), 0);
            for (int y = 0; y < (std::min)(old_h, new_h); y++) {
                for (int x = 0; x < (std::min)(old_w, new_w); x++) {
                    tm.tiles[y * new_w + x] = old_tiles[y * old_w + x];
                }
            }
            tm.dirty = true;
        }
        if (ImGui::Button("Clear All Tiles")) {
            std::fill(tm.tiles.begin(), tm.tiles.end(), 0);
            tm.dirty = true;
        }

        // Random scatter button
        ImGui::SameLine();
        if (ImGui::Button("Random Scatter")) {
            for (auto& t : tm.tiles) {
                if (t == 0) continue; // skip empty
                t = 1 + (std::rand() % (tm.tileset_cols * tm.tileset_rows));
            }
            tm.dirty = true;
        }

    } else {
        ImGui::TextDisabled("Select a TilemapComponent entity to start painting.");
        state.editing_active = false;

        // List available tilemaps
        ImGui::Separator();
        ImGui::Text("Available Tilemaps:");
        auto tmview = registry.view<TilemapComponent>();
        for (auto e : tmview) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Entity %u", static_cast<unsigned>(e));
            if (ImGui::Selectable(buf)) {
                state.active_tilemap = e;
                state.editing_active = true;
            }
        }
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Viewport grid overlay
// ---------------------------------------------------------------------------

void DrawTilemapGridOverlay(entt::registry& registry,
                            const glm::vec2& window_pos,
                            const glm::vec2& panel_size,
                            const glm::mat4& view,
                            const glm::mat4& proj) {
    auto& state = GetTilemapEditorState();
    if (!state.editing_active) return;
    if (state.active_tilemap == entt::null) return;
    if (!registry.valid(state.active_tilemap)) return;
    if (!registry.all_of<TilemapComponent, TransformComponent>(state.active_tilemap)) return;

    auto& tm = registry.get<TilemapComponent>(state.active_tilemap);
    auto& tf = registry.get<TransformComponent>(state.active_tilemap);

    if (tm.width <= 0 || tm.height <= 0 || tm.tile_size <= 0.0f) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    float map_w = static_cast<float>(tm.width) * tm.tile_size;
    float map_h = static_cast<float>(tm.height) * tm.tile_size;
    float origin_x = tf.position.x - map_w * 0.5f;
    float origin_y = tf.position.y - map_h * 0.5f;
    float z = tf.position.z;

    const ImU32 grid_color = IM_COL32(100, 200, 255, 60);
    const ImU32 border_color = IM_COL32(100, 200, 255, 140);

    // Vertical lines
    for (int x = 0; x <= tm.width; x++) {
        float wx = origin_x + static_cast<float>(x) * tm.tile_size;
        glm::vec2 s0 = WorldToScreen(glm::vec3(wx, origin_y, z), view, proj, window_pos, panel_size);
        glm::vec2 s1 = WorldToScreen(glm::vec3(wx, origin_y + map_h, z), view, proj, window_pos, panel_size);
        ImU32 c = (x == 0 || x == tm.width) ? border_color : grid_color;
        dl->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), c);
    }

    // Horizontal lines
    for (int y = 0; y <= tm.height; y++) {
        float wy = origin_y + static_cast<float>(y) * tm.tile_size;
        glm::vec2 s0 = WorldToScreen(glm::vec3(origin_x, wy, z), view, proj, window_pos, panel_size);
        glm::vec2 s1 = WorldToScreen(glm::vec3(origin_x + map_w, wy, z), view, proj, window_pos, panel_size);
        ImU32 c = (y == 0 || y == tm.height) ? border_color : grid_color;
        dl->AddLine(ImVec2(s0.x, s0.y), ImVec2(s1.x, s1.y), c);
    }

    // Highlight hovered cell
    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec3 world_mouse = ScreenToWorld(glm::vec2(mouse.x, mouse.y), view, proj,
                                           window_pos, panel_size, z);
    int hx, hy;
    if (WorldToTilemapCell(world_mouse, tm, tf, hx, hy)) {
        float cx = origin_x + static_cast<float>(hx) * tm.tile_size;
        float cy = origin_y + static_cast<float>(hy) * tm.tile_size;
        glm::vec2 c0 = WorldToScreen(glm::vec3(cx, cy, z), view, proj, window_pos, panel_size);
        glm::vec2 c1 = WorldToScreen(glm::vec3(cx + tm.tile_size, cy + tm.tile_size, z),
                                       view, proj, window_pos, panel_size);
        dl->AddRectFilled(ImVec2(c0.x, c0.y), ImVec2(c1.x, c1.y), IM_COL32(255, 255, 100, 40));
        dl->AddRect(ImVec2(c0.x, c0.y), ImVec2(c1.x, c1.y), IM_COL32(255, 255, 100, 160), 0.0f, 0, 1.5f);
    }
}

// ---------------------------------------------------------------------------
// Viewport paint handling
// ---------------------------------------------------------------------------

bool HandleTilemapViewportPaint(entt::registry& registry,
                                const glm::vec2& window_pos,
                                const glm::vec2& panel_size,
                                const glm::mat4& view,
                                const glm::mat4& proj) {
    auto& state = GetTilemapEditorState();
    if (!state.editing_active) return false;
    if (state.active_tilemap == entt::null) return false;
    if (!registry.valid(state.active_tilemap)) return false;
    if (!registry.all_of<TilemapComponent, TransformComponent>(state.active_tilemap)) return false;

    // Only paint on left-click (not Alt-held, that's orbit)
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyAlt) return false;

    bool mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouse_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    auto& tm = registry.get<TilemapComponent>(state.active_tilemap);
    auto& tf = registry.get<TransformComponent>(state.active_tilemap);

    // Handle stroke end -> push undo
    if (!mouse_down && state.painting) {
        state.painting = false;
        if (state.tiles_snapshot != tm.tiles) {
            std::vector<int> old_tiles = state.tiles_snapshot;
            std::vector<int> new_tiles = tm.tiles;
            entt::entity ent = state.active_tilemap;
            auto& undo_mgr = GetUndoRedoManager();
            auto cmd = std::make_unique<LambdaCommand>(
                "Tilemap Paint",
                [&reg = registry, ent, new_tiles]() {
                    if (reg.valid(ent) && reg.all_of<TilemapComponent>(ent)) {
                        auto& t = reg.get<TilemapComponent>(ent);
                        t.tiles = new_tiles;
                        t.dirty = true;
                    }
                },
                [&reg = registry, ent, old_tiles]() {
                    if (reg.valid(ent) && reg.all_of<TilemapComponent>(ent)) {
                        auto& t = reg.get<TilemapComponent>(ent);
                        t.tiles = old_tiles;
                        t.dirty = true;
                    }
                }
            );
            undo_mgr.Execute(std::move(cmd), false);
        }
        return false;
    }

    if (!mouse_down) return false;

    if (tm.width <= 0 || tm.height <= 0) return false;

    // Snapshot on stroke start
    if (mouse_clicked && !state.painting) {
        state.painting = true;
        state.tiles_snapshot = tm.tiles;
    }

    ImVec2 mouse = ImGui::GetMousePos();
    glm::vec3 world_mouse = ScreenToWorld(glm::vec2(mouse.x, mouse.y), view, proj,
                                           window_pos, panel_size, tf.position.z);

    int cx, cy;
    if (!WorldToTilemapCell(world_mouse, tm, tf, cx, cy)) return true;

    int half_brush = state.brush_size / 2;
    bool changed = false;

    bool is_line = (state.active_tool == TilemapBrushTool::Line);
    bool is_rect = (state.active_tool == TilemapBrushTool::Rectangle);

    if (is_line || is_rect) {
        // Drag-based tools: record start on click, apply on release
        if (mouse_clicked) {
            state.drag_started = true;
            state.drag_start_cx = cx;
            state.drag_start_cy = cy;
        }
        if (state.drag_started && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            state.drag_started = false;
            int paint_id = state.selected_tile_id;
            if (is_line) {
                auto pts = BresenhamLine(state.drag_start_cx, state.drag_start_cy, cx, cy);
                for (auto& [lx, ly] : pts) {
                    if (lx >= 0 && lx < tm.width && ly >= 0 && ly < tm.height) {
                        tm.tiles[ly * tm.width + lx] = paint_id;
                        AutoTileResolveNeighbours(tm, lx, ly, state.auto_tile_rule);
                        changed = true;
                    }
                }
            } else { // Rectangle
                int x0 = (std::min)(state.drag_start_cx, cx);
                int x1 = (std::max)(state.drag_start_cx, cx);
                int y0 = (std::min)(state.drag_start_cy, cy);
                int y1 = (std::max)(state.drag_start_cy, cy);
                for (int ry = y0; ry <= y1; ry++) {
                    for (int rx = x0; rx <= x1; rx++) {
                        if (rx >= 0 && rx < tm.width && ry >= 0 && ry < tm.height) {
                            tm.tiles[ry * tm.width + rx] = paint_id;
                            changed = true;
                        }
                    }
                }
                // Auto-tile resolve for entire rect + border
                if (state.auto_tile_rule.enabled) {
                    for (int ry = y0 - 1; ry <= y1 + 1; ry++) {
                        for (int rx = x0 - 1; rx <= x1 + 1; rx++) {
                            if (rx >= 0 && rx < tm.width && ry >= 0 && ry < tm.height) {
                                AutoTileResolve(tm, rx, ry, state.auto_tile_rule);
                            }
                        }
                    }
                }
            }
            if (changed) tm.dirty = true;
        }
        return true;
    }

    if (state.active_tool == TilemapBrushTool::FloodFill) {
        if (!mouse_clicked) return true;
        FloodFillTiles(tm, cx, cy, state.selected_tile_id);
        if (state.tiles_snapshot != tm.tiles) {
            tm.dirty = true;
            changed = true;
        }
    } else {
        int paint_id = (state.active_tool == TilemapBrushTool::Erase) ? 0 : state.selected_tile_id;
        for (int dy = -half_brush; dy <= half_brush; dy++) {
            for (int dx = -half_brush; dx <= half_brush; dx++) {
                int px = cx + dx;
                int py = cy + dy;
                if (px < 0 || px >= tm.width || py < 0 || py >= tm.height) continue;
                int idx = py * tm.width + px;
                if (tm.tiles[idx] != paint_id) {
                    tm.tiles[idx] = paint_id;
                    changed = true;
                }
            }
        }
        if (changed) {
            tm.dirty = true;
            AutoTileResolveNeighbours(tm, cx, cy, state.auto_tile_rule);
        }
    }

    return true;
}

} // namespace dse::editor

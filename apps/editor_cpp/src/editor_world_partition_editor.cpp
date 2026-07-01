/**
 * @file editor_world_partition_editor.cpp
 * @brief World Partition visual editor — cell boundary editing, streaming visualization,
 *        drag-to-resize cells, LOD level assignment, data layer overlay
 */

#include "editor_world_partition_editor.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dse::editor {

namespace {

// ─── Data model ─────────────────────────────────────────────────────────

enum class CellState { Unloaded, Loading, Loaded, Streaming };

struct WorldCell {
    int grid_x = 0;
    int grid_z = 0;
    int size = 1;  // cell size in grid units (1, 2, or 4 for HLOD)
    CellState state = CellState::Unloaded;
    int lod_level = 0;       // 0 = full detail, 1-3 = HLOD levels
    int entity_count = 0;
    float memory_mb = 0.0f;
    bool selected = false;
    std::string label;
    // Data layers
    bool has_gameplay = true;
    bool has_navigation = true;
    bool has_audio = false;
    bool has_landscape = true;
};

struct PartitionGrid {
    static constexpr int kMaxGridSize = 16;
    std::vector<WorldCell> cells;
    int grid_width = 8;
    int grid_height = 8;
    float cell_world_size = 256.0f; // world units per cell
};

enum class DataLayerView { None, LOD, Memory, EntityCount, StreamState, Gameplay, Navigation };

struct WorldPartitionState {
    PartitionGrid grid;
    DataLayerView overlay = DataLayerView::StreamState;
    int selected_cell = -1;
    // View
    float view_offset_x = 0.0f;
    float view_offset_y = 0.0f;
    float view_zoom = 1.0f;
    // Interaction
    bool dragging_view = false;
    bool resizing_cell = false;
    int resize_cell_index = -1;
    // Player position indicator
    float player_x = 3.5f;
    float player_z = 4.2f;
    float streaming_radius = 3.0f;
    // Settings
    bool show_grid_lines = true;
    bool show_labels = true;
    bool show_player = true;
    bool show_streaming_radius = true;
    bool auto_stream_sim = true;

    bool initialized = false;
};

static WorldPartitionState s_state;

void InitWorldPartition() {
    if (s_state.initialized) return;
    s_state.initialized = true;

    auto& grid = s_state.grid;
    // Create default grid of cells
    for (int z = 0; z < grid.grid_height; z++) {
        for (int x = 0; x < grid.grid_width; x++) {
            WorldCell cell;
            cell.grid_x = x;
            cell.grid_z = z;
            cell.entity_count = 10 + (x * 7 + z * 13) % 50;
            cell.memory_mb = 2.0f + static_cast<float>((x * 3 + z * 5) % 20) * 0.5f;
            cell.has_audio = ((x + z) % 3 == 0);
            char label[32];
            snprintf(label, sizeof(label), "Cell_%d_%d", x, z);
            cell.label = label;
            grid.cells.push_back(cell);
        }
    }
}

void UpdateStreamingSimulation() {
    if (!s_state.auto_stream_sim) return;
    auto& grid = s_state.grid;
    float px = s_state.player_x;
    float pz = s_state.player_z;
    float r = s_state.streaming_radius;

    for (auto& cell : grid.cells) {
        float cx = static_cast<float>(cell.grid_x) + 0.5f;
        float cz = static_cast<float>(cell.grid_z) + 0.5f;
        float dist = std::sqrt((cx - px) * (cx - px) + (cz - pz) * (cz - pz));

        if (dist < r * 0.6f) cell.state = CellState::Loaded;
        else if (dist < r) cell.state = CellState::Streaming;
        else if (dist < r * 1.3f) cell.state = CellState::Loading;
        else cell.state = CellState::Unloaded;

        // LOD based on distance
        if (dist < r * 0.5f) cell.lod_level = 0;
        else if (dist < r) cell.lod_level = 1;
        else if (dist < r * 1.5f) cell.lod_level = 2;
        else cell.lod_level = 3;
    }
}

ImU32 GetCellColor(const WorldCell& cell, DataLayerView overlay) {
    switch (overlay) {
        case DataLayerView::StreamState:
            switch (cell.state) {
                case CellState::Loaded: return IM_COL32(50, 150, 50, 180);
                case CellState::Streaming: return IM_COL32(200, 200, 50, 180);
                case CellState::Loading: return IM_COL32(50, 100, 200, 180);
                case CellState::Unloaded: return IM_COL32(60, 60, 60, 180);
            }
            break;
        case DataLayerView::LOD: {
            int lod = cell.lod_level;
            if (lod == 0) return IM_COL32(50, 200, 50, 180);
            if (lod == 1) return IM_COL32(150, 200, 50, 180);
            if (lod == 2) return IM_COL32(200, 150, 50, 180);
            return IM_COL32(200, 50, 50, 180);
        }
        case DataLayerView::Memory: {
            float t = std::min(1.0f, cell.memory_mb / 15.0f);
            return IM_COL32(static_cast<int>(t * 200), static_cast<int>((1 - t) * 200), 50, 180);
        }
        case DataLayerView::EntityCount: {
            float t = std::min(1.0f, cell.entity_count / 50.0f);
            return IM_COL32(50, static_cast<int>((1 - t) * 200), static_cast<int>(t * 200), 180);
        }
        case DataLayerView::Gameplay:
            return cell.has_gameplay ? IM_COL32(80, 180, 80, 180) : IM_COL32(60, 60, 60, 100);
        case DataLayerView::Navigation:
            return cell.has_navigation ? IM_COL32(80, 80, 200, 180) : IM_COL32(60, 60, 60, 100);
        default:
            return IM_COL32(80, 80, 80, 180);
    }
    return IM_COL32(80, 80, 80, 180);
}

} // anonymous namespace

void DrawWorldPartitionEditor(EditorContext& /*ctx*/) {
    InitWorldPartition();
    UpdateStreamingSimulation();
    auto& state = s_state;

    ImGui::Begin(MDI_ICON_GRID "  World Partition Editor");

    // ─── Toolbar ─────────────────────────────────────────────────────────
    {
        const char* overlay_names[] = {"None", "LOD", "Memory", "Entities", "Stream State", "Gameplay", "Navigation"};
        int overlay_i = static_cast<int>(state.overlay);
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("Overlay", &overlay_i, overlay_names, 7)) {
            state.overlay = static_cast<DataLayerView>(overlay_i);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Grid", &state.show_grid_lines);
        ImGui::SameLine();
        ImGui::Checkbox("Labels", &state.show_labels);
        ImGui::SameLine();
        ImGui::Checkbox("Player", &state.show_player);
        ImGui::SameLine();
        ImGui::Checkbox("Stream Radius", &state.show_streaming_radius);
        ImGui::SameLine();
        ImGui::Checkbox("Auto Stream", &state.auto_stream_sim);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::SliderFloat("Radius", &state.streaming_radius, 1.0f, 8.0f);
    }

    ImGui::Separator();

    // ─── Main layout: Left = Grid viewport, Right = Cell properties ──────
    float panel_width = 200.0f;
    ImGui::BeginChild("GridView", ImVec2(-panel_width, 0), ImGuiChildFlags_Borders);

    ImVec2 vp_pos = ImGui::GetCursorScreenPos();
    ImVec2 vp_size = ImGui::GetContentRegionAvail();
    if (vp_size.x < 100) vp_size.x = 100;
    if (vp_size.y < 100) vp_size.y = 100;
    ImGui::InvisibleButton("grid_canvas", vp_size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y),
                      IM_COL32(20, 20, 25, 255));

    // View controls
    if (hovered && ImGui::IsMouseDragging(2)) {
        state.view_offset_x += ImGui::GetIO().MouseDelta.x;
        state.view_offset_y += ImGui::GetIO().MouseDelta.y;
    }
    if (hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.01f) {
        state.view_zoom *= (1.0f + ImGui::GetIO().MouseWheel * 0.1f);
        state.view_zoom = std::max(0.3f, std::min(5.0f, state.view_zoom));
    }

    float cell_px_size = 50.0f * state.view_zoom;
    float grid_ox = vp_pos.x + vp_size.x * 0.5f + state.view_offset_x -
                    state.grid.grid_width * cell_px_size * 0.5f;
    float grid_oy = vp_pos.y + vp_size.y * 0.5f + state.view_offset_y -
                    state.grid.grid_height * cell_px_size * 0.5f;

    // Draw cells
    for (int i = 0; i < static_cast<int>(state.grid.cells.size()); i++) {
        auto& cell = state.grid.cells[i];
        float x0 = grid_ox + cell.grid_x * cell_px_size;
        float y0 = grid_oy + cell.grid_z * cell_px_size;
        float x1 = x0 + cell_px_size * cell.size;
        float y1 = y0 + cell_px_size * cell.size;

        // Cell fill
        ImU32 fill_col = GetCellColor(cell, state.overlay);
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), fill_col);

        // Selection highlight
        if (state.selected_cell == i) {
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(255, 220, 50, 255), 0, 0, 3.0f);
        }

        // Grid lines
        if (state.show_grid_lines) {
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(80, 80, 100, 150));
        }

        // Labels
        if (state.show_labels && cell_px_size > 30) {
            char lbl[32];
            snprintf(lbl, sizeof(lbl), "%d,%d", cell.grid_x, cell.grid_z);
            dl->AddText(ImVec2(x0 + 2, y0 + 2), IM_COL32(180, 180, 200, 200), lbl);
            if (cell_px_size > 45) {
                snprintf(lbl, sizeof(lbl), "E:%d", cell.entity_count);
                dl->AddText(ImVec2(x0 + 2, y0 + 14), IM_COL32(150, 150, 170, 150), lbl);
            }
        }
    }

    // Draw player position
    if (state.show_player) {
        float px = grid_ox + state.player_x * cell_px_size;
        float py = grid_oy + state.player_z * cell_px_size;
        dl->AddCircleFilled(ImVec2(px, py), 6.0f, IM_COL32(255, 100, 100, 255));
        dl->AddCircle(ImVec2(px, py), 6.0f, IM_COL32(255, 255, 255, 200), 0, 2.0f);

        // Streaming radius circle
        if (state.show_streaming_radius) {
            float sr = state.streaming_radius * cell_px_size;
            dl->AddCircle(ImVec2(px, py), sr, IM_COL32(255, 200, 50, 150), 48, 1.5f);
            dl->AddCircle(ImVec2(px, py), sr * 0.6f, IM_COL32(100, 255, 100, 100), 32, 1.0f);
        }
    }

    // Cell selection
    if (hovered && ImGui::IsMouseClicked(0)) {
        ImVec2 mp = ImGui::GetMousePos();
        int gx = static_cast<int>((mp.x - grid_ox) / cell_px_size);
        int gz = static_cast<int>((mp.y - grid_oy) / cell_px_size);
        state.selected_cell = -1;
        for (int i = 0; i < static_cast<int>(state.grid.cells.size()); i++) {
            auto& c = state.grid.cells[i];
            if (c.grid_x == gx && c.grid_z == gz) {
                state.selected_cell = i;
                break;
            }
        }
    }

    // Player dragging (Shift+click)
    if (hovered && ImGui::IsMouseDown(0) && ImGui::GetIO().KeyShift) {
        ImVec2 mp = ImGui::GetMousePos();
        state.player_x = (mp.x - grid_ox) / cell_px_size;
        state.player_z = (mp.y - grid_oy) / cell_px_size;
        state.player_x = std::max(0.0f, std::min(static_cast<float>(state.grid.grid_width), state.player_x));
        state.player_z = std::max(0.0f, std::min(static_cast<float>(state.grid.grid_height), state.player_z));
    }

    // Legend
    {
        float lx = vp_pos.x + 8;
        float ly = vp_pos.y + vp_size.y - 70;
        dl->AddRectFilled(ImVec2(lx, ly), ImVec2(lx + 130, ly + 65), IM_COL32(30, 30, 40, 220));
        dl->AddText(ImVec2(lx + 4, ly + 2), IM_COL32(200, 200, 200, 255), "Legend:");
        if (state.overlay == DataLayerView::StreamState) {
            dl->AddRectFilled(ImVec2(lx + 4, ly + 16), ImVec2(lx + 14, ly + 24), IM_COL32(50, 150, 50, 255));
            dl->AddText(ImVec2(lx + 18, ly + 14), IM_COL32(180, 180, 180, 255), "Loaded");
            dl->AddRectFilled(ImVec2(lx + 4, ly + 28), ImVec2(lx + 14, ly + 36), IM_COL32(200, 200, 50, 255));
            dl->AddText(ImVec2(lx + 18, ly + 26), IM_COL32(180, 180, 180, 255), "Streaming");
            dl->AddRectFilled(ImVec2(lx + 4, ly + 40), ImVec2(lx + 14, ly + 48), IM_COL32(50, 100, 200, 255));
            dl->AddText(ImVec2(lx + 18, ly + 38), IM_COL32(180, 180, 180, 255), "Loading");
            dl->AddRectFilled(ImVec2(lx + 4, ly + 52), ImVec2(lx + 14, ly + 60), IM_COL32(60, 60, 60, 255));
            dl->AddText(ImVec2(lx + 18, ly + 50), IM_COL32(180, 180, 180, 255), "Unloaded");
        }
    }

    ImGui::EndChild(); // GridView

    ImGui::SameLine();

    // ─── Cell Properties Panel ───────────────────────────────────────────
    ImGui::BeginChild("CellProps", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::Text(MDI_ICON_INFORMATION "  Cell Properties");
    ImGui::Separator();

    if (state.selected_cell >= 0 && state.selected_cell < static_cast<int>(state.grid.cells.size())) {
        auto& cell = state.grid.cells[state.selected_cell];
        ImGui::Text("Cell: %s", cell.label.c_str());
        ImGui::Text("Grid: (%d, %d)", cell.grid_x, cell.grid_z);
        ImGui::Text("World: (%.0f, %.0f)", cell.grid_x * state.grid.cell_world_size,
                    cell.grid_z * state.grid.cell_world_size);
        ImGui::Separator();

        const char* state_labels[] = {"Unloaded", "Loading", "Loaded", "Streaming"};
        ImGui::Text("State: %s", state_labels[static_cast<int>(cell.state)]);
        ImGui::Text("LOD: %d", cell.lod_level);
        ImGui::Text("Entities: %d", cell.entity_count);
        ImGui::Text("Memory: %.1f MB", cell.memory_mb);
        ImGui::Separator();

        ImGui::Text("Data Layers:");
        ImGui::Checkbox("Gameplay", &cell.has_gameplay);
        ImGui::Checkbox("Navigation", &cell.has_navigation);
        ImGui::Checkbox("Audio", &cell.has_audio);
        ImGui::Checkbox("Landscape", &cell.has_landscape);
        ImGui::Separator();

        ImGui::SetNextItemWidth(60);
        ImGui::InputInt("Size", &cell.size);
        cell.size = std::max(1, std::min(4, cell.size));
        ImGui::SetNextItemWidth(80);
        ImGui::InputFloat("World Size", &state.grid.cell_world_size, 64.0f);
    } else {
        ImGui::TextDisabled("Select a cell to view properties.");
        ImGui::TextDisabled("Shift+Click to move player.");
    }

    ImGui::Separator();
    ImGui::Text("Grid Stats:");
    int loaded = 0, streaming = 0;
    float total_mem = 0;
    int total_entities = 0;
    for (auto& c : state.grid.cells) {
        if (c.state == CellState::Loaded) loaded++;
        if (c.state == CellState::Streaming) streaming++;
        total_mem += c.memory_mb;
        total_entities += c.entity_count;
    }
    ImGui::Text("Cells: %d", static_cast<int>(state.grid.cells.size()));
    ImGui::Text("Loaded: %d", loaded);
    ImGui::Text("Streaming: %d", streaming);
    ImGui::Text("Total Mem: %.1f MB", total_mem);
    ImGui::Text("Total Entities: %d", total_entities);

    ImGui::EndChild(); // CellProps

    ImGui::End();
}

} // namespace dse::editor

/**
 * @file editor_terrain_tools.cpp
 * @brief 编辑器地形/植被/道路编辑面板
 *
 * 调用引擎层 WorldEditorTools 接口，不重复实现逻辑。
 * 编辑器只负责 UI 呈现 + 用户交互 → 调用引擎 API。
 */

#include "editor_terrain_tools.h"
#include "editor_context.h"

#include "engine/terrain/world_editor_tools.h"
#include "engine/terrain/spline_system.h"
#include "imgui.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <string>
#include <cmath>

namespace dse::editor {

namespace {

// ─── Editor-side state for terrain tools ─────────────────────────────────────

struct TerrainToolsState {
    // Shared engine systems (non-owning — lifetime managed by engine)
    terrain::WorldEditorTools* tools = nullptr;
    terrain::SplineSystem* spline = nullptr;

    // Lazy-init editor-owned instances (for standalone editor testing)
    std::unique_ptr<terrain::WorldEditorTools> owned_tools;
    std::unique_ptr<terrain::SplineSystem> owned_spline;

    // Active tool tab
    enum class Tab { Terrain = 0, Foliage, Road, Partition };
    Tab active_tab = Tab::Terrain;

    // Terrain brush params
    terrain::TerrainBrushOp brush_op = terrain::TerrainBrushOp::RaiseHeight;
    terrain::BrushParams brush_params;

    // Foliage brush params
    terrain::FoliageBrushParams foliage_params;

    // Road draw state
    bool road_drawing = false;
    uint32_t road_session = 0;
    float road_width = 6.0f;

    // Partition vis
    float partition_cell_size = 256.0f;

    // Stats
    uint32_t last_brush_op_id = 0;
    uint32_t last_foliage_placed = 0;

    bool initialized = false;
};

static TerrainToolsState s_state;

void EnsureInitialized() {
    if (s_state.initialized) return;

    if (!s_state.tools) {
        s_state.owned_tools = std::make_unique<terrain::WorldEditorTools>();
        s_state.owned_tools->Init();
        s_state.tools = s_state.owned_tools.get();
    }
    if (!s_state.spline) {
        s_state.owned_spline = std::make_unique<terrain::SplineSystem>();
        s_state.spline = s_state.owned_spline.get();
    }

    s_state.initialized = true;
}

// ─── Tab: Terrain Brush ──────────────────────────────────────────────────────

void DrawTerrainBrushTab() {
    ImGui::Text("Terrain Brush");
    ImGui::Separator();

    // Op selection
    const char* op_names[] = {
        "Raise", "Lower", "Smooth", "Flatten",
        "Erode (Thermal)", "Erode (Hydraulic)", "Paint Splat", "Paint Hole"
    };
    int op_idx = static_cast<int>(s_state.brush_op);
    if (ImGui::Combo("Operation", &op_idx, op_names, 8)) {
        s_state.brush_op = static_cast<terrain::TerrainBrushOp>(op_idx);
    }

    // Brush shape
    const char* shape_names[] = { "Circle", "Square", "Custom" };
    int shape_idx = static_cast<int>(s_state.brush_params.shape);
    if (ImGui::Combo("Shape", &shape_idx, shape_names, 3)) {
        s_state.brush_params.shape = static_cast<terrain::BrushShape>(shape_idx);
    }

    ImGui::SliderFloat("Radius", &s_state.brush_params.radius, 1.0f, 200.0f);
    ImGui::SliderFloat("Strength", &s_state.brush_params.strength, 0.01f, 1.0f);
    ImGui::SliderFloat("Falloff", &s_state.brush_params.falloff, 0.0f, 1.0f);

    if (s_state.brush_op == terrain::TerrainBrushOp::FlattenHeight) {
        ImGui::InputFloat("Target Height", &s_state.brush_params.target_height);
    }
    if (s_state.brush_op == terrain::TerrainBrushOp::PaintSplat) {
        int layer = static_cast<int>(s_state.brush_params.splat_layer);
        ImGui::InputInt("Splat Layer", &layer);
        s_state.brush_params.splat_layer = static_cast<uint32_t>(std::max(0, layer));
    }

    ImGui::Spacing();

    // Apply button (normally triggered by mouse drag in viewport)
    if (ImGui::Button("Apply Brush")) {
        s_state.last_brush_op_id = s_state.tools->ApplyTerrainBrush(
            s_state.brush_op, s_state.brush_params);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Undo/Redo
    ImGui::BeginDisabled(s_state.tools->GetUndoCount() == 0);
    if (ImGui::Button("Undo")) { s_state.tools->Undo(); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(s_state.tools->GetRedoCount() == 0);
    if (ImGui::Button("Redo")) { s_state.tools->Redo(); }
    ImGui::EndDisabled();

    ImGui::Text("Undo stack: %u  Redo stack: %u",
                s_state.tools->GetUndoCount(), s_state.tools->GetRedoCount());
}

// ─── Tab: Foliage Brush ──────────────────────────────────────────────────────

void DrawFoliageBrushTab() {
    ImGui::Text("Foliage Brush");
    ImGui::Separator();

    // Mode selection
    const char* mode_names[] = { "Single", "Scatter", "Erase", "Align" };
    int mode_idx = static_cast<int>(s_state.foliage_params.mode);
    if (ImGui::Combo("Mode", &mode_idx, mode_names, 4)) {
        s_state.foliage_params.mode = static_cast<terrain::FoliagePlaceMode>(mode_idx);
    }

    ImGui::SliderFloat("Radius", &s_state.foliage_params.radius, 1.0f, 100.0f);
    ImGui::SliderFloat("Density", &s_state.foliage_params.density, 0.01f, 5.0f);
    ImGui::SliderFloat("Min Scale", &s_state.foliage_params.min_scale, 0.1f, 2.0f);
    ImGui::SliderFloat("Max Scale", &s_state.foliage_params.max_scale, 0.1f, 3.0f);
    ImGui::SliderFloat("Random Rotation", &s_state.foliage_params.random_rotation, 0.0f, 360.0f);
    ImGui::SliderFloat("Slope Limit", &s_state.foliage_params.slope_limit, 0.0f, 90.0f);
    ImGui::Checkbox("Align to Normal", &s_state.foliage_params.align_to_normal);

    // Mesh selection (simplified — real version would use asset browser)
    static char mesh_buf[256] = "meshes/tree_default.dmesh";
    ImGui::InputText("Mesh", mesh_buf, sizeof(mesh_buf));
    s_state.foliage_params.mesh_path = mesh_buf;

    ImGui::Spacing();

    if (s_state.foliage_params.mode == terrain::FoliagePlaceMode::Erase) {
        if (ImGui::Button("Erase in Radius")) {
            s_state.last_foliage_placed = s_state.tools->EraseFoliage(
                s_state.foliage_params.center, s_state.foliage_params.radius);
        }
    } else {
        if (ImGui::Button("Place Foliage")) {
            s_state.last_foliage_placed = s_state.tools->PlaceFoliage(s_state.foliage_params);
        }
    }

    ImGui::Spacing();
    ImGui::Text("Total instances: %u", s_state.tools->GetFoliageCount());
    ImGui::Text("Last action: %u instances", s_state.last_foliage_placed);
}

// ─── Tab: Road Drawing ───────────────────────────────────────────────────────

void DrawRoadTab() {
    ImGui::Text("Road Drawing");
    ImGui::Separator();

    ImGui::SliderFloat("Road Width", &s_state.road_width, 2.0f, 30.0f);

    if (!s_state.road_drawing) {
        if (ImGui::Button("Start Road")) {
            s_state.road_session = s_state.tools->BeginRoadDraw(s_state.road_width);
            s_state.road_drawing = true;
        }
    } else {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Drawing road (session %u)...",
                           s_state.road_session);
        ImGui::Text("Click in viewport to add control points");

        if (ImGui::Button("Finish Road")) {
            s_state.tools->EndRoadDraw(s_state.road_session);
            s_state.road_drawing = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            s_state.tools->CancelRoadDraw(s_state.road_session);
            s_state.road_drawing = false;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Spline System");
    if (s_state.spline) {
        ImGui::Text("Active splines: %u", s_state.spline->GetSplineCount());
    }
}

// ─── Tab: World Partition Visualization ──────────────────────────────────────

void DrawPartitionTab() {
    ImGui::Text("World Partition");
    ImGui::Separator();

    ImGui::SliderFloat("Cell Size", &s_state.partition_cell_size, 64.0f, 1024.0f);

    if (ImGui::Button("Update Visualization")) {
        // Use origin as camera pos — real version uses editor camera
        s_state.tools->UpdatePartitionVisualization(glm::vec3(0), s_state.partition_cell_size);
    }

    ImGui::Spacing();
    ImGui::Text("Visible cells: %u", s_state.tools->GetVisibleCellCount());

    // Cell list
    const auto& cells = s_state.tools->GetCellStates();
    if (!cells.empty() && ImGui::TreeNode("Cell Details")) {
        for (size_t i = 0; i < cells.size() && i < 20; ++i) {
            const auto& c = cells[i];
            ImGui::Text("[%d,%d] %s LOD=%d dist=%.1f",
                        c.cell_x, c.cell_y,
                        c.loaded ? "LOADED" : "unloaded",
                        c.lod_level, c.distance_to_camera);
        }
        if (cells.size() > 20) {
            ImGui::Text("... and %zu more", cells.size() - 20);
        }
        ImGui::TreePop();
    }
}

} // anonymous namespace

// ─── Public API ──────────────────────────────────────────────────────────────

void DrawTerrainToolsPanel(EditorContext& ctx) {
    (void)ctx; // Available for future engine integration
    EnsureInitialized();

    if (!ImGui::Begin("Terrain Tools")) {
        ImGui::End();
        return;
    }

    // Tab bar
    if (ImGui::BeginTabBar("TerrainToolsTabs")) {
        if (ImGui::BeginTabItem("Terrain")) {
            s_state.active_tab = TerrainToolsState::Tab::Terrain;
            DrawTerrainBrushTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Foliage")) {
            s_state.active_tab = TerrainToolsState::Tab::Foliage;
            DrawFoliageBrushTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Road")) {
            s_state.active_tab = TerrainToolsState::Tab::Road;
            DrawRoadTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Partition")) {
            s_state.active_tab = TerrainToolsState::Tab::Partition;
            DrawPartitionTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void DrawTerrainToolsOverlay(EditorContext& ctx,
                              const glm::vec2& window_pos,
                              const glm::vec2& panel_size,
                              const glm::mat4& view,
                              const glm::mat4& proj) {
    (void)ctx; (void)window_pos; (void)panel_size; (void)view; (void)proj;

    if (!s_state.initialized || !s_state.tools) return;

    // Get brush preview AABB for current params
    auto preview = s_state.tools->GetBrushPreview(s_state.brush_params);
    (void)preview;

    // Real implementation would project preview AABB to screen coords
    // and draw a circle/rectangle gizmo at the brush location.
    // This requires integration with the editor's viewport hit-testing
    // (raycast from mouse → terrain intersection → set brush_params.center).
}

} // namespace dse::editor

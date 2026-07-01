/**
 * @file editor_terrain_sculpt_preview.cpp
 * @brief Terrain sculpt real-time brush preview — height/weight painting with 3D visualization
 */

#include "editor_terrain_sculpt_preview.h"
#include "editor_icons.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace dse::editor {

namespace {

// ─── Brush types ────────────────────────────────────────────────────────

enum class BrushMode { Raise, Lower, Smooth, Flatten, Paint, Erosion, Noise };
enum class BrushFalloff { Linear, Smooth, Sphere, Tip, Custom };

struct BrushSettings {
    BrushMode mode = BrushMode::Raise;
    BrushFalloff falloff = BrushFalloff::Smooth;
    float radius = 5.0f;
    float strength = 0.5f;
    float opacity = 1.0f;
    float flow = 1.0f;
    float target_height = 0.0f;  // for flatten mode
    int paint_layer = 0;         // for paint mode
    float noise_scale = 1.0f;    // for noise mode
    float erosion_strength = 0.3f;
    // Brush shape preview params
    float rotation = 0.0f;
    float squish = 1.0f;         // 1.0 = circle, <1 = ellipse
    bool mirror_x = false;
    bool mirror_z = false;
};

struct TerrainPreviewState {
    BrushSettings brush;
    // Heightmap preview (small grid for visualization)
    static constexpr int kGridSize = 32;
    float heightmap[kGridSize][kGridSize] = {};
    float weightmap[4][kGridSize][kGridSize] = {};  // 4 paint layers

    // 3D preview camera
    float cam_angle_y = 0.6f;
    float cam_angle_x = 0.4f;
    float cam_distance = 8.0f;

    // Brush position (on heightmap grid)
    float brush_pos_x = 16.0f;
    float brush_pos_z = 16.0f;
    bool brush_active = false;

    // Paint layer names/colors
    const char* layer_names[4] = {"Grass", "Dirt", "Rock", "Sand"};
    ImU32 layer_colors[4] = {
        IM_COL32(80, 160, 80, 255),
        IM_COL32(140, 100, 50, 255),
        IM_COL32(120, 120, 130, 255),
        IM_COL32(200, 180, 100, 255)
    };

    // Undo
    float undo_heightmap[kGridSize][kGridSize] = {};
    bool has_undo = false;

    bool initialized = false;
};

static TerrainPreviewState s_state;

void InitTerrainPreview() {
    if (s_state.initialized) return;
    s_state.initialized = true;

    // Generate initial terrain (hills)
    for (int z = 0; z < TerrainPreviewState::kGridSize; z++) {
        for (int x = 0; x < TerrainPreviewState::kGridSize; x++) {
            float fx = static_cast<float>(x) / TerrainPreviewState::kGridSize * 6.28f;
            float fz = static_cast<float>(z) / TerrainPreviewState::kGridSize * 6.28f;
            s_state.heightmap[z][x] = std::sin(fx) * std::cos(fz * 0.7f) * 0.5f +
                                       std::sin(fx * 2.3f + 1.0f) * 0.2f;
        }
    }
    // Default weight: all grass
    for (int z = 0; z < TerrainPreviewState::kGridSize; z++)
        for (int x = 0; x < TerrainPreviewState::kGridSize; x++)
            s_state.weightmap[0][z][x] = 1.0f;
}

float BrushFalloffValue(float dist, float radius, BrushFalloff falloff) {
    float t = dist / radius;
    if (t >= 1.0f) return 0.0f;
    switch (falloff) {
        case BrushFalloff::Linear: return 1.0f - t;
        case BrushFalloff::Smooth: return (1.0f - t * t) * (1.0f - t * t); // bilinear
        case BrushFalloff::Sphere: return std::sqrt(1.0f - t * t);
        case BrushFalloff::Tip: return t < 0.3f ? 1.0f : (1.0f - (t - 0.3f) / 0.7f);
        case BrushFalloff::Custom: return std::cos(t * 3.14159f * 0.5f);
        default: return 1.0f - t;
    }
}

void ApplyBrush(float dt) {
    if (!s_state.brush_active) return;
    auto& brush = s_state.brush;
    float grid_radius = brush.radius;

    int cx = static_cast<int>(s_state.brush_pos_x);
    int cz = static_cast<int>(s_state.brush_pos_z);
    int r = static_cast<int>(std::ceil(grid_radius));

    for (int dz = -r; dz <= r; dz++) {
        for (int dx = -r; dx <= r; dx++) {
            int gx = cx + dx;
            int gz = cz + dz;
            if (gx < 0 || gx >= TerrainPreviewState::kGridSize) continue;
            if (gz < 0 || gz >= TerrainPreviewState::kGridSize) continue;

            float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
            float falloff_val = BrushFalloffValue(dist, grid_radius, brush.falloff);
            float effect = falloff_val * brush.strength * brush.opacity * dt;

            switch (brush.mode) {
                case BrushMode::Raise:
                    s_state.heightmap[gz][gx] += effect;
                    break;
                case BrushMode::Lower:
                    s_state.heightmap[gz][gx] -= effect;
                    break;
                case BrushMode::Smooth: {
                    float avg = 0; int cnt = 0;
                    for (int sy = -1; sy <= 1; sy++) for (int sx = -1; sx <= 1; sx++) {
                        int nx = gx + sx, nz = gz + sy;
                        if (nx >= 0 && nx < TerrainPreviewState::kGridSize && nz >= 0 && nz < TerrainPreviewState::kGridSize) {
                            avg += s_state.heightmap[nz][nx]; cnt++;
                        }
                    }
                    avg /= static_cast<float>(cnt);
                    s_state.heightmap[gz][gx] += (avg - s_state.heightmap[gz][gx]) * effect * 2.0f;
                    break;
                }
                case BrushMode::Flatten:
                    s_state.heightmap[gz][gx] += (brush.target_height - s_state.heightmap[gz][gx]) * effect * 2.0f;
                    break;
                case BrushMode::Paint: {
                    int layer = brush.paint_layer;
                    if (layer >= 0 && layer < 4) {
                        s_state.weightmap[layer][gz][gx] = std::min(1.0f, s_state.weightmap[layer][gz][gx] + effect);
                        // Normalize weights
                        float total = 0;
                        for (int l = 0; l < 4; l++) total += s_state.weightmap[l][gz][gx];
                        if (total > 0) for (int l = 0; l < 4; l++) s_state.weightmap[l][gz][gx] /= total;
                    }
                    break;
                }
                case BrushMode::Noise:
                    s_state.heightmap[gz][gx] += std::sin(gx * brush.noise_scale + gz * 1.7f) * effect * 0.5f;
                    break;
                case BrushMode::Erosion: {
                    // Simple thermal erosion: move height downhill
                    float h = s_state.heightmap[gz][gx];
                    float min_h = h;
                    for (int sy = -1; sy <= 1; sy++) for (int sx = -1; sx <= 1; sx++) {
                        int nx = gx + sx, nz = gz + sy;
                        if (nx >= 0 && nx < TerrainPreviewState::kGridSize && nz >= 0 && nz < TerrainPreviewState::kGridSize) {
                            min_h = std::min(min_h, s_state.heightmap[nz][nx]);
                        }
                    }
                    if (h - min_h > 0.01f) {
                        s_state.heightmap[gz][gx] -= (h - min_h) * effect * brush.erosion_strength;
                    }
                    break;
                }
            }
        }
    }
}

// Project 3D terrain point to 2D for the preview viewport
ImVec2 ProjectPoint(float x, float y, float z, ImVec2 center, float scale) {
    float cos_ay = std::cos(s_state.cam_angle_y), sin_ay = std::sin(s_state.cam_angle_y);
    float cos_ax = std::cos(s_state.cam_angle_x), sin_ax = std::sin(s_state.cam_angle_x);
    // Rotate around Y
    float rx = x * cos_ay + z * sin_ay;
    float rz = -x * sin_ay + z * cos_ay;
    // Rotate around X
    float ry = y * cos_ax - rz * sin_ax;
    float rz2 = y * sin_ax + rz * cos_ax;
    (void)rz2;
    // Simple orthographic projection
    return ImVec2(center.x + rx * scale, center.y - ry * scale);
}

} // anonymous namespace

void DrawTerrainSculptPreview(EditorContext& /*ctx*/) {
    InitTerrainPreview();
    auto& state = s_state;

    ImGui::Begin(MDI_ICON_TERRAIN "  Terrain Sculpt Preview");

    // ─── Brush Settings ──────────────────────────────────────────────────
    ImGui::BeginChild("BrushSettings", ImVec2(220, 0), ImGuiChildFlags_Borders);
    ImGui::Text(MDI_ICON_BRUSH "  Brush Settings");
    ImGui::Separator();

    const char* mode_names[] = {"Raise", "Lower", "Smooth", "Flatten", "Paint", "Erosion", "Noise"};
    int mode_i = static_cast<int>(state.brush.mode);
    if (ImGui::Combo("Mode", &mode_i, mode_names, 7)) {
        state.brush.mode = static_cast<BrushMode>(mode_i);
    }

    const char* falloff_names[] = {"Linear", "Smooth", "Sphere", "Tip", "Custom"};
    int falloff_i = static_cast<int>(state.brush.falloff);
    if (ImGui::Combo("Falloff", &falloff_i, falloff_names, 5)) {
        state.brush.falloff = static_cast<BrushFalloff>(falloff_i);
    }

    ImGui::SliderFloat("Radius", &state.brush.radius, 1.0f, 15.0f);
    ImGui::SliderFloat("Strength", &state.brush.strength, 0.01f, 2.0f);
    ImGui::SliderFloat("Opacity", &state.brush.opacity, 0.0f, 1.0f);
    ImGui::SliderFloat("Flow", &state.brush.flow, 0.0f, 1.0f);

    if (state.brush.mode == BrushMode::Flatten) {
        ImGui::SliderFloat("Target H", &state.brush.target_height, -2.0f, 2.0f);
    }
    if (state.brush.mode == BrushMode::Paint) {
        ImGui::Combo("Layer", &state.brush.paint_layer, state.layer_names, 4);
    }
    if (state.brush.mode == BrushMode::Noise) {
        ImGui::SliderFloat("Noise Scale", &state.brush.noise_scale, 0.1f, 5.0f);
    }
    if (state.brush.mode == BrushMode::Erosion) {
        ImGui::SliderFloat("Erosion Str", &state.brush.erosion_strength, 0.01f, 1.0f);
    }

    ImGui::Separator();
    ImGui::Text("Brush Shape");
    ImGui::SliderFloat("Rotation", &state.brush.rotation, 0.0f, 360.0f);
    ImGui::SliderFloat("Squish", &state.brush.squish, 0.3f, 1.0f);
    ImGui::Checkbox("Mirror X", &state.brush.mirror_x);
    ImGui::SameLine();
    ImGui::Checkbox("Mirror Z", &state.brush.mirror_z);

    ImGui::Separator();

    // ─── Falloff curve preview ───────────────────────────────────────────
    ImGui::Text("Falloff Curve:");
    {
        ImVec2 curve_pos = ImGui::GetCursorScreenPos();
        ImVec2 curve_size(200, 60);
        ImGui::InvisibleButton("falloff_curve", curve_size);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        cdl->AddRectFilled(curve_pos, ImVec2(curve_pos.x + curve_size.x, curve_pos.y + curve_size.y),
                           IM_COL32(30, 30, 40, 255));
        ImVec2 prev;
        for (int i = 0; i <= 40; i++) {
            float t = static_cast<float>(i) / 40.0f;
            float v = BrushFalloffValue(t * state.brush.radius, state.brush.radius, state.brush.falloff);
            ImVec2 pt(curve_pos.x + t * curve_size.x,
                      curve_pos.y + curve_size.y - v * curve_size.y);
            if (i > 0) cdl->AddLine(prev, pt, IM_COL32(100, 200, 100, 255), 2.0f);
            prev = pt;
        }
    }

    ImGui::Separator();

    // Undo/actions
    if (ImGui::Button("Undo")) {
        if (state.has_undo) {
            std::memcpy(state.heightmap, state.undo_heightmap, sizeof(state.heightmap));
            state.has_undo = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Terrain")) {
        for (int z = 0; z < TerrainPreviewState::kGridSize; z++)
            for (int x = 0; x < TerrainPreviewState::kGridSize; x++)
                state.heightmap[z][x] = 0;
    }

    ImGui::EndChild(); // BrushSettings

    ImGui::SameLine();

    // ─── 3D Terrain Preview Viewport ─────────────────────────────────────
    ImGui::BeginChild("TerrainViewport", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::Text("3D Preview (drag to rotate, scroll to zoom)");

    ImVec2 vp_pos = ImGui::GetCursorScreenPos();
    ImVec2 vp_size = ImGui::GetContentRegionAvail();
    if (vp_size.x < 100) vp_size.x = 100;
    if (vp_size.y < 100) vp_size.y = 100;
    ImGui::InvisibleButton("terrain_vp", vp_size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool vp_hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(vp_pos, ImVec2(vp_pos.x + vp_size.x, vp_pos.y + vp_size.y),
                      IM_COL32(20, 25, 30, 255));

    // Camera control
    if (vp_hovered && ImGui::IsMouseDragging(2)) {
        state.cam_angle_y += ImGui::GetIO().MouseDelta.x * 0.01f;
        state.cam_angle_x += ImGui::GetIO().MouseDelta.y * 0.01f;
        state.cam_angle_x = std::max(-1.2f, std::min(1.2f, state.cam_angle_x));
    }
    if (vp_hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.01f) {
        state.cam_distance -= ImGui::GetIO().MouseWheel * 0.5f;
        state.cam_distance = std::max(3.0f, std::min(20.0f, state.cam_distance));
    }

    // Brush painting (left click)
    if (vp_hovered && ImGui::IsMouseDown(0)) {
        if (!state.brush_active) {
            // Save undo state
            std::memcpy(state.undo_heightmap, state.heightmap, sizeof(state.heightmap));
            state.has_undo = true;
        }
        state.brush_active = true;
        // Map mouse to terrain coords (approximate)
        ImVec2 mp = ImGui::GetMousePos();
        float nx = (mp.x - vp_pos.x) / vp_size.x * TerrainPreviewState::kGridSize;
        float nz = (mp.y - vp_pos.y) / vp_size.y * TerrainPreviewState::kGridSize;
        state.brush_pos_x = std::max(0.0f, std::min(static_cast<float>(TerrainPreviewState::kGridSize - 1), nx));
        state.brush_pos_z = std::max(0.0f, std::min(static_cast<float>(TerrainPreviewState::kGridSize - 1), nz));
        ApplyBrush(ImGui::GetIO().DeltaTime * 2.0f);
    } else {
        state.brush_active = false;
    }

    // Draw terrain wireframe
    ImVec2 center(vp_pos.x + vp_size.x * 0.5f, vp_pos.y + vp_size.y * 0.5f);
    float scale = vp_size.x / state.cam_distance;
    int grid = TerrainPreviewState::kGridSize;
    int step = std::max(1, grid / 16);  // reduce resolution for wireframe

    for (int z = 0; z < grid - step; z += step) {
        for (int x = 0; x < grid - step; x += step) {
            float fx = (static_cast<float>(x) / grid - 0.5f) * 4.0f;
            float fz = (static_cast<float>(z) / grid - 0.5f) * 4.0f;
            float fx1 = (static_cast<float>(x + step) / grid - 0.5f) * 4.0f;
            float fz1 = (static_cast<float>(z + step) / grid - 0.5f) * 4.0f;

            float h00 = state.heightmap[z][x];
            float h10 = state.heightmap[z][std::min(x + step, grid - 1)];
            float h01 = state.heightmap[std::min(z + step, grid - 1)][x];

            ImVec2 p00 = ProjectPoint(fx, h00, fz, center, scale);
            ImVec2 p10 = ProjectPoint(fx1, h10, fz, center, scale);
            ImVec2 p01 = ProjectPoint(fx, h01, fz1, center, scale);

            // Color based on height/weight
            float normalized_h = (h00 + 1.0f) * 0.5f;
            int layer = 0;
            float max_w = 0;
            for (int l = 0; l < 4; l++) {
                if (state.weightmap[l][z][x] > max_w) { max_w = state.weightmap[l][z][x]; layer = l; }
            }
            ImU32 wire_col = state.layer_colors[layer];
            // Darken based on height
            int r_c = ((wire_col >> 0) & 0xFF);
            int g_c = ((wire_col >> 8) & 0xFF);
            int b_c = ((wire_col >> 16) & 0xFF);
            float brightness = 0.4f + normalized_h * 0.6f;
            wire_col = IM_COL32(static_cast<int>(r_c * brightness), static_cast<int>(g_c * brightness),
                                static_cast<int>(b_c * brightness), 180);

            dl->AddLine(p00, p10, wire_col, 1.0f);
            dl->AddLine(p00, p01, wire_col, 1.0f);
        }
    }

    // Draw brush indicator (circle on terrain)
    if (vp_hovered) {
        ImVec2 mp = ImGui::GetMousePos();
        float brush_screen_r = state.brush.radius / grid * vp_size.x * 0.5f;
        dl->AddCircle(mp, brush_screen_r, IM_COL32(255, 200, 50, 200), 32, 2.0f);
        dl->AddCircle(mp, brush_screen_r * 0.3f, IM_COL32(255, 255, 100, 150), 16, 1.0f);

        // Brush center dot
        dl->AddCircleFilled(mp, 3.0f, IM_COL32(255, 255, 255, 255));

        // Show brush info
        char info[64];
        snprintf(info, sizeof(info), "%s R:%.1f S:%.2f",
                 mode_names[static_cast<int>(state.brush.mode)],
                 state.brush.radius, state.brush.strength);
        dl->AddText(ImVec2(mp.x + 10, mp.y + 10), IM_COL32(255, 255, 200, 200), info);
    }

    ImGui::EndChild(); // TerrainViewport

    ImGui::End();
}

// ─── Test accessors ─────────────────────────────────────────────────────
static TerrainSculptTestState s_test_state;

TerrainSculptTestState& GetTerrainSculptState() {
    InitTerrainPreview();
    s_test_state.brush.mode = static_cast<TerrainSculptBrushMode>(static_cast<int>(s_state.brush.mode));
    s_test_state.brush.radius = s_state.brush.radius;
    s_test_state.brush.strength = s_state.brush.strength;
    s_test_state.brush.opacity = s_state.brush.opacity;
    s_test_state.heightmap_size = TerrainPreviewState::kGridSize;
    s_test_state.heightmap.resize(
        static_cast<size_t>(TerrainPreviewState::kGridSize * TerrainPreviewState::kGridSize));
    for (int z = 0; z < TerrainPreviewState::kGridSize; z++)
        for (int x = 0; x < TerrainPreviewState::kGridSize; x++)
            s_test_state.heightmap[z * TerrainPreviewState::kGridSize + x] = s_state.heightmap[z][x];
    return s_test_state;
}

} // namespace dse::editor

/**
 * @file editor_curve_editor.cpp
 * @brief 通用曲线编辑器 — 支持关键帧拖拽、三次 Hermite 插值、缩放平移
 */

#include "editor_curve_editor.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dse::editor {

// ─── EditorCurve impl ─────────────────────────────────────────────────────

void EditorCurve::SortKeys() {
    std::sort(keys.begin(), keys.end(),
              [](const CurveKey& a, const CurveKey& b) { return a.time < b.time; });
}

float EditorCurve::Evaluate(float t) const {
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1 || t <= keys.front().time) return keys.front().value;
    if (t >= keys.back().time) return keys.back().value;

    // Find segment
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        if (t >= keys[i].time && t <= keys[i + 1].time) {
            float dt = keys[i + 1].time - keys[i].time;
            if (dt < 1e-6f) return keys[i].value;
            float s = (t - keys[i].time) / dt;

            if (interp == CurveInterp::Linear) {
                return keys[i].value + (keys[i + 1].value - keys[i].value) * s;
            }

            // Cubic Hermite
            float s2 = s * s;
            float s3 = s2 * s;
            float h00 = 2 * s3 - 3 * s2 + 1;
            float h10 = s3 - 2 * s2 + s;
            float h01 = -2 * s3 + 3 * s2;
            float h11 = s3 - s2;
            float m0 = keys[i].out_tangent * dt;
            float m1 = keys[i + 1].in_tangent * dt;
            return h00 * keys[i].value + h10 * m0 + h01 * keys[i + 1].value + h11 * m1;
        }
    }
    return keys.back().value;
}

EditorCurve MakeDefaultCurve(const char* name, float start_val, float end_val) {
    EditorCurve c;
    c.name = name;
    c.keys.push_back({0.0f, start_val, 0.0f, 0.0f});
    c.keys.push_back({1.0f, end_val, 0.0f, 0.0f});
    return c;
}

// ─── Helpers ──────────────────────────────────────────────────────────────

namespace {

ImVec2 CurveToScreen(float t, float v, const ImVec2& origin, const ImVec2& sz,
                      float tmin, float tmax, float vmin, float vmax) {
    float nx = (t - tmin) / (tmax - tmin);
    float ny = 1.0f - (v - vmin) / (vmax - vmin);
    return ImVec2(origin.x + nx * sz.x, origin.y + ny * sz.y);
}

ImVec2 ScreenToCurve(const ImVec2& pos, const ImVec2& origin, const ImVec2& sz,
                      float tmin, float tmax, float vmin, float vmax) {
    float nx = (pos.x - origin.x) / sz.x;
    float ny = (pos.y - origin.y) / sz.y;
    float t = tmin + nx * (tmax - tmin);
    float v = vmax - ny * (vmax - vmin);
    return ImVec2(t, v);
}

float SnapValue(float val, float grid) {
    return std::round(val / grid) * grid;
}

} // anonymous namespace

// ─── Main widget ──────────────────────────────────────────────────────────

bool DrawCurveEditor(const char* label, CurveEditorState& state, const ImVec2& size) {
    bool modified = false;

    ImGui::PushID(label);

    // Curve selector
    if (!state.curves.empty()) {
        if (ImGui::BeginCombo("##curve_sel",
                state.selected_curve >= 0 && state.selected_curve < static_cast<int>(state.curves.size())
                    ? state.curves[state.selected_curve].name.c_str() : "—")) {
            for (int i = 0; i < static_cast<int>(state.curves.size()); ++i) {
                bool sel = (i == state.selected_curve);
                if (ImGui::Selectable(state.curves[i].name.c_str(), sel))
                    state.selected_curve = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
    }

    // Interp toggle
    if (state.selected_curve >= 0 && state.selected_curve < static_cast<int>(state.curves.size())) {
        auto& curve = state.curves[state.selected_curve];
        const char* interp_label = curve.interp == CurveInterp::Linear ? "Linear" : "Cubic";
        if (ImGui::Button(interp_label, ImVec2(60, 0))) {
            curve.interp = (curve.interp == CurveInterp::Linear) ? CurveInterp::Cubic : CurveInterp::Linear;
            modified = true;
        }
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &state.snap_to_grid);
    }

    // Canvas
    ImVec2 canvas_size = size;
    if (canvas_size.x <= 0) canvas_size.x = ImGui::GetContentRegionAvail().x;
    if (canvas_size.y <= 0) canvas_size.y = 200;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##curve_canvas", canvas_size,
                            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool canvas_hovered = ImGui::IsItemHovered();
    bool canvas_active = ImGui::IsItemActive();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float tmin = state.view_time_min, tmax = state.view_time_max;
    float vmin = state.view_value_min, vmax = state.view_value_max;

    // Background
    dl->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                      IM_COL32(30, 30, 30, 255));
    dl->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                IM_COL32(80, 80, 80, 255));

    // Grid lines
    {
        float time_step = (tmax - tmin) / 10.0f;
        float val_step  = (vmax - vmin) / 8.0f;
        for (float t = tmin; t <= tmax + 1e-5f; t += time_step) {
            ImVec2 p = CurveToScreen(t, vmin, canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
            dl->AddLine(ImVec2(p.x, canvas_pos.y), ImVec2(p.x, canvas_pos.y + canvas_size.y),
                        IM_COL32(50, 50, 50, 255));
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", t);
            dl->AddText(ImVec2(p.x + 2, canvas_pos.y + canvas_size.y - 14), IM_COL32(100, 100, 100, 200), buf);
        }
        for (float v = vmin; v <= vmax + 1e-5f; v += val_step) {
            ImVec2 p = CurveToScreen(tmin, v, canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
            dl->AddLine(ImVec2(canvas_pos.x, p.y), ImVec2(canvas_pos.x + canvas_size.x, p.y),
                        IM_COL32(50, 50, 50, 255));
            char buf[16];
            snprintf(buf, sizeof(buf), "%.2f", v);
            dl->AddText(ImVec2(canvas_pos.x + 2, p.y - 12), IM_COL32(100, 100, 100, 200), buf);
        }
    }

    // Draw all curves
    for (int ci = 0; ci < static_cast<int>(state.curves.size()); ++ci) {
        auto& curve = state.curves[ci];
        if (curve.keys.empty()) continue;
        bool is_selected = (ci == state.selected_curve);
        ImU32 col = is_selected ? curve.color : ((curve.color & 0x00FFFFFF) | 0x60000000);
        float thickness = is_selected ? 2.0f : 1.0f;

        // Sample the curve and draw polyline
        const int samples = static_cast<int>(canvas_size.x);
        ImVec2 prev{};
        for (int s = 0; s <= samples; ++s) {
            float t = tmin + (tmax - tmin) * (static_cast<float>(s) / samples);
            float v = curve.Evaluate(t);
            ImVec2 screen = CurveToScreen(t, v, canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
            if (s > 0) dl->AddLine(prev, screen, col, thickness);
            prev = screen;
        }
    }

    // Draw keyframes for selected curve
    if (state.selected_curve >= 0 && state.selected_curve < static_cast<int>(state.curves.size())) {
        auto& curve = state.curves[state.selected_curve];
        const float key_radius = 5.0f;
        ImVec2 mouse = ImGui::GetMousePos();

        for (int ki = 0; ki < static_cast<int>(curve.keys.size()); ++ki) {
            auto& key = curve.keys[ki];
            ImVec2 kp = CurveToScreen(key.time, key.value, canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
            bool is_sel = (ki == state.selected_key);

            ImU32 key_col = is_sel ? IM_COL32(255, 200, 50, 255) : IM_COL32(200, 200, 200, 255);
            dl->AddCircleFilled(kp, key_radius, key_col);
            dl->AddCircle(kp, key_radius, IM_COL32(0, 0, 0, 200));

            // Tangent handles for cubic + selected key
            if (is_sel && curve.interp == CurveInterp::Cubic) {
                float handle_len = 30.0f;
                float time_per_px = (tmax - tmin) / canvas_size.x;
                float val_per_px = (vmax - vmin) / canvas_size.y;
                ImVec2 in_handle(kp.x - handle_len, kp.y + key.in_tangent * handle_len * val_per_px / time_per_px);
                ImVec2 out_handle(kp.x + handle_len, kp.y - key.out_tangent * handle_len * val_per_px / time_per_px);
                dl->AddLine(kp, in_handle, IM_COL32(100, 200, 255, 200));
                dl->AddLine(kp, out_handle, IM_COL32(255, 150, 100, 200));
                dl->AddCircleFilled(in_handle, 3.0f, IM_COL32(100, 200, 255, 255));
                dl->AddCircleFilled(out_handle, 3.0f, IM_COL32(255, 150, 100, 255));
            }

            // Tooltip on hover
            float dx = mouse.x - kp.x, dy = mouse.y - kp.y;
            if (dx * dx + dy * dy < (key_radius + 3) * (key_radius + 3) && canvas_hovered) {
                ImGui::SetTooltip("t=%.3f  v=%.3f", key.time, key.value);
            }
        }

        // ── Interaction ──────────────────────────────────────────────────
        // Click to select/start drag
        if (canvas_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            state.selected_key = -1;
            for (int ki = 0; ki < static_cast<int>(curve.keys.size()); ++ki) {
                ImVec2 kp = CurveToScreen(curve.keys[ki].time, curve.keys[ki].value,
                                           canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
                float dx = mouse.x - kp.x, dy = mouse.y - kp.y;
                if (dx * dx + dy * dy < (key_radius + 4) * (key_radius + 4)) {
                    state.selected_key = ki;
                    state.dragging_key = true;
                    break;
                }
            }
        }

        // Drag keyframe
        if (state.dragging_key && state.selected_key >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 cv = ScreenToCurve(mouse, canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
            float new_t = cv.x;
            float new_v = cv.y;
            if (state.snap_to_grid) {
                new_t = SnapValue(new_t, state.grid_snap_time);
                new_v = SnapValue(new_v, state.grid_snap_value);
            }
            curve.keys[state.selected_key].time = new_t;
            curve.keys[state.selected_key].value = new_v;
            modified = true;
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            if (state.dragging_key) {
                curve.SortKeys();
                // Re-find selected key after sort
                state.dragging_key = false;
            }
        }

        // Double-click to add key
        if (canvas_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !state.dragging_key) {
            ImVec2 cv = ScreenToCurve(mouse, canvas_pos, canvas_size, tmin, tmax, vmin, vmax);
            CurveKey nk;
            nk.time = cv.x;
            nk.value = cv.y;
            curve.keys.push_back(nk);
            curve.SortKeys();
            state.selected_key = -1;
            modified = true;
        }

        // Delete selected key
        if (state.selected_key >= 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            curve.keys.erase(curve.keys.begin() + state.selected_key);
            state.selected_key = -1;
            modified = true;
        }

        // Pan with middle mouse
        if (canvas_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            float dt = -(delta.x / canvas_size.x) * (tmax - tmin);
            float dv = (delta.y / canvas_size.y) * (vmax - vmin);
            state.view_time_min += dt;
            state.view_time_max += dt;
            state.view_value_min += dv;
            state.view_value_max += dv;
        }

        // Zoom with scroll
        if (canvas_hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.01f) {
            float zoom = 1.0f - ImGui::GetIO().MouseWheel * 0.1f;
            float tc = (tmin + tmax) * 0.5f;
            float vc = (vmin + vmax) * 0.5f;
            state.view_time_min = tc + (tmin - tc) * zoom;
            state.view_time_max = tc + (tmax - tc) * zoom;
            state.view_value_min = vc + (vmin - vc) * zoom;
            state.view_value_max = vc + (vmax - vc) * zoom;
        }
    }

    // Key info bar
    if (state.selected_curve >= 0 && state.selected_curve < static_cast<int>(state.curves.size())) {
        auto& curve = state.curves[state.selected_curve];
        ImGui::Text("Keys: %d", static_cast<int>(curve.keys.size()));
        if (state.selected_key >= 0 && state.selected_key < static_cast<int>(curve.keys.size())) {
            auto& k = curve.keys[state.selected_key];
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            modified |= ImGui::DragFloat("T", &k.time, 0.01f);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            modified |= ImGui::DragFloat("V", &k.value, 0.01f);
            if (curve.interp == CurveInterp::Cubic) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60);
                modified |= ImGui::DragFloat("In", &k.in_tangent, 0.01f);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60);
                modified |= ImGui::DragFloat("Out", &k.out_tangent, 0.01f);
            }
        }
    }

    ImGui::PopID();
    return modified;
}

} // namespace dse::editor

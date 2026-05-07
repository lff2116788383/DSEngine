#include "editor_particle_panel.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_shortcuts.h"
#include "engine/ecs/particle_2d.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>

namespace dse::editor {

namespace {

// ============================================================================
// Curve Editor Widget State
// ============================================================================

struct CurveEditorState {
    int dragging_point = -1;       // index of point being dragged (-1 = none)
    bool drag_started = false;     // true when drag began this frame
    ParticleCurve old_curve;       // snapshot for undo on drag start
};

CurveEditorState& GetCurveEditorState(const char* id) {
    static std::unordered_map<std::string, CurveEditorState> states;
    return states[id];
}

// ============================================================================
// Helper: MarkParticleEmitterDirty (local declaration)
// ============================================================================
void MarkDirty(ParticleEmitterComponent& /*emitter*/) {
    // Particles are rebuilt each frame from curves, no explicit dirty flag needed
}

// ============================================================================
// Helper: Evaluate a curve at normalized t for visualization
// ============================================================================
float EvalCurveForDisplay(const ParticleCurve& curve, float t) {
    return curve.Evaluate(t);
}

// ============================================================================
// Helper: Convert curve value to canvas Y (value_min at bottom, value_max at top)
// ============================================================================
float ValueToCanvasY(float value, float canvas_top, float canvas_height,
                     float value_min, float value_max) {
    float norm = (value - value_min) / (value_max - value_min);
    return canvas_top + canvas_height * (1.0f - norm);
}

float CanvasYToValue(float y, float canvas_top, float canvas_height,
                     float value_min, float value_max) {
    float norm = 1.0f - (y - canvas_top) / canvas_height;
    return value_min + norm * (value_max - value_min);
}

float TimeToCanvasX(float time, float canvas_left, float canvas_width) {
    return canvas_left + time * canvas_width;
}

float CanvasXToTime(float x, float canvas_left, float canvas_width) {
    return (x - canvas_left) / canvas_width;
}

// ============================================================================
// Draw a single curve editor widget (uses ImDrawList)
// Returns true if curve was modified
// ============================================================================
bool DrawCurveWidget(const char* label, ParticleCurve& curve,
                     float value_min, float value_max,
                     entt::registry& registry, entt::entity entity) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const float canvas_width = ImGui::GetContentRegionAvail().x;
    const float canvas_height = 120.0f;
    const float padding = 4.0f;

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(canvas_width, canvas_height);
    ImRect bb(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y));

    ImGui::ItemSize(bb);
    ImGuiID widget_id = window->GetID(label);
    if (!ImGui::ItemAdd(bb, widget_id)) return false;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddRectFilled(bb.Min, bb.Max, IM_COL32(30, 30, 35, 255));
    draw_list->AddRect(bb.Min, bb.Max, IM_COL32(80, 80, 90, 255));

    // Grid lines (horizontal at 0, 0.25, 0.5, 0.75, 1.0 of value range)
    for (int i = 0; i <= 4; ++i) {
        float frac = i / 4.0f;
        float y = bb.Min.y + canvas_height * (1.0f - frac);
        ImU32 grid_col = (i == 0 || i == 4) ? IM_COL32(60, 60, 70, 200) : IM_COL32(50, 50, 60, 120);
        draw_list->AddLine(ImVec2(bb.Min.x, y), ImVec2(bb.Max.x, y), grid_col, 1.0f);
    }
    // Vertical grid (0, 0.25, 0.5, 0.75, 1.0 of time)
    for (int i = 0; i <= 4; ++i) {
        float frac = i / 4.0f;
        float x = bb.Min.x + canvas_width * frac;
        ImU32 grid_col = (i == 0 || i == 4) ? IM_COL32(60, 60, 70, 200) : IM_COL32(50, 50, 60, 120);
        draw_list->AddLine(ImVec2(x, bb.Min.y), ImVec2(x, bb.Max.y), grid_col, 1.0f);
    }

    // Draw the curve polyline
    const int segments = 64;
    ImVec2 prev_pt;
    for (int i = 0; i <= segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        float val = EvalCurveForDisplay(curve, t);
        float x = TimeToCanvasX(t, bb.Min.x, canvas_width);
        float y = ValueToCanvasY(val, bb.Min.y, canvas_height, value_min, value_max);
        ImVec2 pt(x, y);
        if (i > 0) {
            draw_list->AddLine(prev_pt, pt, IM_COL32(100, 200, 255, 255), 2.0f);
        }
        prev_pt = pt;
    }

    // --- Interactive control points (Custom mode) ---
    bool modified = false;
    auto& state = GetCurveEditorState(label);
    ImVec2 mouse_pos = ImGui::GetMousePos();
    bool hovered = ImGui::IsItemHovered();

    if (curve.type == ParticleCurveType::Custom) {
        // Ensure at least 2 keyframes
        if (curve.keyframes.size() < 2) {
            curve.keyframes.clear();
            curve.keyframes.push_back({0.0f, curve.start_value});
            curve.keyframes.push_back({1.0f, curve.end_value});
            modified = true;
        }

        const float point_radius = 6.0f;
        const float hit_radius = 10.0f;

        // Draw control points
        for (size_t i = 0; i < curve.keyframes.size(); ++i) {
            auto& kf = curve.keyframes[i];
            float px = TimeToCanvasX(kf.time, bb.Min.x, canvas_width);
            float py = ValueToCanvasY(kf.value, bb.Min.y, canvas_height, value_min, value_max);
            ImVec2 pt(px, py);

            bool pt_hovered = (std::abs(mouse_pos.x - px) < hit_radius &&
                               std::abs(mouse_pos.y - py) < hit_radius);

            ImU32 pt_color = pt_hovered ? IM_COL32(255, 255, 100, 255) : IM_COL32(255, 200, 80, 255);
            if (state.dragging_point == static_cast<int>(i)) {
                pt_color = IM_COL32(255, 100, 50, 255);
            }
            draw_list->AddCircleFilled(pt, point_radius, pt_color);
            draw_list->AddCircle(pt, point_radius, IM_COL32(255, 255, 255, 180), 0, 1.5f);
        }

        // --- Left-click: select & drag control point ---
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            int closest = -1;
            float closest_dist = hit_radius;
            for (size_t i = 0; i < curve.keyframes.size(); ++i) {
                float px = TimeToCanvasX(curve.keyframes[i].time, bb.Min.x, canvas_width);
                float py = ValueToCanvasY(curve.keyframes[i].value, bb.Min.y, canvas_height, value_min, value_max);
                float dist = std::max(std::abs(mouse_pos.x - px), std::abs(mouse_pos.y - py));
                if (dist < closest_dist) {
                    closest_dist = dist;
                    closest = static_cast<int>(i);
                }
            }
            if (closest >= 0) {
                state.dragging_point = closest;
                state.drag_started = true;
                state.old_curve = curve;  // snapshot for undo
            }
        }

        // Dragging
        if (state.dragging_point >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            int idx = state.dragging_point;
            if (idx >= 0 && idx < static_cast<int>(curve.keyframes.size())) {
                float new_time = CanvasXToTime(mouse_pos.x, bb.Min.x, canvas_width);
                float new_value = CanvasYToValue(mouse_pos.y, bb.Min.y, canvas_height, value_min, value_max);
                new_time = std::clamp(new_time, 0.0f, 1.0f);
                new_value = std::clamp(new_value, value_min, value_max);

                // First and last keyframes are time-locked at 0 and 1
                if (idx == 0) new_time = 0.0f;
                if (idx == static_cast<int>(curve.keyframes.size()) - 1) new_time = 1.0f;

                curve.keyframes[idx].time = new_time;
                curve.keyframes[idx].value = new_value;
                modified = true;
            }
        }

        // Release: push undo command
        if (state.dragging_point >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // Sort keyframes by time
            std::sort(curve.keyframes.begin(), curve.keyframes.end(),
                      [](const CurveKeyframe& a, const CurveKeyframe& b) { return a.time < b.time; });

            // Push undo
            ParticleCurve old_curve = state.old_curve;
            ParticleCurve new_curve = curve;
            entt::entity ent = entity;
            auto& undo_mgr = GetUndoRedoManager();
            std::string desc = std::string("Curve Edit: ") + label;
            auto cmd = std::make_unique<LambdaCommand>(
                desc,
                [&reg = registry, ent, new_curve, label_str = std::string(label)]() {
                    if (!reg.valid(ent) || !reg.all_of<ParticleEmitterComponent>(ent)) return;
                    auto& emitter = reg.get<ParticleEmitterComponent>(ent);
                    // Find matching curve by label
                    if (label_str.find("Size") != std::string::npos) emitter.size_curve = new_curve;
                    else if (label_str.find("Alpha") != std::string::npos) emitter.alpha_curve = new_curve;
                    else if (label_str.find("Speed") != std::string::npos) emitter.speed_curve = new_curve;
                },
                [&reg = registry, ent, old_curve, label_str = std::string(label)]() {
                    if (!reg.valid(ent) || !reg.all_of<ParticleEmitterComponent>(ent)) return;
                    auto& emitter = reg.get<ParticleEmitterComponent>(ent);
                    if (label_str.find("Size") != std::string::npos) emitter.size_curve = old_curve;
                    else if (label_str.find("Alpha") != std::string::npos) emitter.alpha_curve = old_curve;
                    else if (label_str.find("Speed") != std::string::npos) emitter.speed_curve = old_curve;
                },
                ""
            );
            undo_mgr.Execute(std::move(cmd), true);

            state.dragging_point = -1;
            state.drag_started = false;
        }

        // --- Right-click: add or delete control point ---
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            // Check if clicking near an existing point (delete it)
            int to_delete = -1;
            for (size_t i = 1; i + 1 < curve.keyframes.size(); ++i) {  // never delete first/last
                float px = TimeToCanvasX(curve.keyframes[i].time, bb.Min.x, canvas_width);
                float py = ValueToCanvasY(curve.keyframes[i].value, bb.Min.y, canvas_height, value_min, value_max);
                float dist = std::max(std::abs(mouse_pos.x - px), std::abs(mouse_pos.y - py));
                if (dist < hit_radius) {
                    to_delete = static_cast<int>(i);
                    break;
                }
            }

            ParticleCurve old_curve_snap = curve;

            if (to_delete >= 0) {
                curve.keyframes.erase(curve.keyframes.begin() + to_delete);
            } else {
                // Add new control point at mouse position
                float new_time = CanvasXToTime(mouse_pos.x, bb.Min.x, canvas_width);
                float new_value = CanvasYToValue(mouse_pos.y, bb.Min.y, canvas_height, value_min, value_max);
                new_time = std::clamp(new_time, 0.01f, 0.99f);
                new_value = std::clamp(new_value, value_min, value_max);
                curve.keyframes.push_back({new_time, new_value});
                std::sort(curve.keyframes.begin(), curve.keyframes.end(),
                          [](const CurveKeyframe& a, const CurveKeyframe& b) { return a.time < b.time; });
            }

            // Push undo for add/delete
            ParticleCurve new_curve_snap = curve;
            entt::entity ent = entity;
            auto& undo_mgr = GetUndoRedoManager();
            std::string desc = to_delete >= 0 ? "Delete Curve Point" : "Add Curve Point";
            auto cmd = std::make_unique<LambdaCommand>(
                desc,
                [&reg = registry, ent, new_curve_snap, label_str = std::string(label)]() {
                    if (!reg.valid(ent) || !reg.all_of<ParticleEmitterComponent>(ent)) return;
                    auto& emitter = reg.get<ParticleEmitterComponent>(ent);
                    if (label_str.find("Size") != std::string::npos) emitter.size_curve = new_curve_snap;
                    else if (label_str.find("Alpha") != std::string::npos) emitter.alpha_curve = new_curve_snap;
                    else if (label_str.find("Speed") != std::string::npos) emitter.speed_curve = new_curve_snap;
                },
                [&reg = registry, ent, old_curve_snap, label_str = std::string(label)]() {
                    if (!reg.valid(ent) || !reg.all_of<ParticleEmitterComponent>(ent)) return;
                    auto& emitter = reg.get<ParticleEmitterComponent>(ent);
                    if (label_str.find("Size") != std::string::npos) emitter.size_curve = old_curve_snap;
                    else if (label_str.find("Alpha") != std::string::npos) emitter.alpha_curve = old_curve_snap;
                    else if (label_str.find("Speed") != std::string::npos) emitter.speed_curve = old_curve_snap;
                },
                ""
            );
            undo_mgr.Execute(std::move(cmd), true);
            modified = true;
        }
    } else {
        // For preset types, draw start/end handles as visual indicators (non-draggable)
        float start_x = bb.Min.x;
        float start_y = ValueToCanvasY(curve.start_value, bb.Min.y, canvas_height, value_min, value_max);
        float end_x = bb.Max.x;
        float end_y = ValueToCanvasY(curve.end_value, bb.Min.y, canvas_height, value_min, value_max);

        draw_list->AddCircleFilled(ImVec2(start_x, start_y), 5.0f, IM_COL32(100, 255, 100, 200));
        draw_list->AddCircleFilled(ImVec2(end_x, end_y), 5.0f, IM_COL32(255, 100, 100, 200));
    }

    // Axis labels
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", value_max);
    draw_list->AddText(ImVec2(bb.Min.x + 2, bb.Min.y + 1), IM_COL32(150, 150, 150, 200), buf);
    std::snprintf(buf, sizeof(buf), "%.1f", value_min);
    draw_list->AddText(ImVec2(bb.Min.x + 2, bb.Max.y - 14), IM_COL32(150, 150, 150, 200), buf);
    draw_list->AddText(ImVec2(bb.Min.x + 2, bb.Min.y + canvas_height * 0.5f - 7), IM_COL32(120, 120, 120, 150), label);

    // Tooltip showing value under cursor
    if (hovered) {
        float hover_t = CanvasXToTime(mouse_pos.x, bb.Min.x, canvas_width);
        hover_t = std::clamp(hover_t, 0.0f, 1.0f);
        float hover_val = EvalCurveForDisplay(curve, hover_t);
        ImGui::SetTooltip("t=%.2f  val=%.3f", hover_t, hover_val);
    }

    return modified;
}

} // namespace

// ============================================================================
// Public API: Draw the full particle curve editor section
// ============================================================================

void DrawParticleCurveEditor(entt::registry& registry, entt::entity entity) {
    if (!registry.all_of<ParticleEmitterComponent>(entity)) return;

    auto& emitter = registry.get<ParticleEmitterComponent>(entity);

    if (!ImGui::CollapsingHeader(MDI_ICON_CREATION "  Particle Curves", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Indent(4.0f);
    ImGui::TextDisabled("Left-drag: move point | Right-click: add/delete point");
    ImGui::TextDisabled("Set curve type to 'Custom' to enable keyframe editing");
    ImGui::Spacing();

    // --- Size over Lifetime ---
    if (emitter.size_curve.enabled) {
        ImGui::PushID("size_curve_editor");
        ImGui::Text("Size over Lifetime");
        const char* curve_types[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Custom" };
        int current_type = static_cast<int>(emitter.size_curve.type);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("##size_type", &current_type, curve_types, IM_ARRAYSIZE(curve_types))) {
            emitter.size_curve.type = static_cast<ParticleCurveType>(current_type);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##size_start", &emitter.size_curve.start_value, 0.05f, 0.0f, 100.0f, "S:%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##size_end", &emitter.size_curve.end_value, 0.05f, 0.0f, 100.0f, "E:%.1f");

        DrawCurveWidget("Size", emitter.size_curve, 0.0f, std::max(emitter.size_curve.start_value, emitter.size_curve.end_value) * 1.2f + 0.1f, registry, entity);
        ImGui::Spacing();
        ImGui::PopID();
    }

    // --- Alpha over Lifetime ---
    if (emitter.alpha_curve.enabled) {
        ImGui::PushID("alpha_curve_editor");
        ImGui::Text("Alpha over Lifetime");
        const char* curve_types[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Custom" };
        int current_type = static_cast<int>(emitter.alpha_curve.type);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("##alpha_type", &current_type, curve_types, IM_ARRAYSIZE(curve_types))) {
            emitter.alpha_curve.type = static_cast<ParticleCurveType>(current_type);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##alpha_start", &emitter.alpha_curve.start_value, 0.01f, 0.0f, 1.0f, "S:%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##alpha_end", &emitter.alpha_curve.end_value, 0.01f, 0.0f, 1.0f, "E:%.2f");

        DrawCurveWidget("Alpha", emitter.alpha_curve, 0.0f, 1.0f, registry, entity);
        ImGui::Spacing();
        ImGui::PopID();
    }

    // --- Speed over Lifetime ---
    if (emitter.speed_curve.enabled) {
        ImGui::PushID("speed_curve_editor");
        ImGui::Text("Speed over Lifetime");
        const char* curve_types[] = { "Linear", "EaseIn", "EaseOut", "EaseInOut", "Custom" };
        int current_type = static_cast<int>(emitter.speed_curve.type);
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("##speed_type", &current_type, curve_types, IM_ARRAYSIZE(curve_types))) {
            emitter.speed_curve.type = static_cast<ParticleCurveType>(current_type);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##speed_start", &emitter.speed_curve.start_value, 0.05f, 0.0f, 10.0f, "S:%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##speed_end", &emitter.speed_curve.end_value, 0.05f, 0.0f, 10.0f, "E:%.1f");

        DrawCurveWidget("Speed", emitter.speed_curve, 0.0f, std::max(emitter.speed_curve.start_value, emitter.speed_curve.end_value) * 1.2f + 0.1f, registry, entity);
        ImGui::Spacing();
        ImGui::PopID();
    }

    // --- Color over Lifetime (simple gradient, no curve widget) ---
    if (emitter.use_color_curve) {
        ImGui::PushID("color_curve_editor");
        ImGui::Text("Color over Lifetime");
        ImGui::ColorEdit4("Start Color", &emitter.start_color.x);
        ImGui::ColorEdit4("End Color", &emitter.color_curve_end.x);

        // Draw color gradient preview bar
        ImVec2 bar_pos = ImGui::GetCursorScreenPos();
        float bar_width = ImGui::GetContentRegionAvail().x;
        float bar_height = 20.0f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const int color_segments = 32;
        for (int i = 0; i < color_segments; ++i) {
            float t0 = static_cast<float>(i) / static_cast<float>(color_segments);
            float t1 = static_cast<float>(i + 1) / static_cast<float>(color_segments);
            glm::vec4 c0 = glm::mix(emitter.start_color, emitter.color_curve_end, t0);
            glm::vec4 c1 = glm::mix(emitter.start_color, emitter.color_curve_end, t1);
            ImU32 col0 = IM_COL32(static_cast<int>(c0.r * 255), static_cast<int>(c0.g * 255),
                                   static_cast<int>(c0.b * 255), static_cast<int>(c0.a * 255));
            ImU32 col1 = IM_COL32(static_cast<int>(c1.r * 255), static_cast<int>(c1.g * 255),
                                   static_cast<int>(c1.b * 255), static_cast<int>(c1.a * 255));
            float x0 = bar_pos.x + t0 * bar_width;
            float x1 = bar_pos.x + t1 * bar_width;
            dl->AddRectFilledMultiColor(
                ImVec2(x0, bar_pos.y), ImVec2(x1, bar_pos.y + bar_height),
                col0, col1, col1, col0);
        }
        dl->AddRect(ImVec2(bar_pos.x, bar_pos.y),
                    ImVec2(bar_pos.x + bar_width, bar_pos.y + bar_height),
                    IM_COL32(80, 80, 90, 255));
        ImGui::Dummy(ImVec2(bar_width, bar_height));
        ImGui::PopID();
    }

    // Show message if no curves are enabled
    if (!emitter.size_curve.enabled && !emitter.alpha_curve.enabled &&
        !emitter.speed_curve.enabled && !emitter.use_color_curve) {
        ImGui::TextDisabled("Enable curves in the Particle Emitter section above to edit them here.");
    }

    ImGui::Unindent(4.0f);
}

} // namespace dse::editor

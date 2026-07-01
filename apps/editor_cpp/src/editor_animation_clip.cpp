/**
 * @file editor_animation_clip.cpp
 * @brief Animation Clip Editor — 3D bone pose preview, curve fine-tuning, additive layer editing
 */

#include "editor_animation_clip.h"
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

// ─── Data model ─────────────────────────────────────────────────────────

struct BoneTransform {
    float position[3] = {0, 0, 0};
    float rotation[4] = {0, 0, 0, 1}; // quaternion xyzw
    float scale[3] = {1, 1, 1};
};

struct BoneInfo {
    std::string name;
    int parent_index = -1;
    BoneTransform bind_pose;
    BoneTransform current_pose;
    float bone_length = 0.5f;
};

struct CurveKeyframe {
    float time = 0.0f;
    float value = 0.0f;
    float in_tangent = 0.0f;
    float out_tangent = 0.0f;
    enum class InterpMode { Linear, Cubic, Step } mode = InterpMode::Cubic;
};

struct AnimationCurve {
    std::string bone_name;
    std::string property; // "pos.x", "rot.y", "scale.z" etc
    std::vector<CurveKeyframe> keyframes;
    bool visible = true;
    ImU32 color = IM_COL32(200, 200, 200, 255);
};

struct AdditiveLayer {
    std::string name;
    float weight = 1.0f;
    bool active = true;
    bool solo = false;
    bool muted = false;
    std::vector<AnimationCurve> curves;
};

struct AnimClipState {
    // Skeleton
    std::vector<BoneInfo> bones;
    int selected_bone = -1;

    // Curves
    std::vector<AnimationCurve> curves;
    int selected_curve = -1;
    int selected_keyframe = -1;

    // Additive layers
    std::vector<AdditiveLayer> layers;
    int selected_layer = 0;

    // Playback
    float current_time = 0.0f;
    float clip_duration = 2.0f;
    bool playing = false;
    float playback_speed = 1.0f;
    bool loop = true;

    // 3D Preview
    float preview_rotation_y = 0.0f;
    float preview_rotation_x = 0.2f;
    float preview_zoom = 1.0f;
    bool show_bind_pose = false;

    // Curve editor view
    float curve_view_x = 0.0f;
    float curve_view_y = 0.0f;
    float curve_zoom_x = 1.0f;
    float curve_zoom_y = 1.0f;
    bool snap_to_frame = true;
    float frame_rate = 30.0f;

    bool initialized = false;
};

static AnimClipState s_state;

void InitDemoSkeleton() {
    if (s_state.initialized) return;
    s_state.initialized = true;

    // Create a simple humanoid skeleton for preview
    auto addBone = [](const char* name, int parent, float x, float y, float z, float len) {
        BoneInfo b;
        b.name = name;
        b.parent_index = parent;
        b.bind_pose.position[0] = x; b.bind_pose.position[1] = y; b.bind_pose.position[2] = z;
        b.current_pose = b.bind_pose;
        b.bone_length = len;
        s_state.bones.push_back(b);
        return static_cast<int>(s_state.bones.size()) - 1;
    };

    int root = addBone("Root", -1, 0, 0, 0, 0.1f);
    int hip = addBone("Hips", root, 0, 1.0f, 0, 0.15f);
    int spine = addBone("Spine", hip, 0, 0.2f, 0, 0.25f);
    int chest = addBone("Chest", spine, 0, 0.25f, 0, 0.2f);
    int neck = addBone("Neck", chest, 0, 0.2f, 0, 0.1f);
    addBone("Head", neck, 0, 0.15f, 0, 0.15f);
    int l_shoulder = addBone("L_Shoulder", chest, 0.15f, 0.15f, 0, 0.1f);
    int l_arm = addBone("L_UpperArm", l_shoulder, 0.1f, 0, 0, 0.25f);
    addBone("L_ForeArm", l_arm, 0.25f, 0, 0, 0.22f);
    int r_shoulder = addBone("R_Shoulder", chest, -0.15f, 0.15f, 0, 0.1f);
    int r_arm = addBone("R_UpperArm", r_shoulder, -0.1f, 0, 0, 0.25f);
    addBone("R_ForeArm", r_arm, -0.25f, 0, 0, 0.22f);
    int l_leg = addBone("L_Thigh", hip, 0.1f, -0.05f, 0, 0.35f);
    addBone("L_Calf", l_leg, 0, -0.35f, 0, 0.32f);
    int r_leg = addBone("R_Thigh", hip, -0.1f, -0.05f, 0, 0.35f);
    addBone("R_Calf", r_leg, 0, -0.35f, 0, 0.32f);
    (void)root; (void)spine; (void)chest; (void)neck;

    // Create demo curves
    AnimationCurve c;
    c.bone_name = "Hips";
    c.property = "pos.y";
    c.color = IM_COL32(100, 200, 100, 255);
    c.keyframes.push_back({0.0f, 1.0f, 0, 0.5f, CurveKeyframe::InterpMode::Cubic});
    c.keyframes.push_back({0.5f, 1.1f, 0.5f, -0.5f, CurveKeyframe::InterpMode::Cubic});
    c.keyframes.push_back({1.0f, 1.0f, -0.5f, 0.5f, CurveKeyframe::InterpMode::Cubic});
    c.keyframes.push_back({1.5f, 1.1f, 0.5f, -0.5f, CurveKeyframe::InterpMode::Cubic});
    c.keyframes.push_back({2.0f, 1.0f, -0.5f, 0, CurveKeyframe::InterpMode::Cubic});
    s_state.curves.push_back(c);

    c = {};
    c.bone_name = "L_UpperArm";
    c.property = "rot.z";
    c.color = IM_COL32(200, 100, 100, 255);
    c.keyframes.push_back({0.0f, 0, 0, 0.3f, CurveKeyframe::InterpMode::Cubic});
    c.keyframes.push_back({1.0f, 0.5f, 0, 0, CurveKeyframe::InterpMode::Cubic});
    c.keyframes.push_back({2.0f, 0, -0.3f, 0, CurveKeyframe::InterpMode::Cubic});
    s_state.curves.push_back(c);

    // Demo additive layer
    AdditiveLayer layer;
    layer.name = "Base";
    layer.weight = 1.0f;
    s_state.layers.push_back(layer);

    AdditiveLayer additive;
    additive.name = "Breathing";
    additive.weight = 0.5f;
    AnimationCurve bc;
    bc.bone_name = "Chest";
    bc.property = "scale.y";
    bc.keyframes.push_back({0.0f, 1.0f, 0, 0, CurveKeyframe::InterpMode::Cubic});
    bc.keyframes.push_back({1.0f, 1.02f, 0, 0, CurveKeyframe::InterpMode::Cubic});
    bc.keyframes.push_back({2.0f, 1.0f, 0, 0, CurveKeyframe::InterpMode::Cubic});
    additive.curves.push_back(bc);
    s_state.layers.push_back(additive);
}

// Simple 2D bone skeleton rendering (side view projection)
void DrawBoneSkeleton(ImDrawList* dl, ImVec2 origin, float scale) {
    for (int i = 0; i < static_cast<int>(s_state.bones.size()); i++) {
        auto& bone = s_state.bones[i];
        // Accumulate position
        float wx = bone.current_pose.position[0];
        float wy = bone.current_pose.position[1];
        int parent = bone.parent_index;
        while (parent >= 0) {
            wx += s_state.bones[parent].current_pose.position[0];
            wy += s_state.bones[parent].current_pose.position[1];
            parent = s_state.bones[parent].parent_index;
        }

        float cos_r = std::cos(s_state.preview_rotation_y);
        float sin_r = std::sin(s_state.preview_rotation_y);
        float projected_x = wx * cos_r;

        ImVec2 screen_pos(origin.x + projected_x * scale * 150.0f,
                          origin.y - wy * scale * 150.0f);

        // Draw bone joint
        bool is_selected = (i == s_state.selected_bone);
        ImU32 joint_color = is_selected ? IM_COL32(255, 200, 50, 255) : IM_COL32(180, 180, 220, 255);
        dl->AddCircleFilled(screen_pos, is_selected ? 5.0f : 3.5f, joint_color);

        // Draw bone line to parent
        if (bone.parent_index >= 0) {
            auto& pbone = s_state.bones[bone.parent_index];
            float pwx = 0, pwy = 0;
            int pp = bone.parent_index;
            while (pp >= 0) {
                pwx += s_state.bones[pp].current_pose.position[0];
                pwy += s_state.bones[pp].current_pose.position[1];
                pp = s_state.bones[pp].parent_index;
            }
            float ppx = pwx * cos_r;
            ImVec2 parent_pos(origin.x + ppx * scale * 150.0f,
                              origin.y - pwy * scale * 150.0f);
            dl->AddLine(parent_pos, screen_pos, IM_COL32(120, 120, 180, 200), 2.0f);
        }

        // Bone name label for selected
        if (is_selected) {
            dl->AddText(ImVec2(screen_pos.x + 8, screen_pos.y - 6),
                        IM_COL32(255, 220, 100, 255), bone.name.c_str());
        }
    }
}

float EvaluateCurve(const AnimationCurve& curve, float time) {
    if (curve.keyframes.empty()) return 0.0f;
    if (curve.keyframes.size() == 1) return curve.keyframes[0].value;
    if (time <= curve.keyframes.front().time) return curve.keyframes.front().value;
    if (time >= curve.keyframes.back().time) return curve.keyframes.back().value;

    for (size_t i = 0; i + 1 < curve.keyframes.size(); i++) {
        auto& k0 = curve.keyframes[i];
        auto& k1 = curve.keyframes[i + 1];
        if (time >= k0.time && time <= k1.time) {
            float t = (time - k0.time) / (k1.time - k0.time);
            if (k0.mode == CurveKeyframe::InterpMode::Step) return k0.value;
            if (k0.mode == CurveKeyframe::InterpMode::Linear) return k0.value + t * (k1.value - k0.value);
            // Cubic hermite
            float t2 = t * t, t3 = t2 * t;
            float h00 = 2*t3 - 3*t2 + 1;
            float h10 = t3 - 2*t2 + t;
            float h01 = -2*t3 + 3*t2;
            float h11 = t3 - t2;
            float dt = k1.time - k0.time;
            return h00 * k0.value + h10 * dt * k0.out_tangent + h01 * k1.value + h11 * dt * k1.in_tangent;
        }
    }
    return 0.0f;
}

} // anonymous namespace

void DrawAnimationClipEditor(EditorContext& /*ctx*/) {
    InitDemoSkeleton();
    auto& state = s_state;

    ImGui::Begin(MDI_ICON_ANIMATION "  Animation Clip Editor");

    // ─── Toolbar ─────────────────────────────────────────────────────────
    {
        if (state.playing) {
            if (ImGui::Button(MDI_ICON_PAUSE " Pause")) state.playing = false;
        } else {
            if (ImGui::Button(MDI_ICON_PLAY " Play")) state.playing = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_STOP " Stop")) { state.playing = false; state.current_time = 0; }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_SKIP_PREVIOUS " |<")) state.current_time = 0;
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_SKIP_NEXT " >|")) state.current_time = state.clip_duration;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::SliderFloat("Speed", &state.playback_speed, 0.1f, 3.0f);
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &state.loop);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("Time", &state.current_time, 0.0f, state.clip_duration, "%.3f s");
    }

    // Update playback
    if (state.playing) {
        state.current_time += ImGui::GetIO().DeltaTime * state.playback_speed;
        if (state.current_time > state.clip_duration) {
            if (state.loop) state.current_time = std::fmod(state.current_time, state.clip_duration);
            else { state.current_time = state.clip_duration; state.playing = false; }
        }
    }

    ImGui::Separator();

    // ─── Main layout: Left = Bone list + 3D Preview, Right = Curve editor ─
    float left_width = 280.0f;
    ImGui::BeginChild("LeftPanel", ImVec2(left_width, 0), ImGuiChildFlags_Borders);

    // ─── 3D Bone Pose Preview ────────────────────────────────────────────
    ImGui::Text(MDI_ICON_HUMAN "  Bone Pose Preview");
    ImGui::SliderFloat("##rot_y", &state.preview_rotation_y, -3.14159f, 3.14159f, "Y: %.2f");
    ImGui::Checkbox("Show Bind Pose", &state.show_bind_pose);

    {
        ImVec2 preview_pos = ImGui::GetCursorScreenPos();
        ImVec2 preview_size(left_width - 16, 200);
        ImGui::InvisibleButton("bone_preview", preview_size);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(preview_pos,
            ImVec2(preview_pos.x + preview_size.x, preview_pos.y + preview_size.y),
            IM_COL32(20, 20, 30, 255));

        ImVec2 center(preview_pos.x + preview_size.x * 0.5f,
                      preview_pos.y + preview_size.y * 0.85f);
        DrawBoneSkeleton(dl, center, state.preview_zoom);

        // Grid floor
        for (int i = -3; i <= 3; i++) {
            float gx = center.x + i * 30.0f;
            dl->AddLine(ImVec2(gx, center.y - 2), ImVec2(gx, center.y + 2),
                        IM_COL32(60, 60, 80, 150));
        }
        dl->AddLine(ImVec2(preview_pos.x, center.y),
                    ImVec2(preview_pos.x + preview_size.x, center.y),
                    IM_COL32(60, 60, 80, 150));
    }

    ImGui::Separator();

    // ─── Bone Hierarchy ──────────────────────────────────────────────────
    ImGui::Text(MDI_ICON_BONE "  Bones");
    ImGui::BeginChild("BoneList", ImVec2(0, 150), ImGuiChildFlags_Borders);
    for (int i = 0; i < static_cast<int>(state.bones.size()); i++) {
        auto& bone = state.bones[i];
        // Indent based on depth
        int depth = 0;
        int p = bone.parent_index;
        while (p >= 0) { depth++; p = state.bones[p].parent_index; }
        ImGui::Indent(depth * 12.0f);
        bool selected = (state.selected_bone == i);
        if (ImGui::Selectable(bone.name.c_str(), selected)) {
            state.selected_bone = i;
        }
        ImGui::Unindent(depth * 12.0f);
    }
    ImGui::EndChild();

    // ─── Additive Layers ─────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::Text(MDI_ICON_LAYERS "  Additive Layers");
    if (ImGui::Button("+ Layer")) {
        AdditiveLayer nl;
        nl.name = "Layer " + std::to_string(state.layers.size());
        state.layers.push_back(nl);
    }
    ImGui::BeginChild("LayerList", ImVec2(0, 0), ImGuiChildFlags_Borders);
    for (int i = 0; i < static_cast<int>(state.layers.size()); i++) {
        auto& layer = state.layers[i];
        ImGui::PushID(i);
        bool sel = (state.selected_layer == i);
        if (ImGui::Selectable("##layersel", sel, 0, ImVec2(0, 28))) {
            state.selected_layer = i;
        }
        ImGui::SameLine();
        ImGui::Checkbox("##active", &layer.active);
        ImGui::SameLine();
        ImGui::Text("%s", layer.name.c_str());
        ImGui::SameLine(170);
        ImGui::SetNextItemWidth(60);
        ImGui::SliderFloat("##wt", &layer.weight, 0.0f, 1.0f, "%.2f");
        ImGui::SameLine();
        if (layer.muted) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 80, 80, 255));
        if (ImGui::SmallButton(layer.muted ? "M" : "m")) layer.muted = !layer.muted;
        if (layer.muted) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (layer.solo) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 200, 255, 255));
        if (ImGui::SmallButton(layer.solo ? "S" : "s")) layer.solo = !layer.solo;
        if (layer.solo) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::EndChild(); // LeftPanel

    ImGui::SameLine();

    // ─── Curve Editor (right panel) ──────────────────────────────────────
    ImGui::BeginChild("CurvePanel", ImVec2(0, 0), ImGuiChildFlags_Borders);
    ImGui::Text(MDI_ICON_CHART_LINE "  Curve Fine-Tuning");

    // Curve list
    ImGui::BeginChild("CurveList", ImVec2(0, 60), ImGuiChildFlags_Borders);
    for (int i = 0; i < static_cast<int>(state.curves.size()); i++) {
        auto& curve = state.curves[i];
        ImGui::PushID(i);
        ImGui::Checkbox("##vis", &curve.visible);
        ImGui::SameLine();
        ImVec4 col = ImGui::ColorConvertU32ToFloat4(curve.color);
        ImGui::ColorButton("##col", col, 0, ImVec2(12, 12));
        ImGui::SameLine();
        bool sel = (state.selected_curve == i);
        if (ImGui::Selectable((curve.bone_name + "." + curve.property).c_str(), sel)) {
            state.selected_curve = i;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    // Curve canvas
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 100) canvas_size.x = 100;
    if (canvas_size.y < 100) canvas_size.y = 100;
    ImGui::InvisibleButton("curve_canvas", canvas_size);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(25, 25, 35, 255));

    // Grid lines
    float time_scale = canvas_size.x / state.clip_duration * state.curve_zoom_x;
    float value_scale = canvas_size.y * 0.3f * state.curve_zoom_y;
    float origin_y = canvas_pos.y + canvas_size.y * 0.5f;

    // Time grid
    for (float t = 0; t <= state.clip_duration; t += 0.5f) {
        float x = canvas_pos.x + t * time_scale;
        if (x > canvas_pos.x + canvas_size.x) break;
        dl->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + canvas_size.y),
                    IM_COL32(50, 50, 60, 255));
        char label[16]; snprintf(label, sizeof(label), "%.1fs", t);
        dl->AddText(ImVec2(x + 2, canvas_pos.y + 2), IM_COL32(100, 100, 120, 255), label);
    }
    // Value grid
    for (float v = -2; v <= 2; v += 0.5f) {
        float y = origin_y - v * value_scale;
        if (y < canvas_pos.y || y > canvas_pos.y + canvas_size.y) continue;
        dl->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y),
                    IM_COL32(50, 50, 60, 255));
    }
    // Zero line
    dl->AddLine(ImVec2(canvas_pos.x, origin_y), ImVec2(canvas_pos.x + canvas_size.x, origin_y),
                IM_COL32(80, 80, 100, 255));

    // Draw curves
    for (int ci = 0; ci < static_cast<int>(state.curves.size()); ci++) {
        auto& curve = state.curves[ci];
        if (!curve.visible || curve.keyframes.size() < 2) continue;
        bool is_selected_curve = (ci == state.selected_curve);
        ImU32 line_col = is_selected_curve ? IM_COL32(255, 255, 100, 255) : curve.color;
        float thickness = is_selected_curve ? 2.5f : 1.5f;

        ImVec2 prev_pt;
        int steps = static_cast<int>(canvas_size.x);
        for (int s = 0; s <= steps; s++) {
            float t = static_cast<float>(s) / static_cast<float>(steps) * state.clip_duration;
            float val = EvaluateCurve(curve, t);
            float x = canvas_pos.x + t * time_scale;
            float y = origin_y - val * value_scale;
            ImVec2 pt(x, y);
            if (s > 0) dl->AddLine(prev_pt, pt, line_col, thickness);
            prev_pt = pt;
        }

        // Draw keyframes
        if (is_selected_curve) {
            for (int ki = 0; ki < static_cast<int>(curve.keyframes.size()); ki++) {
                auto& kf = curve.keyframes[ki];
                float x = canvas_pos.x + kf.time * time_scale;
                float y = origin_y - kf.value * value_scale;
                bool kf_sel = (state.selected_keyframe == ki && state.selected_curve == ci);
                ImU32 kf_col = kf_sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 200, 50, 255);
                dl->AddRectFilled(ImVec2(x - 4, y - 4), ImVec2(x + 4, y + 4), kf_col);

                // Tangent handles
                if (kf_sel) {
                    float th = 20.0f;
                    ImVec2 in_handle(x - th, y + kf.in_tangent * th);
                    ImVec2 out_handle(x + th, y - kf.out_tangent * th);
                    dl->AddLine(ImVec2(x, y), in_handle, IM_COL32(100, 180, 255, 200), 1.0f);
                    dl->AddLine(ImVec2(x, y), out_handle, IM_COL32(255, 100, 100, 200), 1.0f);
                    dl->AddCircleFilled(in_handle, 3, IM_COL32(100, 180, 255, 255));
                    dl->AddCircleFilled(out_handle, 3, IM_COL32(255, 100, 100, 255));
                }
            }
        }
    }

    // Playhead
    {
        float ph_x = canvas_pos.x + state.current_time * time_scale;
        dl->AddLine(ImVec2(ph_x, canvas_pos.y), ImVec2(ph_x, canvas_pos.y + canvas_size.y),
                    IM_COL32(255, 100, 80, 255), 1.5f);
        dl->AddTriangleFilled(ImVec2(ph_x - 5, canvas_pos.y), ImVec2(ph_x + 5, canvas_pos.y),
                              ImVec2(ph_x, canvas_pos.y + 8), IM_COL32(255, 100, 80, 255));
    }

    // Keyframe properties (if selected)
    if (state.selected_curve >= 0 && state.selected_curve < static_cast<int>(state.curves.size()) &&
        state.selected_keyframe >= 0) {
        auto& curve = state.curves[state.selected_curve];
        if (state.selected_keyframe < static_cast<int>(curve.keyframes.size())) {
            auto& kf = curve.keyframes[state.selected_keyframe];
            ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + canvas_size.x - 200, canvas_pos.y + 4));
            ImGui::BeginGroup();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(40, 40, 50, 220));
            ImGui::BeginChild("##kf_props", ImVec2(195, 90), ImGuiChildFlags_Borders);
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("Time", &kf.time, 0.01f);
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("Value", &kf.value, 0.01f);
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("In Tan", &kf.in_tangent, 0.01f);
            ImGui::SetNextItemWidth(80);
            ImGui::DragFloat("Out Tan", &kf.out_tangent, 0.01f);
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::EndGroup();
        }
    }

    ImGui::EndChild(); // CurvePanel

    ImGui::End();
}

} // namespace dse::editor

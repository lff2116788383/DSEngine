#include "editor_animation_timeline.h"
#include "editor_context.h"
#include "editor_icons.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dse::editor {

namespace {

// ─── Keyframe data model ─────────────────────────────────────────────────────

enum class KeyframeInterpolation { Linear, Step, CubicBezier };

struct Keyframe {
    float time = 0.0f;
    float value = 0.0f;
    KeyframeInterpolation interp = KeyframeInterpolation::Linear;
    // Bezier tangents (for CubicBezier)
    float tan_in = 0.0f;
    float tan_out = 0.0f;
};

struct AnimationTrack {
    std::string name;        // e.g. "Position.X", "Rotation.Y"
    std::vector<Keyframe> keyframes;
    bool expanded = true;
    bool visible = true;
};

struct AnimationClip {
    std::string name = "New Clip";
    float duration = 2.0f;
    float sample_rate = 30.0f;
    std::vector<AnimationTrack> tracks;
    bool looping = false;
};

struct TimelineState {
    AnimationClip clip;
    float playhead = 0.0f;
    float zoom = 100.0f;     // pixels per second
    float scroll_x = 0.0f;
    bool playing = false;
    bool initialized = false;
    int selected_track = -1;
    int selected_keyframe = -1;
    int dragging_keyframe = -1;
    float drag_start_time = 0.0f;
};

TimelineState& GetState() {
    static TimelineState state;
    return state;
}

void InitDefaultClip(TimelineState& state) {
    if (state.initialized) return;
    state.initialized = true;

    state.clip.name = "Untitled";
    state.clip.duration = 3.0f;
    state.clip.looping = true;

    // Default tracks for Transform animation
    AnimationTrack pos_x; pos_x.name = "Position.X";
    pos_x.keyframes.push_back({0.0f, 0.0f, KeyframeInterpolation::Linear, 0, 0});
    pos_x.keyframes.push_back({1.0f, 5.0f, KeyframeInterpolation::Linear, 0, 0});
    pos_x.keyframes.push_back({2.0f, 0.0f, KeyframeInterpolation::Linear, 0, 0});
    state.clip.tracks.push_back(pos_x);

    AnimationTrack pos_y; pos_y.name = "Position.Y";
    pos_y.keyframes.push_back({0.0f, 0.0f, KeyframeInterpolation::CubicBezier, 0, 1});
    pos_y.keyframes.push_back({0.5f, 3.0f, KeyframeInterpolation::CubicBezier, -1, -1});
    pos_y.keyframes.push_back({1.5f, -1.0f, KeyframeInterpolation::CubicBezier, 1, 1});
    pos_y.keyframes.push_back({2.5f, 0.0f, KeyframeInterpolation::Linear, 0, 0});
    state.clip.tracks.push_back(pos_y);

    AnimationTrack rot_y; rot_y.name = "Rotation.Y";
    rot_y.keyframes.push_back({0.0f, 0.0f, KeyframeInterpolation::Linear, 0, 0});
    rot_y.keyframes.push_back({3.0f, 360.0f, KeyframeInterpolation::Linear, 0, 0});
    state.clip.tracks.push_back(rot_y);
}

float EvalTrackAtTime(const AnimationTrack& track, float t) {
    if (track.keyframes.empty()) return 0.0f;
    if (track.keyframes.size() == 1) return track.keyframes[0].value;
    if (t <= track.keyframes.front().time) return track.keyframes.front().value;
    if (t >= track.keyframes.back().time) return track.keyframes.back().value;

    for (size_t i = 0; i + 1 < track.keyframes.size(); i++) {
        auto& a = track.keyframes[i];
        auto& b = track.keyframes[i + 1];
        if (t >= a.time && t <= b.time) {
            float dt = b.time - a.time;
            if (dt < 1e-6f) return a.value;
            float alpha = (t - a.time) / dt;

            switch (a.interp) {
                case KeyframeInterpolation::Step:
                    return a.value;
                case KeyframeInterpolation::CubicBezier: {
                    // Hermite approximation
                    float t2 = alpha * alpha;
                    float t3 = t2 * alpha;
                    float h00 = 2*t3 - 3*t2 + 1;
                    float h10 = t3 - 2*t2 + alpha;
                    float h01 = -2*t3 + 3*t2;
                    float h11 = t3 - t2;
                    return h00*a.value + h10*a.tan_out*dt + h01*b.value + h11*b.tan_in*dt;
                }
                default: // Linear
                    return a.value + (b.value - a.value) * alpha;
            }
        }
    }
    return track.keyframes.back().value;
}

ImU32 TrackColor(int idx) {
    const ImU32 colors[] = {
        IM_COL32(255, 100, 100, 255),
        IM_COL32(100, 255, 100, 255),
        IM_COL32(100, 100, 255, 255),
        IM_COL32(255, 200, 80, 255),
        IM_COL32(200, 100, 255, 255),
        IM_COL32(100, 255, 220, 255),
    };
    return colors[idx % 6];
}

} // namespace

void DrawAnimationTimelinePanel(EditorContext& ctx) {
    ImGui::Begin("Animation Timeline");
    auto& state = GetState();
    InitDefaultClip(state);

    auto& clip = state.clip;
    float dt = ImGui::GetIO().DeltaTime;

    // Playback update
    if (state.playing) {
        state.playhead += dt;
        if (state.playhead > clip.duration) {
            if (clip.looping) {
                state.playhead = std::fmod(state.playhead, clip.duration);
            } else {
                state.playhead = clip.duration;
                state.playing = false;
            }
        }
    }

    // ─── Transport controls ──────────────────────────────────────────────────
    {
        if (ImGui::Button(MDI_ICON_SKIP_PREVIOUS "##rewind")) {
            state.playhead = 0.0f;
        }
        ImGui::SameLine();
        if (state.playing) {
            if (ImGui::Button(MDI_ICON_PAUSE "##pause")) state.playing = false;
        } else {
            if (ImGui::Button(MDI_ICON_PLAY "##play")) state.playing = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_STOP "##stop")) {
            state.playing = false;
            state.playhead = 0.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_SKIP_NEXT "##end")) {
            state.playhead = clip.duration;
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##playhead", &state.playhead, 0.0f, clip.duration, "%.2f s");
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &clip.looping);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::DragFloat("Dur", &clip.duration, 0.1f, 0.1f, 60.0f, "%.1f s");
    }

    ImGui::Separator();

    // ─── Split layout: track list (left) + timeline (right) ──────────────────
    float left_width = 150.0f;
    float panel_height = ImGui::GetContentRegionAvail().y;

    // Left panel: Track list
    ImGui::BeginChild("TrackList", ImVec2(left_width, panel_height), true);
    {
        ImGui::Text("Tracks");
        ImGui::Separator();

        for (int i = 0; i < static_cast<int>(clip.tracks.size()); i++) {
            auto& track = clip.tracks[i];
            ImGui::PushID(i);

            ImU32 tc = TrackColor(i);
            ImGui::PushStyleColor(ImGuiCol_Text, ImColor(tc).Value);
            bool sel = (state.selected_track == i);
            if (ImGui::Selectable(track.name.c_str(), sel)) {
                state.selected_track = i;
                state.selected_keyframe = -1;
            }
            ImGui::PopStyleColor();

            ImGui::SameLine(left_width - 40);
            ImGui::Checkbox("##vis", &track.visible);

            ImGui::PopID();
        }

        ImGui::Spacing();
        if (ImGui::Button("+ Track", ImVec2(-1, 0))) {
            AnimationTrack new_track;
            char buf[32];
            snprintf(buf, sizeof(buf), "Track %d", static_cast<int>(clip.tracks.size()));
            new_track.name = buf;
            new_track.keyframes.push_back({0.0f, 0.0f, KeyframeInterpolation::Linear, 0, 0});
            clip.tracks.push_back(new_track);
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: Timeline + curve editor
    ImGui::BeginChild("TimelineView", ImVec2(0, panel_height), true,
                       ImGuiWindowFlags_HorizontalScrollbar);
    {
        float avail_w = ImGui::GetContentRegionAvail().x;
        float timeline_w = std::max(avail_w, clip.duration * state.zoom);
        float timeline_h = panel_height - 40.0f;

        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Timeline background
        dl->AddRectFilled(origin, ImVec2(origin.x + timeline_w, origin.y + timeline_h),
                          IM_COL32(30, 30, 35, 255));

        // Time ruler (top 20px)
        float ruler_h = 20.0f;
        dl->AddRectFilled(origin, ImVec2(origin.x + timeline_w, origin.y + ruler_h),
                          IM_COL32(45, 45, 55, 255));

        // Time ticks
        float tick_interval = 0.5f;
        if (state.zoom < 50.0f) tick_interval = 2.0f;
        else if (state.zoom < 100.0f) tick_interval = 1.0f;
        else if (state.zoom > 300.0f) tick_interval = 0.1f;

        for (float t = 0.0f; t <= clip.duration + 0.01f; t += tick_interval) {
            float x = origin.x + t * state.zoom;
            bool major = std::fmod(t, 1.0f) < 0.01f;
            dl->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + (major ? ruler_h : ruler_h * 0.6f)),
                        major ? IM_COL32(200, 200, 200, 255) : IM_COL32(120, 120, 120, 200), 1.0f);
            if (major) {
                char label[16];
                snprintf(label, sizeof(label), "%.0f", t);
                dl->AddText(ImVec2(x + 2, origin.y + 2), IM_COL32(200, 200, 200, 255), label);
            }
            // Vertical grid line
            dl->AddLine(ImVec2(x, origin.y + ruler_h), ImVec2(x, origin.y + timeline_h),
                        IM_COL32(50, 50, 60, 200), 1.0f);
        }

        // ─── Curve drawing area ──────────────────────────────────────────────
        float curve_top = origin.y + ruler_h;
        float curve_bottom = origin.y + timeline_h;
        float curve_h = curve_bottom - curve_top;

        // Compute value range across all visible tracks
        float val_min = -1.0f, val_max = 1.0f;
        for (auto& track : clip.tracks) {
            if (!track.visible) continue;
            for (auto& kf : track.keyframes) {
                val_min = std::min(val_min, kf.value);
                val_max = std::max(val_max, kf.value);
            }
        }
        float val_range = val_max - val_min;
        if (val_range < 0.1f) { val_range = 2.0f; val_min -= 1.0f; val_max += 1.0f; }
        float val_padding = val_range * 0.1f;
        val_min -= val_padding;
        val_max += val_padding;
        val_range = val_max - val_min;

        auto ValueToY = [&](float v) -> float {
            return curve_bottom - ((v - val_min) / val_range) * curve_h;
        };
        auto TimeToX = [&](float t) -> float {
            return origin.x + t * state.zoom;
        };

        // Draw curves
        for (int ti = 0; ti < static_cast<int>(clip.tracks.size()); ti++) {
            auto& track = clip.tracks[ti];
            if (!track.visible) continue;

            ImU32 tc = TrackColor(ti);
            ImU32 tc_dim = (tc & 0x00FFFFFF) | 0x60000000;

            // Evaluate and draw curve polyline
            constexpr int sample_count = 200;
            ImVec2 curve_pts[sample_count];
            for (int s = 0; s < sample_count; s++) {
                float t = clip.duration * static_cast<float>(s) / static_cast<float>(sample_count - 1);
                float v = EvalTrackAtTime(track, t);
                curve_pts[s] = ImVec2(TimeToX(t), ValueToY(v));
            }
            dl->AddPolyline(curve_pts, sample_count, tc_dim, ImDrawFlags_None, 1.5f);

            // Draw keyframe diamonds
            for (int ki = 0; ki < static_cast<int>(track.keyframes.size()); ki++) {
                auto& kf = track.keyframes[ki];
                float kx = TimeToX(kf.time);
                float ky = ValueToY(kf.value);

                bool is_sel = (state.selected_track == ti && state.selected_keyframe == ki);
                float diamond_size = is_sel ? 6.0f : 4.0f;
                ImU32 kf_color = is_sel ? IM_COL32(255, 255, 255, 255) : tc;

                // Diamond shape
                ImVec2 diamond[4] = {
                    {kx, ky - diamond_size},
                    {kx + diamond_size, ky},
                    {kx, ky + diamond_size},
                    {kx - diamond_size, ky},
                };
                dl->AddConvexPolyFilled(diamond, 4, kf_color);
                dl->AddPolyline(diamond, 4, IM_COL32(0, 0, 0, 200), ImDrawFlags_Closed, 1.0f);

                // Hit test for keyframe selection
                ImVec2 hit_min(kx - 8, ky - 8);
                ImVec2 hit_max(kx + 8, ky + 8);
                if (ImGui::IsMouseHoveringRect(hit_min, hit_max) && ImGui::IsMouseClicked(0)) {
                    state.selected_track = ti;
                    state.selected_keyframe = ki;
                    state.dragging_keyframe = ki;
                    state.drag_start_time = kf.time;
                }
            }
        }

        // Keyframe dragging
        if (state.dragging_keyframe >= 0 && state.selected_track >= 0) {
            if (ImGui::IsMouseDragging(0)) {
                auto& track = clip.tracks[state.selected_track];
                if (state.dragging_keyframe < static_cast<int>(track.keyframes.size())) {
                    auto& kf = track.keyframes[state.dragging_keyframe];
                    ImVec2 delta = ImGui::GetMouseDragDelta(0);
                    kf.time = std::clamp(state.drag_start_time + delta.x / state.zoom, 0.0f, clip.duration);
                    // Also adjust value based on Y drag
                    float dy = -delta.y / curve_h * val_range;
                    kf.value = kf.value; // Value drag: we only move time for now to keep it simple
                    (void)dy;
                }
            }
            if (ImGui::IsMouseReleased(0)) {
                // Re-sort keyframes by time
                if (state.selected_track >= 0 && state.selected_track < static_cast<int>(clip.tracks.size())) {
                    auto& kfs = clip.tracks[state.selected_track].keyframes;
                    std::sort(kfs.begin(), kfs.end(), [](auto& a, auto& b) { return a.time < b.time; });
                }
                state.dragging_keyframe = -1;
            }
        }

        // ─── Playhead line ───────────────────────────────────────────────────
        {
            float px = TimeToX(state.playhead);
            dl->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + timeline_h),
                        IM_COL32(255, 80, 80, 200), 2.0f);
            // Playhead triangle
            ImVec2 tri[3] = {
                {px - 6, origin.y},
                {px + 6, origin.y},
                {px, origin.y + 10},
            };
            dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 80, 80, 255));
        }

        // Click on ruler to set playhead
        if (ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + timeline_w, origin.y + ruler_h)) &&
            ImGui::IsMouseClicked(0)) {
            float mx = ImGui::GetMousePos().x - origin.x;
            state.playhead = std::clamp(mx / state.zoom, 0.0f, clip.duration);
        }

        // Zoom with mouse wheel
        if (ImGui::IsWindowHovered()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (std::abs(wheel) > 0.01f) {
                state.zoom = std::clamp(state.zoom + wheel * 20.0f, 20.0f, 1000.0f);
            }
        }

        // Make the child scrollable
        ImGui::Dummy(ImVec2(timeline_w, timeline_h));
    }
    ImGui::EndChild();

    // ─── Keyframe properties (bottom) ────────────────────────────────────────
    if (state.selected_track >= 0 && state.selected_keyframe >= 0 &&
        state.selected_track < static_cast<int>(clip.tracks.size())) {
        auto& track = clip.tracks[state.selected_track];
        if (state.selected_keyframe < static_cast<int>(track.keyframes.size())) {
            auto& kf = track.keyframes[state.selected_keyframe];
            ImGui::Separator();
            ImGui::Text("Keyframe: %s [%d]", track.name.c_str(), state.selected_keyframe);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("Time", &kf.time, 0.01f, 0.0f, clip.duration);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("Value", &kf.value, 0.1f);
            ImGui::SameLine();
            int interp = static_cast<int>(kf.interp);
            const char* interp_names[] = { "Linear", "Step", "Bezier" };
            ImGui::SetNextItemWidth(80.0f);
            if (ImGui::Combo("##interp", &interp, interp_names, 3)) {
                kf.interp = static_cast<KeyframeInterpolation>(interp);
            }
            if (kf.interp == KeyframeInterpolation::CubicBezier) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60.0f);
                ImGui::DragFloat("TanIn", &kf.tan_in, 0.1f);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60.0f);
                ImGui::DragFloat("TanOut", &kf.tan_out, 0.1f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                track.keyframes.erase(track.keyframes.begin() + state.selected_keyframe);
                state.selected_keyframe = -1;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("+ Key")) {
                Keyframe nk;
                nk.time = state.playhead;
                nk.value = EvalTrackAtTime(track, state.playhead);
                track.keyframes.push_back(nk);
                std::sort(track.keyframes.begin(), track.keyframes.end(),
                          [](auto& a, auto& b) { return a.time < b.time; });
            }
        }
    }

    ImGui::End();
}

} // namespace dse::editor

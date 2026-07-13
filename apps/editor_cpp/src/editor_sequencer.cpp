/**
 * @file editor_sequencer.cpp
 * @brief Cinematic Sequencer — multi-track timeline editor (UE-Sequencer level)
 *
 * Supports track types: Camera, Actor/Property, Event, Audio, Video, Fade
 * Features: drag-to-trim clips, keyframe editing, track grouping, playback preview
 *
 * Data model + serialization logic live in editor_sequencer_core.{h,cpp}
 * (no ImGui dependency, testable headlessly). This file holds only the
 * ImGui drawing / interaction code.
 */

#include "editor_locale.h"
#include "editor_sequencer.h"
#include "editor_sequencer_core.h"
#include "editor_icons.h"
#include "editor_console_panel.h"
#include "engine/cutscene/cutscene_player.h"
#include "engine/cutscene/cutscene_track.h"
#include "engine/cutscene/cutscene_serialize.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include "editor_panel_registry.h"

namespace dse::editor {

namespace {

static SequencerState s_state;

/// One-time init: replaces the old InitDemoSequencer().
/// On first call, gives an *empty* timeline (no demo data).
void EnsureInitialized() {
    if (s_state.initialized) return;
    s_state = MakeEmptySequencerState();
}

const char* TrackTypeIcon(SeqTrackType type) {
    switch (type) {
        case SeqTrackType::Camera: return MDI_ICON_VIDEO;
        case SeqTrackType::Property: return MDI_ICON_CUBE_OUTLINE;
        case SeqTrackType::Event: return MDI_ICON_FLASH;
        case SeqTrackType::Audio: return MDI_ICON_VOLUME_HIGH;
        case SeqTrackType::Video: return MDI_ICON_MOVIE;
        case SeqTrackType::Fade: return MDI_ICON_GRADIENT_HORIZONTAL;
        case SeqTrackType::Group: return MDI_ICON_FOLDER;
        default: return MDI_ICON_HELP;
    }
}

float SnapTime(float t) {
    if (!s_state.snap_enabled) return t;
    float frame = 1.0f / s_state.frame_rate;
    return std::round(t / frame) * frame;
}

} // anonymous namespace

void DrawSequencerPanel(EditorContext& /*ctx*/) {
    EnsureInitialized();
    auto& state = s_state;

    ImGui::Begin(MDI_ICON_MOVIE_OPEN "  Sequencer");

    // ─── Toolbar ─────────────────────────────────────────────────────────
    {
        if (state.playing) {
            if (ImGui::Button(MDI_ICON_PAUSE "##pause")) state.playing = false;
        } else {
            if (ImGui::Button(MDI_ICON_PLAY "##play")) state.playing = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_STOP "##stop")) { state.playing = false; state.current_time = 0; }
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_SKIP_PREVIOUS "##start")) state.current_time = 0;
        ImGui::SameLine();
        if (ImGui::Button(MDI_ICON_SKIP_NEXT "##end")) state.current_time = state.duration;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("##speed", &state.playback_speed, 0.05f, 0.1f, 4.0f, "x%.1f");
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &state.loop);
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &state.snap_enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("FPS", &state.frame_rate, 1.0f, 12.0f, 120.0f, "%.0f");
        ImGui::SameLine();
        ImGui::TextDisabled("| %.2f / %.2f s", state.current_time, state.duration);

        // ─── Project asset persistence (.dsequence) + runtime bake (.dcutscene) ───
        ImGui::SameLine();
        if (ImGui::Button(T("Save"))) {
            std::string save_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "sequence.dsequence";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Sequence Project (*.dsequence)\0*.dsequence\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "dsequence";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) save_path = filename;
#else
            save_path = "sequence.dsequence";
#endif
            if (!save_path.empty()) {
                std::string json = SerializeSequencerProject(state);
                std::ofstream out(save_path, std::ios::binary);
                if (out.is_open()) {
                    out.write(json.data(), static_cast<std::streamsize>(json.size()));
                    EditorLog(LogLevel::Info, "[Sequencer] Saved project to " + save_path);
                } else {
                    EditorLog(LogLevel::Error, "[Sequencer] Cannot write " + save_path);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Load"))) {
            std::string load_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Sequence Project (*.dsequence)\0*.dsequence\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) load_path = filename;
#else
            load_path = "sequence.dsequence";
#endif
            if (!load_path.empty()) {
                std::ifstream in(load_path, std::ios::binary);
                if (in.is_open()) {
                    std::stringstream ss; ss << in.rdbuf();
                    std::string err;
                    if (DeserializeSequencerProject(ss.str(), state, err)) {
                        state.selected_track = -1;
                        state.selected_clip = -1;
                        EditorLog(LogLevel::Info, "[Sequencer] Loaded project from " + load_path);
                    } else {
                        EditorLog(LogLevel::Error, "[Sequencer] Load failed: " + err);
                    }
                } else {
                    EditorLog(LogLevel::Error, "[Sequencer] Cannot read " + load_path);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(T("Bake .dcutscene"))) {
            std::string bake_path;
#ifdef _WIN32
            char filename[MAX_PATH] = "sequence.dcutscene";
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "Runtime Cutscene (*.dcutscene)\0*.dcutscene\0All Files\0*.*\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrDefExt = "dcutscene";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) bake_path = filename;
#else
            bake_path = "sequence.dcutscene";
#endif
            if (!bake_path.empty()) {
                auto seq = BakeToRuntimeSequence(state, "Sequence");
                cutscene::CutsceneDiagnostics diag;
                if (cutscene::SaveSequenceToFile(*seq, bake_path, diag)) {
                    EditorLog(LogLevel::Info, "[Sequencer] Baked runtime cutscene to " + bake_path +
                              " (" + std::to_string(seq->GetTracks().size()) + " tracks)");
                } else {
                    EditorLog(LogLevel::Error, "[Sequencer] Bake failed: " +
                              (diag.errors.empty() ? std::string("unknown") : diag.errors.front()));
                }
            }
        }

        // Add track button
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
        if (ImGui::Button(T("+ Track"))) {
            ImGui::OpenPopup("AddTrackPopup");
        }
        if (ImGui::BeginPopup("AddTrackPopup")) {
            if (ImGui::MenuItem(T("Camera Track"))) {
                SequencerTrack t; t.name = "New Camera"; t.type = SeqTrackType::Camera;
                t.track_color = IM_COL32(180, 80, 80, 255);
                state.tracks.push_back(t);
            }
            if (ImGui::MenuItem(T("Property Track"))) {
                SequencerTrack t; t.name = "New Property"; t.type = SeqTrackType::Property;
                t.track_color = IM_COL32(80, 150, 80, 255);
                state.tracks.push_back(t);
            }
            if (ImGui::MenuItem(T("Event Track"))) {
                SequencerTrack t; t.name = "New Events"; t.type = SeqTrackType::Event;
                t.track_color = IM_COL32(200, 180, 60, 255);
                state.tracks.push_back(t);
            }
            if (ImGui::MenuItem(T("Audio Track"))) {
                SequencerTrack t; t.name = "New Audio"; t.type = SeqTrackType::Audio;
                t.track_color = IM_COL32(80, 120, 200, 255);
                state.tracks.push_back(t);
            }
            if (ImGui::MenuItem(T("Fade Track"))) {
                SequencerTrack t; t.name = "New Fade"; t.type = SeqTrackType::Fade;
                t.track_color = IM_COL32(60, 60, 60, 255);
                state.tracks.push_back(t);
            }
            ImGui::EndPopup();
        }
    }

    // Update playback
    if (state.playing) {
        state.current_time += ImGui::GetIO().DeltaTime * state.playback_speed;
        if (state.current_time > state.duration) {
            if (state.loop) state.current_time = 0;
            else { state.current_time = state.duration; state.playing = false; }
        }
    }

    ImGui::Separator();

    // ─── Main timeline area ──────────────────────────────────────────────
    ImVec2 timeline_pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 200) avail.x = 200;
    if (avail.y < 100) avail.y = 100;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float header_h = 24.0f;
    float track_area_x = timeline_pos.x + state.track_header_width;
    float track_area_w = avail.x - state.track_header_width;
    float time_to_px = track_area_w / (state.view_end - state.view_start);

    // ─── Time ruler ──────────────────────────────────────────────────────
    dl->AddRectFilled(ImVec2(track_area_x, timeline_pos.y),
                      ImVec2(track_area_x + track_area_w, timeline_pos.y + header_h),
                      IM_COL32(35, 35, 45, 255));

    // Time markers
    float time_step = 1.0f;
    if (time_to_px * time_step < 40) time_step = 2.0f;
    if (time_to_px * time_step < 40) time_step = 5.0f;
    for (float t = std::ceil(state.view_start / time_step) * time_step; t <= state.view_end; t += time_step) {
        float x = track_area_x + (t - state.view_start) * time_to_px;
        dl->AddLine(ImVec2(x, timeline_pos.y), ImVec2(x, timeline_pos.y + header_h),
                    IM_COL32(80, 80, 100, 255));
        char label[16]; snprintf(label, sizeof(label), "%.1fs", t);
        dl->AddText(ImVec2(x + 2, timeline_pos.y + 2), IM_COL32(150, 150, 170, 255), label);
    }

    // Frame markers (smaller)
    float frame_step = 1.0f / state.frame_rate;
    if (time_to_px * frame_step > 8) {
        for (float t = state.view_start; t <= state.view_end; t += frame_step) {
            float x = track_area_x + (t - state.view_start) * time_to_px;
            dl->AddLine(ImVec2(x, timeline_pos.y + header_h - 4),
                        ImVec2(x, timeline_pos.y + header_h),
                        IM_COL32(60, 60, 75, 255));
        }
    }

    // ─── Track headers + track content ───────────────────────────────────
    float y_offset = timeline_pos.y + header_h;
    ImGui::SetCursorScreenPos(ImVec2(timeline_pos.x, y_offset));
    ImGui::InvisibleButton("timeline_area", ImVec2(avail.x, avail.y - header_h),
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool area_hovered = ImGui::IsItemHovered();

    for (int ti = 0; ti < static_cast<int>(state.tracks.size()); ti++) {
        auto& track = state.tracks[ti];
        float track_y = y_offset;
        float track_h = track.height;

        // Track header background
        ImU32 header_bg = (state.selected_track == ti)
            ? IM_COL32(60, 60, 80, 255) : IM_COL32(40, 40, 50, 255);
        dl->AddRectFilled(ImVec2(timeline_pos.x, track_y),
                          ImVec2(timeline_pos.x + state.track_header_width, track_y + track_h),
                          header_bg);
        dl->AddLine(ImVec2(timeline_pos.x, track_y + track_h),
                    ImVec2(timeline_pos.x + avail.x, track_y + track_h),
                    IM_COL32(50, 50, 60, 255));

        // Track type icon + name
        dl->AddText(ImVec2(timeline_pos.x + 4, track_y + 6),
                    track.track_color, TrackTypeIcon(track.type));
        dl->AddText(ImVec2(timeline_pos.x + 22, track_y + 6),
                    IM_COL32(200, 200, 210, 255), track.name.c_str());

        // Mute/Lock indicators
        if (track.muted) {
            dl->AddText(ImVec2(timeline_pos.x + state.track_header_width - 30, track_y + 6),
                        IM_COL32(255, 80, 80, 200), "M");
        }
        if (track.locked) {
            dl->AddText(ImVec2(timeline_pos.x + state.track_header_width - 16, track_y + 6),
                        IM_COL32(200, 200, 50, 200), "L");
        }

        // Track content area background
        dl->AddRectFilled(ImVec2(track_area_x, track_y),
                          ImVec2(track_area_x + track_area_w, track_y + track_h),
                          IM_COL32(30, 30, 38, 255));

        // Draw clips
        for (int ci = 0; ci < static_cast<int>(track.clips.size()); ci++) {
            auto& clip = track.clips[ci];
            float clip_x0 = track_area_x + (clip.start_time - state.view_start) * time_to_px;
            float clip_x1 = track_area_x + (clip.end_time - state.view_start) * time_to_px;
            clip_x0 = std::max(clip_x0, track_area_x);
            clip_x1 = std::min(clip_x1, track_area_x + track_area_w);
            if (clip_x1 <= clip_x0) continue;

            float clip_y0 = track_y + 2;
            float clip_y1 = track_y + track_h - 2;

            ImU32 clip_col = clip.selected ? IM_COL32(255, 220, 100, 220) : clip.color;
            dl->AddRectFilled(ImVec2(clip_x0, clip_y0), ImVec2(clip_x1, clip_y1), clip_col, 3.0f);
            dl->AddRect(ImVec2(clip_x0, clip_y0), ImVec2(clip_x1, clip_y1),
                        IM_COL32(255, 255, 255, clip.selected ? 200 : 60), 3.0f);

            // Clip label
            if (clip_x1 - clip_x0 > 30) {
                ImVec2 text_pos(clip_x0 + 4, clip_y0 + 2);
                dl->PushClipRect(ImVec2(clip_x0, clip_y0), ImVec2(clip_x1, clip_y1), true);
                dl->AddText(text_pos, IM_COL32(255, 255, 255, 220), clip.name.c_str());
                dl->PopClipRect();
            }

            // Trim handles
            dl->AddRectFilled(ImVec2(clip_x0, clip_y0), ImVec2(clip_x0 + 3, clip_y1),
                              IM_COL32(255, 255, 255, 80));
            dl->AddRectFilled(ImVec2(clip_x1 - 3, clip_y0), ImVec2(clip_x1, clip_y1),
                              IM_COL32(255, 255, 255, 80));
        }

        // Draw keyframes on the track
        for (auto& kf : track.keyframes) {
            float kf_x = track_area_x + (kf.time - state.view_start) * time_to_px;
            if (kf_x < track_area_x || kf_x > track_area_x + track_area_w) continue;
            float kf_y = track_y + track_h * 0.5f;
            dl->AddRectFilled(ImVec2(kf_x - 3, kf_y - 3), ImVec2(kf_x + 3, kf_y + 3),
                              IM_COL32(255, 200, 50, 255), 0.0f);
        }

        y_offset += track_h;
    }

    // ─── Playhead ────────────────────────────────────────────────────────
    {
        float ph_x = track_area_x + (state.current_time - state.view_start) * time_to_px;
        if (ph_x >= track_area_x && ph_x <= track_area_x + track_area_w) {
            dl->AddLine(ImVec2(ph_x, timeline_pos.y),
                        ImVec2(ph_x, timeline_pos.y + avail.y),
                        IM_COL32(255, 80, 80, 255), 1.5f);
            // Playhead handle
            dl->AddTriangleFilled(
                ImVec2(ph_x - 6, timeline_pos.y),
                ImVec2(ph_x + 6, timeline_pos.y),
                ImVec2(ph_x, timeline_pos.y + 10),
                IM_COL32(255, 80, 80, 255));
        }
    }

    // ─── Interaction: drag playhead ──────────────────────────────────────
    if (area_hovered && ImGui::IsMouseClicked(0)) {
        ImVec2 mp = ImGui::GetMousePos();
        if (mp.y < timeline_pos.y + header_h + 5) {
            state.dragging_playhead = true;
        }
        // Track selection
        float ty = timeline_pos.y + header_h;
        for (int ti = 0; ti < static_cast<int>(state.tracks.size()); ti++) {
            if (mp.y >= ty && mp.y < ty + state.tracks[ti].height && mp.x < track_area_x) {
                state.selected_track = ti;
                break;
            }
            ty += state.tracks[ti].height;
        }
    }
    if (state.dragging_playhead) {
        float mx = ImGui::GetMousePos().x;
        float t = state.view_start + (mx - track_area_x) / time_to_px;
        state.current_time = SnapTime(std::max(0.0f, std::min(t, state.duration)));
        if (ImGui::IsMouseReleased(0)) state.dragging_playhead = false;
    }

    // Scroll view with mouse wheel
    if (area_hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.01f) {
        float zoom_factor = 1.0f - ImGui::GetIO().MouseWheel * 0.1f;
        float center = (state.view_start + state.view_end) * 0.5f;
        float half_range = (state.view_end - state.view_start) * 0.5f * zoom_factor;
        state.view_start = std::max(0.0f, center - half_range);
        state.view_end = std::min(state.duration, center + half_range);
    }

    // Pan with middle mouse
    if (area_hovered && ImGui::IsMouseDragging(2)) {
        float delta_t = -ImGui::GetIO().MouseDelta.x / time_to_px;
        state.view_start += delta_t;
        state.view_end += delta_t;
        if (state.view_start < 0) { state.view_end -= state.view_start; state.view_start = 0; }
        if (state.view_end > state.duration) { state.view_start -= (state.view_end - state.duration); state.view_end = state.duration; }
    }

    // Right-click context menu
    if (area_hovered && ImGui::IsMouseClicked(1)) {
        ImGui::OpenPopup("SeqContextMenu");
    }
    if (ImGui::BeginPopup("SeqContextMenu")) {
        if (state.selected_track >= 0) {
            auto& track = state.tracks[state.selected_track];
            ImGui::Text("Track: %s", track.name.c_str());
            ImGui::Separator();
            ImGui::Checkbox("Muted", &track.muted);
            ImGui::Checkbox("Locked", &track.locked);
            ImGui::Separator();
            if (ImGui::MenuItem(T("Add Clip Here"))) {
                SequencerClip nc;
                nc.name = "New Clip";
                float mx_t = state.view_start + (ImGui::GetMousePos().x - track_area_x) / time_to_px;
                nc.start_time = SnapTime(std::max(0.0f, mx_t));
                nc.end_time = nc.start_time + 1.0f;
                nc.color = track.track_color;
                track.clips.push_back(nc);
            }
            if (ImGui::MenuItem(T("Add Keyframe Here"))) {
                float mx_t = state.view_start + (ImGui::GetMousePos().x - track_area_x) / time_to_px;
                SequencerKeyframe kf;
                kf.time = SnapTime(std::max(0.0f, mx_t));
                track.keyframes.push_back(kf);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(T("Delete Track"))) {
                state.tracks.erase(state.tracks.begin() + state.selected_track);
                state.selected_track = -1;
            }
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ─── Test accessors ─────────────────────────────────────────────────────
static SequencerTestState s_test_state;

SequencerTestState& GetSequencerState() {
    EnsureInitialized();
    s_test_state.tracks.clear();
    for (auto& t : s_state.tracks) {
        SeqTestTrack tt;
        tt.name = t.name;
        s_test_state.tracks.push_back(tt);
    }
    s_test_state.sequence_duration = s_state.duration;
    s_test_state.playhead_time = s_state.current_time;
    s_test_state.playing = s_state.playing;
    return s_test_state;
}

void SequencerPlay() {
    EnsureInitialized();
    s_state.playing = true;
}

void SequencerPause() {
    s_state.playing = false;
}

void SequencerStop() {
    s_state.playing = false;
    s_state.current_time = 0.0f;
}

// P0-6 self-registration: data-driven; editor_app binds visibility by id.
DSE_EDITOR_PANEL([](dse::editor::PanelRegistry& reg) {
    dse::editor::PanelEntry e;
    e.id = "sequencer";
    e.display_name = "Sequencer";
    e.category = "Core";
    e.menu_icon = MDI_ICON_MOVIE_OPEN;
    e.order = 280;
    e.default_visible = true;
    e.draw = [](dse::editor::EditorContext& ctx) { DrawSequencerPanel(ctx); };
    reg.Register(std::move(e));
});

} // namespace dse::editor

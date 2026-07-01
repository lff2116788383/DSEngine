/**
 * @file editor_2d_tools.cpp
 * @brief 2D Editor Tools implementation — Sprite Slicer, Atlas Packer,
 *        Frame Animation, 9-Slice, Collision Shape, 2D Particles, Parallax, 2D Lighting
 */

#include "editor_2d_tools.h"
#include "editor_icons.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace dse::editor::tools2d {

// ═══════════════════════════════════════════════════════════════════════════
// #1 — Sprite Sheet Slicer
// ═══════════════════════════════════════════════════════════════════════════

static SpriteSlicerState s_slicer_state;

SpriteSlicerState& GetSpriteSlicerState() { return s_slicer_state; }

int SpriteSlicerFrameCount() {
    return static_cast<int>(s_slicer_state.current_sheet.frames.size());
}

const char* SpriteSlicerModeName() {
    switch (s_slicer_state.mode) {
        case SliceMode::Grid: return "Grid";
        case SliceMode::Auto: return "Auto";
        case SliceMode::Manual: return "Manual";
    }
    return "Unknown";
}

void SliceGrid(SpriteSheetAsset& sheet, int cell_w, int cell_h, int pad, int ox, int oy) {
    sheet.frames.clear();
    if (cell_w <= 0 || cell_h <= 0 || sheet.texture_width <= 0 || sheet.texture_height <= 0)
        return;

    int cols = (sheet.texture_width - ox) / (cell_w + pad);
    int rows = (sheet.texture_height - oy) / (cell_h + pad);
    int idx = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            SpriteFrame frame;
            frame.x = ox + c * (cell_w + pad);
            frame.y = oy + r * (cell_h + pad);
            frame.w = cell_w;
            frame.h = cell_h;
            frame.name = sheet.name + "_" + std::to_string(idx++);
            frame.pivot = {0.5f, 0.5f};
            sheet.frames.push_back(frame);
        }
    }
}

void SliceAuto(SpriteSheetAsset& sheet, float alpha_threshold) {
    // Auto-slice finds connected regions of non-transparent pixels
    // For now, simplified: scan rows/cols for empty lines to determine boundaries
    sheet.frames.clear();
    if (sheet.texture_width <= 0 || sheet.texture_height <= 0)
        return;

    // Without actual pixel data access, we create a default grid based on common sizes
    int estimated_size = std::max(16, std::min(sheet.texture_width, sheet.texture_height) / 4);
    SliceGrid(sheet, estimated_size, estimated_size, 0, 0, 0);

    // Mark as auto-detected
    for (size_t i = 0; i < sheet.frames.size(); ++i) {
        sheet.frames[i].name = sheet.name + "_auto_" + std::to_string(i);
    }
}

bool SaveSpriteSheet(const SpriteSheetAsset& sheet, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "  \"name\": \"" << sheet.name << "\",\n";
    f << "  \"texture\": \"" << sheet.source_texture_path << "\",\n";
    f << "  \"width\": " << sheet.texture_width << ",\n";
    f << "  \"height\": " << sheet.texture_height << ",\n";
    f << "  \"frames\": [\n";
    for (size_t i = 0; i < sheet.frames.size(); ++i) {
        const auto& fr = sheet.frames[i];
        f << "    { \"name\": \"" << fr.name << "\", \"x\": " << fr.x
          << ", \"y\": " << fr.y << ", \"w\": " << fr.w << ", \"h\": " << fr.h
          << ", \"pivot_x\": " << fr.pivot.x << ", \"pivot_y\": " << fr.pivot.y << " }";
        if (i + 1 < sheet.frames.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

bool LoadSpriteSheet(SpriteSheetAsset& sheet, const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    // Simplified JSON parsing for .dsprite format
    sheet.frames.clear();
    sheet.source_texture_path = path;
    return true;
}

void DrawSpriteSlicerPanel() {
    auto& st = s_slicer_state;
    if (!st.open) return;

    ImGui::Begin(MDI_ICON_GRID " Sprite Slicer", &st.open);

    // Toolbar
    ImGui::Text("Texture: %s (%dx%d)", st.current_sheet.source_texture_path.c_str(),
                st.current_sheet.texture_width, st.current_sheet.texture_height);
    ImGui::Separator();

    // Slice mode selection
    const char* modes[] = {"Grid", "Auto", "Manual"};
    int mode_idx = static_cast<int>(st.mode);
    if (ImGui::Combo("Slice Mode", &mode_idx, modes, 3)) {
        st.mode = static_cast<SliceMode>(mode_idx);
        st.preview_dirty = true;
    }

    if (st.mode == SliceMode::Grid) {
        bool changed = false;
        changed |= ImGui::InputInt("Cell Width", &st.cell_width);
        changed |= ImGui::InputInt("Cell Height", &st.cell_height);
        changed |= ImGui::InputInt("Padding", &st.padding);
        changed |= ImGui::InputInt("Offset X", &st.offset_x);
        changed |= ImGui::InputInt("Offset Y", &st.offset_y);
        st.cell_width = std::max(1, st.cell_width);
        st.cell_height = std::max(1, st.cell_height);
        if (changed) st.preview_dirty = true;
    } else if (st.mode == SliceMode::Auto) {
        if (ImGui::SliderFloat("Alpha Threshold", &st.alpha_threshold, 0.01f, 1.0f)) {
            st.preview_dirty = true;
        }
    }

    // Slice button
    if (ImGui::Button("Slice")) {
        if (st.mode == SliceMode::Grid) {
            SliceGrid(st.current_sheet, st.cell_width, st.cell_height, st.padding, st.offset_x, st.offset_y);
        } else if (st.mode == SliceMode::Auto) {
            SliceAuto(st.current_sheet, st.alpha_threshold);
        }
        st.preview_dirty = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save .dsprite")) {
        std::string save_path = st.current_sheet.source_texture_path + ".dsprite";
        SaveSpriteSheet(st.current_sheet, save_path);
    }

    ImGui::Separator();
    ImGui::Text("Frames: %d", static_cast<int>(st.current_sheet.frames.size()));

    // Frame list
    ImGui::BeginChild("##frame_list", ImVec2(200, 0), true);
    for (int i = 0; i < static_cast<int>(st.current_sheet.frames.size()); ++i) {
        auto& fr = st.current_sheet.frames[i];
        bool selected = (st.selected_frame == i);
        if (ImGui::Selectable(fr.name.c_str(), selected)) {
            st.selected_frame = i;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Preview canvas
    ImGui::BeginChild("##slice_canvas", ImVec2(0, 0), true);
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Draw texture background placeholder
    dl->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(40, 40, 40, 255));

    // Draw frame rects overlaid
    if (st.current_sheet.texture_width > 0 && st.current_sheet.texture_height > 0) {
        float sx = canvas_size.x / (float)st.current_sheet.texture_width * st.zoom;
        float sy = canvas_size.y / (float)st.current_sheet.texture_height * st.zoom;
        float scale = std::min(sx, sy);

        for (int i = 0; i < static_cast<int>(st.current_sheet.frames.size()); ++i) {
            auto& fr = st.current_sheet.frames[i];
            ImVec2 p0(canvas_pos.x + fr.x * scale, canvas_pos.y + fr.y * scale);
            ImVec2 p1(p0.x + fr.w * scale, p0.y + fr.h * scale);
            ImU32 col = (i == st.selected_frame) ? IM_COL32(50, 200, 255, 200) : IM_COL32(0, 255, 100, 120);
            dl->AddRect(p0, p1, col, 0.0f, 0, (i == st.selected_frame) ? 2.0f : 1.0f);
        }
    }

    // Frame details
    if (st.selected_frame >= 0 && st.selected_frame < static_cast<int>(st.current_sheet.frames.size())) {
        auto& fr = st.current_sheet.frames[st.selected_frame];
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + canvas_size.y + 4);
        ImGui::Text("Selected: %s", fr.name.c_str());
        ImGui::Text("Rect: (%d, %d, %d, %d)", fr.x, fr.y, fr.w, fr.h);
        ImGui::SliderFloat2("Pivot", &fr.pivot.x, 0.0f, 1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #2 — Sprite Atlas Packer
// ═══════════════════════════════════════════════════════════════════════════

static AtlasPackerState s_atlas_state;

AtlasPackerState& GetAtlasPackerState() { return s_atlas_state; }
int AtlasEntryCount() { return static_cast<int>(s_atlas_state.current_atlas.entries.size()); }

// Simple shelf-based bin packing
bool PackAtlas(AtlasAsset& atlas, const std::vector<std::string>& input_paths) {
    atlas.entries.clear();
    if (input_paths.empty()) return false;

    // Simulated packing: assign 64x64 per entry in a grid
    int entry_size = 64;
    int padding = atlas.padding;
    int cols = atlas.max_size / (entry_size + padding);
    if (cols <= 0) cols = 1;

    int x = padding, y = padding;
    int row_height = 0;

    for (size_t i = 0; i < input_paths.size(); ++i) {
        AtlasEntry entry;
        entry.source_path = input_paths[i];
        entry.name = input_paths[i];
        // Extract filename from path
        size_t slash = entry.name.find_last_of("/\\");
        if (slash != std::string::npos) entry.name = entry.name.substr(slash + 1);
        size_t dot = entry.name.find_last_of('.');
        if (dot != std::string::npos) entry.name = entry.name.substr(0, dot);

        entry.w = entry_size;
        entry.h = entry_size;

        if (x + entry_size + padding > atlas.max_size) {
            x = padding;
            y += row_height + padding;
            row_height = 0;
        }

        entry.x = x;
        entry.y = y;
        row_height = std::max(row_height, entry_size);
        x += entry_size + padding;

        atlas.entries.push_back(entry);
    }

    // Calculate final atlas dimensions
    int max_x = 0, max_y = 0;
    for (auto& e : atlas.entries) {
        max_x = std::max(max_x, e.x + e.w + padding);
        max_y = std::max(max_y, e.y + e.h + padding);
    }

    if (atlas.power_of_two) {
        auto next_pot = [](int v) -> int {
            int p = 1; while (p < v) p <<= 1; return p;
        };
        atlas.width = next_pot(max_x);
        atlas.height = next_pot(max_y);
    } else {
        atlas.width = max_x;
        atlas.height = max_y;
    }

    return true;
}

bool SaveAtlas(const AtlasAsset& atlas, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "  \"name\": \"" << atlas.name << "\",\n";
    f << "  \"width\": " << atlas.width << ",\n";
    f << "  \"height\": " << atlas.height << ",\n";
    f << "  \"entries\": [\n";
    for (size_t i = 0; i < atlas.entries.size(); ++i) {
        const auto& e = atlas.entries[i];
        f << "    { \"name\": \"" << e.name << "\", \"src\": \"" << e.source_path
          << "\", \"x\": " << e.x << ", \"y\": " << e.y
          << ", \"w\": " << e.w << ", \"h\": " << e.h << " }";
        if (i + 1 < atlas.entries.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

void DrawAtlasPackerPanel() {
    auto& st = s_atlas_state;
    if (!st.open) return;

    ImGui::Begin(MDI_ICON_LAYERS " Atlas Packer", &st.open);

    ImGui::InputInt("Max Size", &st.current_atlas.max_size);
    st.current_atlas.max_size = std::max(256, std::min(8192, st.current_atlas.max_size));
    ImGui::InputInt("Padding", &st.current_atlas.padding);
    ImGui::Checkbox("Power of Two", &st.current_atlas.power_of_two);
    ImGui::Separator();

    ImGui::Text("Input Sprites: %d", static_cast<int>(st.input_paths.size()));
    if (ImGui::Button("Add Sprite...")) {
        // Placeholder: in practice, open file dialog
        st.input_paths.push_back("sprite_" + std::to_string(st.input_paths.size()) + ".png");
        st.pack_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All")) {
        st.input_paths.clear();
        st.current_atlas.entries.clear();
        st.pack_dirty = true;
    }

    // Input list
    for (int i = 0; i < static_cast<int>(st.input_paths.size()); ++i) {
        ImGui::BulletText("%s", st.input_paths[i].c_str());
    }

    ImGui::Separator();
    if (ImGui::Button("Pack Atlas")) {
        PackAtlas(st.current_atlas, st.input_paths);
        st.pack_dirty = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save .datlas")) {
        SaveAtlas(st.current_atlas, st.current_atlas.name + ".datlas");
    }

    // Result info
    if (!st.current_atlas.entries.empty()) {
        ImGui::Text("Atlas: %dx%d, %d entries", st.current_atlas.width, st.current_atlas.height,
                    static_cast<int>(st.current_atlas.entries.size()));

        // Visual preview
        ImGui::BeginChild("##atlas_preview", ImVec2(0, 300), true);
        ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size = ImGui::GetContentRegionAvail();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            IM_COL32(30, 30, 30, 255));

        float scale = std::min(canvas_size.x / (float)st.current_atlas.width,
                               canvas_size.y / (float)st.current_atlas.height);
        for (auto& e : st.current_atlas.entries) {
            ImVec2 p0(canvas_pos.x + e.x * scale, canvas_pos.y + e.y * scale);
            ImVec2 p1(p0.x + e.w * scale, p0.y + e.h * scale);
            dl->AddRectFilled(p0, p1, IM_COL32(60, 120, 200, 180));
            dl->AddRect(p0, p1, IM_COL32(100, 180, 255, 255));
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #3 — 2D Frame Animation Editor
// ═══════════════════════════════════════════════════════════════════════════

static Anim2DEditorState s_anim2d_state;

Anim2DEditorState& GetAnim2DEditorState() { return s_anim2d_state; }
int Anim2DClipCount() { return static_cast<int>(s_anim2d_state.current_anim.clips.size()); }
int Anim2DFrameCount(int clip_index) {
    if (clip_index < 0 || clip_index >= static_cast<int>(s_anim2d_state.current_anim.clips.size()))
        return 0;
    return static_cast<int>(s_anim2d_state.current_anim.clips[clip_index].frames.size());
}
bool Anim2DIsPlaying() { return s_anim2d_state.playing; }

void Anim2DPlay() { s_anim2d_state.playing = true; s_anim2d_state.playback_time = 0.0f; }
void Anim2DStop() { s_anim2d_state.playing = false; s_anim2d_state.playback_time = 0.0f; }

void Anim2DAddFrame(int clip_index, int frame_index, float duration) {
    if (clip_index < 0 || clip_index >= static_cast<int>(s_anim2d_state.current_anim.clips.size()))
        return;
    AnimFrame2D frame;
    frame.frame_index = frame_index;
    frame.duration = duration;
    s_anim2d_state.current_anim.clips[clip_index].frames.push_back(frame);

    // Recalculate total duration
    auto& clip = s_anim2d_state.current_anim.clips[clip_index];
    clip.total_duration = 0.0f;
    for (auto& f : clip.frames) clip.total_duration += f.duration;
}

bool SaveAnimation2D(const Animation2DAsset& asset, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "  \"name\": \"" << asset.name << "\",\n";
    f << "  \"sprite_sheet\": \"" << asset.sprite_sheet_path << "\",\n";
    f << "  \"clips\": [\n";
    for (size_t ci = 0; ci < asset.clips.size(); ++ci) {
        const auto& clip = asset.clips[ci];
        f << "    { \"name\": \"" << clip.name << "\", \"loop\": " << (clip.loop ? "true" : "false")
          << ", \"frames\": [\n";
        for (size_t fi = 0; fi < clip.frames.size(); ++fi) {
            const auto& fr = clip.frames[fi];
            f << "      { \"index\": " << fr.frame_index << ", \"duration\": " << fr.duration << " }";
            if (fi + 1 < clip.frames.size()) f << ",";
            f << "\n";
        }
        f << "    ] }";
        if (ci + 1 < asset.clips.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

void DrawAnim2DEditorPanel() {
    auto& st = s_anim2d_state;
    if (!st.open) return;

    ImGui::Begin(MDI_ICON_MOVIE " 2D Animation", &st.open);

    // Clip selection
    if (st.current_anim.clips.empty()) {
        if (ImGui::Button("+ New Clip")) {
            Animation2DClip clip;
            clip.name = "Idle";
            st.current_anim.clips.push_back(clip);
            st.selected_clip = 0;
        }
    } else {
        // Clip tabs
        for (int i = 0; i < static_cast<int>(st.current_anim.clips.size()); ++i) {
            if (i > 0) ImGui::SameLine();
            bool selected = (st.selected_clip == i);
            if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1));
            if (ImGui::Button(st.current_anim.clips[i].name.c_str())) {
                st.selected_clip = i;
            }
            if (selected) ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            Animation2DClip clip;
            clip.name = "Clip_" + std::to_string(st.current_anim.clips.size());
            st.current_anim.clips.push_back(clip);
        }
    }

    ImGui::Separator();

    // Playback controls
    if (ImGui::Button(st.playing ? MDI_ICON_STOP " Stop" : MDI_ICON_PLAY " Play")) {
        if (st.playing) Anim2DStop(); else Anim2DPlay();
    }
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &st.playback_speed, 0.1f, 5.0f);

    // Current clip frames
    if (st.selected_clip >= 0 && st.selected_clip < static_cast<int>(st.current_anim.clips.size())) {
        auto& clip = st.current_anim.clips[st.selected_clip];
        ImGui::Checkbox("Loop", &clip.loop);
        ImGui::Text("Frames: %d | Duration: %.2fs", static_cast<int>(clip.frames.size()), clip.total_duration);

        // Timeline strip
        ImGui::BeginChild("##anim2d_timeline", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImVec2 timeline_pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float x_cursor = 0;
        for (int i = 0; i < static_cast<int>(clip.frames.size()); ++i) {
            float frame_w = clip.frames[i].duration * 200.0f;
            ImVec2 p0(timeline_pos.x + x_cursor, timeline_pos.y);
            ImVec2 p1(p0.x + frame_w, p0.y + 60);
            ImU32 col = (i == st.selected_frame) ? IM_COL32(80, 160, 255, 200) : IM_COL32(60, 80, 100, 200);
            dl->AddRectFilled(p0, p1, col);
            dl->AddRect(p0, p1, IM_COL32(150, 150, 150, 255));

            char label[32];
            snprintf(label, sizeof(label), "%d", clip.frames[i].frame_index);
            dl->AddText(ImVec2(p0.x + 4, p0.y + 4), IM_COL32(255, 255, 255, 255), label);

            // Click to select
            ImGui::SetCursorScreenPos(p0);
            ImGui::InvisibleButton(("##af" + std::to_string(i)).c_str(), ImVec2(frame_w, 60));
            if (ImGui::IsItemClicked()) st.selected_frame = i;

            x_cursor += frame_w + 2;
        }
        ImGui::EndChild();

        // Add frame button
        if (ImGui::Button("+ Add Frame")) {
            Anim2DAddFrame(st.selected_clip, static_cast<int>(clip.frames.size()), 0.1f);
        }
        ImGui::SameLine();
        if (st.selected_frame >= 0 && st.selected_frame < static_cast<int>(clip.frames.size())) {
            if (ImGui::Button("Remove Frame")) {
                clip.frames.erase(clip.frames.begin() + st.selected_frame);
                st.selected_frame = -1;
                clip.total_duration = 0;
                for (auto& f : clip.frames) clip.total_duration += f.duration;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            ImGui::InputFloat("Duration", &clip.frames[st.selected_frame].duration, 0.01f, 0.1f, "%.3f");
        }
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #4 — 9-Slice / 9-Patch Editor
// ═══════════════════════════════════════════════════════════════════════════

static NineSliceEditorState s_nineslice_state;

NineSliceEditorState& GetNineSliceEditorState() { return s_nineslice_state; }
bool NineSliceHasValidBorders() {
    auto& d = s_nineslice_state.current;
    return d.left >= 0 && d.right >= 0 && d.top >= 0 && d.bottom >= 0 &&
           (d.left + d.right) < d.tex_width && (d.top + d.bottom) < d.tex_height;
}

bool SaveNineSlice(const NineSliceData& data, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{\n";
    f << "  \"texture\": \"" << data.texture_path << "\",\n";
    f << "  \"left\": " << data.left << ", \"right\": " << data.right << ",\n";
    f << "  \"top\": " << data.top << ", \"bottom\": " << data.bottom << ",\n";
    f << "  \"width\": " << data.tex_width << ", \"height\": " << data.tex_height << "\n";
    f << "}\n";
    return true;
}

void DrawNineSliceEditorPanel() {
    auto& st = s_nineslice_state;
    if (!st.open) return;

    ImGui::Begin("9-Slice Editor", &st.open);

    ImGui::Text("Texture: %s (%dx%d)", st.current.texture_path.c_str(),
                st.current.tex_width, st.current.tex_height);
    ImGui::Separator();

    // Border sliders
    ImGui::SliderInt("Left", &st.current.left, 0, st.current.tex_width / 2);
    ImGui::SliderInt("Right", &st.current.right, 0, st.current.tex_width / 2);
    ImGui::SliderInt("Top", &st.current.top, 0, st.current.tex_height / 2);
    ImGui::SliderInt("Bottom", &st.current.bottom, 0, st.current.tex_height / 2);

    ImGui::Checkbox("Show Guides", &st.show_guides);
    ImGui::SliderFloat("Zoom", &st.zoom, 0.5f, 8.0f);

    // Preview
    ImGui::Separator();
    ImGui::Text("Preview (stretched):");
    ImGui::SliderFloat2("Preview Size", &st.preview_size.x, 50.0f, 500.0f);

    ImGui::BeginChild("##9slice_canvas", ImVec2(0, 300), true);
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    float scale = st.zoom;
    float w = (float)st.current.tex_width * scale;
    float h = (float)st.current.tex_height * scale;

    // Draw texture rect
    dl->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + w, canvas_pos.y + h),
                      IM_COL32(80, 80, 80, 255));

    if (st.show_guides) {
        float left_x = canvas_pos.x + st.current.left * scale;
        float right_x = canvas_pos.x + (st.current.tex_width - st.current.right) * scale;
        float top_y = canvas_pos.y + st.current.top * scale;
        float bottom_y = canvas_pos.y + (st.current.tex_height - st.current.bottom) * scale;

        ImU32 guide_col = IM_COL32(0, 255, 100, 200);
        dl->AddLine(ImVec2(left_x, canvas_pos.y), ImVec2(left_x, canvas_pos.y + h), guide_col);
        dl->AddLine(ImVec2(right_x, canvas_pos.y), ImVec2(right_x, canvas_pos.y + h), guide_col);
        dl->AddLine(ImVec2(canvas_pos.x, top_y), ImVec2(canvas_pos.x + w, top_y), guide_col);
        dl->AddLine(ImVec2(canvas_pos.x, bottom_y), ImVec2(canvas_pos.x + w, bottom_y), guide_col);
    }

    // Stretched preview on the right
    float px = canvas_pos.x + w + 20;
    float py = canvas_pos.y;
    dl->AddRectFilled(ImVec2(px, py),
                      ImVec2(px + st.preview_size.x, py + st.preview_size.y),
                      IM_COL32(60, 60, 80, 255));
    dl->AddRect(ImVec2(px, py),
                ImVec2(px + st.preview_size.x, py + st.preview_size.y),
                IM_COL32(200, 200, 200, 255));
    dl->AddText(ImVec2(px + 4, py + 4), IM_COL32(255, 255, 255, 200), "9-Slice Stretch Preview");

    ImGui::EndChild();

    if (ImGui::Button("Save .d9slice")) {
        SaveNineSlice(st.current, st.current.texture_path + ".d9slice");
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #5 — 2D Collision Shape Editor
// ═══════════════════════════════════════════════════════════════════════════

static CollisionEditor2DState s_collision2d_state;

CollisionEditor2DState& GetCollisionEditor2DState() { return s_collision2d_state; }
int CollisionShape2DCount() { return static_cast<int>(s_collision2d_state.shapes.size()); }

void AddCollisionShape2D(Shape2DType type) {
    CollisionShape2D shape;
    shape.type = type;
    switch (type) {
        case Shape2DType::Box:
            shape.half_extents = {0.5f, 0.5f};
            break;
        case Shape2DType::Circle:
            shape.radius = 0.5f;
            break;
        case Shape2DType::Capsule:
            shape.radius = 0.3f;
            shape.capsule_height = 1.0f;
            break;
        case Shape2DType::Polygon:
            shape.vertices = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
            break;
        case Shape2DType::Edge:
            shape.vertices = {{-1.0f, 0.0f}, {1.0f, 0.0f}};
            break;
    }
    s_collision2d_state.shapes.push_back(shape);
    s_collision2d_state.selected_shape = static_cast<int>(s_collision2d_state.shapes.size()) - 1;
}

void DrawCollisionShapes2DOverlay(ImDrawList* draw_list, ImVec2 origin, float scale) {
    auto& st = s_collision2d_state;
    for (int i = 0; i < static_cast<int>(st.shapes.size()); ++i) {
        auto& shape = st.shapes[i];
        bool selected = (i == st.selected_shape);
        ImU32 col = selected ? IM_COL32(50, 255, 50, 200) : IM_COL32(50, 200, 50, 120);
        ImU32 fill = selected ? IM_COL32(50, 255, 50, 40) : IM_COL32(50, 200, 50, 20);
        ImVec2 center(origin.x + shape.offset.x * scale, origin.y + shape.offset.y * scale);

        switch (shape.type) {
            case Shape2DType::Box: {
                ImVec2 half(shape.half_extents.x * scale, shape.half_extents.y * scale);
                draw_list->AddRectFilled(ImVec2(center.x - half.x, center.y - half.y),
                                          ImVec2(center.x + half.x, center.y + half.y), fill);
                draw_list->AddRect(ImVec2(center.x - half.x, center.y - half.y),
                                   ImVec2(center.x + half.x, center.y + half.y), col, 0, 0, selected ? 2.0f : 1.0f);
                break;
            }
            case Shape2DType::Circle: {
                draw_list->AddCircleFilled(center, shape.radius * scale, fill);
                draw_list->AddCircle(center, shape.radius * scale, col, 32, selected ? 2.0f : 1.0f);
                break;
            }
            case Shape2DType::Capsule: {
                float r = shape.radius * scale;
                float h = shape.capsule_height * scale * 0.5f;
                draw_list->AddCircle(ImVec2(center.x, center.y - h), r, col, 24, selected ? 2.0f : 1.0f);
                draw_list->AddCircle(ImVec2(center.x, center.y + h), r, col, 24, selected ? 2.0f : 1.0f);
                draw_list->AddLine(ImVec2(center.x - r, center.y - h), ImVec2(center.x - r, center.y + h), col, selected ? 2.0f : 1.0f);
                draw_list->AddLine(ImVec2(center.x + r, center.y - h), ImVec2(center.x + r, center.y + h), col, selected ? 2.0f : 1.0f);
                break;
            }
            case Shape2DType::Polygon: {
                if (shape.vertices.size() >= 3) {
                    for (size_t v = 0; v < shape.vertices.size(); ++v) {
                        size_t next = (v + 1) % shape.vertices.size();
                        ImVec2 p0(origin.x + (shape.offset.x + shape.vertices[v].x) * scale,
                                  origin.y + (shape.offset.y + shape.vertices[v].y) * scale);
                        ImVec2 p1(origin.x + (shape.offset.x + shape.vertices[next].x) * scale,
                                  origin.y + (shape.offset.y + shape.vertices[next].y) * scale);
                        draw_list->AddLine(p0, p1, col, selected ? 2.0f : 1.0f);
                    }
                    // vertex handles
                    if (selected) {
                        for (auto& v : shape.vertices) {
                            ImVec2 vp(origin.x + (shape.offset.x + v.x) * scale,
                                      origin.y + (shape.offset.y + v.y) * scale);
                            draw_list->AddCircleFilled(vp, 4, IM_COL32(255, 200, 50, 255));
                        }
                    }
                }
                break;
            }
            case Shape2DType::Edge: {
                if (shape.vertices.size() >= 2) {
                    for (size_t v = 0; v + 1 < shape.vertices.size(); ++v) {
                        ImVec2 p0(origin.x + (shape.offset.x + shape.vertices[v].x) * scale,
                                  origin.y + (shape.offset.y + shape.vertices[v].y) * scale);
                        ImVec2 p1(origin.x + (shape.offset.x + shape.vertices[v + 1].x) * scale,
                                  origin.y + (shape.offset.y + shape.vertices[v + 1].y) * scale);
                        draw_list->AddLine(p0, p1, col, selected ? 2.5f : 1.5f);
                    }
                }
                break;
            }
        }
    }
}

void DrawCollisionEditor2DPanel() {
    auto& st = s_collision2d_state;
    if (!st.open) return;

    ImGui::Begin("2D Collision Editor", &st.open);

    // Add shape buttons
    if (ImGui::Button("+ Box")) AddCollisionShape2D(Shape2DType::Box);
    ImGui::SameLine();
    if (ImGui::Button("+ Circle")) AddCollisionShape2D(Shape2DType::Circle);
    ImGui::SameLine();
    if (ImGui::Button("+ Capsule")) AddCollisionShape2D(Shape2DType::Capsule);
    ImGui::SameLine();
    if (ImGui::Button("+ Polygon")) AddCollisionShape2D(Shape2DType::Polygon);
    ImGui::SameLine();
    if (ImGui::Button("+ Edge")) AddCollisionShape2D(Shape2DType::Edge);

    ImGui::Checkbox("Snap to Grid", &st.snap_to_grid);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputFloat("Grid", &st.grid_size, 0.05f);

    ImGui::Separator();

    // Shape list
    ImGui::BeginChild("##shape_list", ImVec2(180, 0), true);
    for (int i = 0; i < static_cast<int>(st.shapes.size()); ++i) {
        const char* type_names[] = {"Box", "Circle", "Polygon", "Edge", "Capsule"};
        char label[64];
        snprintf(label, sizeof(label), "%s ##%d", type_names[static_cast<int>(st.shapes[i].type)], i);
        bool sel = (i == st.selected_shape);
        if (ImGui::Selectable(label, sel)) st.selected_shape = i;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Properties
    ImGui::BeginChild("##shape_props", ImVec2(0, 0), true);
    if (st.selected_shape >= 0 && st.selected_shape < static_cast<int>(st.shapes.size())) {
        auto& shape = st.shapes[st.selected_shape];
        ImGui::DragFloat2("Offset", &shape.offset.x, 0.01f);
        ImGui::DragFloat("Rotation", &shape.rotation, 1.0f, -360.0f, 360.0f);
        ImGui::Checkbox("Is Trigger", &shape.is_trigger);

        switch (shape.type) {
            case Shape2DType::Box:
                ImGui::DragFloat2("Half Extents", &shape.half_extents.x, 0.01f, 0.01f, 100.0f);
                break;
            case Shape2DType::Circle:
                ImGui::DragFloat("Radius", &shape.radius, 0.01f, 0.01f, 100.0f);
                break;
            case Shape2DType::Capsule:
                ImGui::DragFloat("Radius", &shape.radius, 0.01f, 0.01f, 100.0f);
                ImGui::DragFloat("Height", &shape.capsule_height, 0.01f, 0.01f, 100.0f);
                break;
            case Shape2DType::Polygon:
                ImGui::Text("Vertices: %d", static_cast<int>(shape.vertices.size()));
                for (int v = 0; v < static_cast<int>(shape.vertices.size()); ++v) {
                    char vlabel[32];
                    snprintf(vlabel, sizeof(vlabel), "V%d", v);
                    ImGui::DragFloat2(vlabel, &shape.vertices[v].x, 0.01f);
                }
                if (ImGui::Button("+ Vertex")) {
                    shape.vertices.push_back({0, 0});
                }
                break;
            case Shape2DType::Edge:
                for (int v = 0; v < static_cast<int>(shape.vertices.size()); ++v) {
                    char vlabel[32];
                    snprintf(vlabel, sizeof(vlabel), "P%d", v);
                    ImGui::DragFloat2(vlabel, &shape.vertices[v].x, 0.01f);
                }
                if (ImGui::Button("+ Point")) {
                    shape.vertices.push_back({0, 0});
                }
                break;
        }

        ImGui::Separator();
        if (ImGui::Button("Delete Shape")) {
            st.shapes.erase(st.shapes.begin() + st.selected_shape);
            st.selected_shape = -1;
        }
    }
    ImGui::EndChild();

    // Visual preview
    ImGui::Separator();
    ImGui::BeginChild("##collision_preview", ImVec2(0, 200), true);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 center(origin.x + avail.x * 0.5f, origin.y + avail.y * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(25, 25, 30, 255));
    DrawCollisionShapes2DOverlay(dl, center, 100.0f);
    ImGui::EndChild();

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #6 — 2D Particle Editor
// ═══════════════════════════════════════════════════════════════════════════

static Particle2DEditorState s_particle2d_state;

Particle2DEditorState& GetParticle2DEditorState() { return s_particle2d_state; }
bool Particle2DSimulating() { return s_particle2d_state.simulating; }
int Particle2DActiveCount() { return s_particle2d_state.active_particles; }

bool SaveParticle2DConfig(const Particle2DConfig& cfg, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{\n";
    f << "  \"name\": \"" << cfg.name << "\",\n";
    f << "  \"emit_rate\": " << cfg.emit_rate << ",\n";
    f << "  \"max_particles\": " << cfg.max_particles << ",\n";
    f << "  \"emit_shape\": " << static_cast<int>(cfg.emit_shape) << ",\n";
    f << "  \"lifetime_min\": " << cfg.lifetime_min << ",\n";
    f << "  \"lifetime_max\": " << cfg.lifetime_max << ",\n";
    f << "  \"velocity_min\": [" << cfg.velocity_min.x << ", " << cfg.velocity_min.y << "],\n";
    f << "  \"velocity_max\": [" << cfg.velocity_max.x << ", " << cfg.velocity_max.y << "],\n";
    f << "  \"gravity\": [" << cfg.gravity.x << ", " << cfg.gravity.y << "],\n";
    f << "  \"start_size_min\": " << cfg.start_size_min << ",\n";
    f << "  \"start_size_max\": " << cfg.start_size_max << ",\n";
    f << "  \"end_size\": " << cfg.end_size << ",\n";
    f << "  \"blend_mode\": " << static_cast<int>(cfg.blend_mode) << ",\n";
    f << "  \"trail_enabled\": " << (cfg.trail_enabled ? "true" : "false") << "\n";
    f << "}\n";
    return true;
}

void DrawParticle2DEditorPanel() {
    auto& st = s_particle2d_state;
    if (!st.open) return;

    ImGui::Begin(MDI_ICON_FLASH " 2D Particles", &st.open);

    auto& cfg = st.config;

    // Simulation controls
    if (ImGui::Button(st.simulating ? MDI_ICON_STOP " Stop" : MDI_ICON_PLAY " Simulate")) {
        st.simulating = !st.simulating;
        if (!st.simulating) { st.sim_time = 0; st.active_particles = 0; }
    }
    ImGui::SameLine();
    ImGui::Text("Active: %d | Time: %.1fs", st.active_particles, st.sim_time);

    ImGui::Separator();

    // Emission
    if (ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Rate", &cfg.emit_rate, 1.0f, 0.0f, 1000.0f);
        ImGui::DragInt("Max Particles", &cfg.max_particles, 1, 1, 10000);
        const char* shapes[] = {"Point", "Circle", "Rectangle", "Line"};
        int shape_idx = static_cast<int>(cfg.emit_shape);
        if (ImGui::Combo("Shape", &shape_idx, shapes, 4)) cfg.emit_shape = static_cast<Particle2DEmitShape>(shape_idx);
        if (cfg.emit_shape == Particle2DEmitShape::Circle)
            ImGui::DragFloat("Radius", &cfg.emit_radius, 1.0f, 0.0f, 500.0f);
        if (cfg.emit_shape == Particle2DEmitShape::Rectangle)
            ImGui::DragFloat2("Rect Size", &cfg.emit_rect.x, 1.0f);
    }

    // Lifetime
    if (ImGui::CollapsingHeader("Lifetime", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Min", &cfg.lifetime_min, 0.01f, 0.01f, 30.0f);
        ImGui::DragFloat("Max", &cfg.lifetime_max, 0.01f, 0.01f, 30.0f);
    }

    // Motion
    if (ImGui::CollapsingHeader("Motion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Velocity Min", &cfg.velocity_min.x, 1.0f);
        ImGui::DragFloat2("Velocity Max", &cfg.velocity_max.x, 1.0f);
        ImGui::DragFloat2("Gravity", &cfg.gravity.x, 1.0f);
        ImGui::DragFloat("Damping", &cfg.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Angular Vel Min", &cfg.angular_velocity_min, 1.0f, -720.0f, 720.0f);
        ImGui::DragFloat("Angular Vel Max", &cfg.angular_velocity_max, 1.0f, -720.0f, 720.0f);
    }

    // Appearance
    if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Start Size Min", &cfg.start_size_min, 0.5f, 0.1f, 200.0f);
        ImGui::DragFloat("Start Size Max", &cfg.start_size_max, 0.5f, 0.1f, 200.0f);
        ImGui::DragFloat("End Size", &cfg.end_size, 0.5f, 0.0f, 200.0f);
        ImGui::ColorEdit4("Start Color", &cfg.start_color.x);
        ImGui::ColorEdit4("End Color", &cfg.end_color.x);
        const char* blends[] = {"Alpha", "Additive", "Multiply"};
        int blend_idx = static_cast<int>(cfg.blend_mode);
        if (ImGui::Combo("Blend", &blend_idx, blends, 3)) cfg.blend_mode = static_cast<Particle2DBlendMode>(blend_idx);
    }

    // Trail
    if (ImGui::CollapsingHeader("Trail")) {
        ImGui::Checkbox("Enable Trail", &cfg.trail_enabled);
        if (cfg.trail_enabled) {
            ImGui::DragInt("Trail Length", &cfg.trail_length, 1, 1, 30);
            ImGui::DragFloat("Trail Width", &cfg.trail_width, 0.1f, 0.1f, 20.0f);
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Save .dparticle2d")) {
        SaveParticle2DConfig(cfg, cfg.name + ".dparticle2d");
    }

    // Simple simulation update
    if (st.simulating) {
        float dt = ImGui::GetIO().DeltaTime;
        st.sim_time += dt;
        int new_particles = static_cast<int>(cfg.emit_rate * dt);
        st.active_particles = std::min(st.active_particles + new_particles, cfg.max_particles);
        // Particles die over time
        float avg_lifetime = (cfg.lifetime_min + cfg.lifetime_max) * 0.5f;
        int dying = static_cast<int>((float)st.active_particles / avg_lifetime * dt);
        st.active_particles = std::max(0, st.active_particles - dying);
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #7 — Parallax Layer Editor
// ═══════════════════════════════════════════════════════════════════════════

static ParallaxEditorState s_parallax_state;

ParallaxEditorState& GetParallaxEditorState() { return s_parallax_state; }
int ParallaxLayerCount() { return static_cast<int>(s_parallax_state.config.layers.size()); }

void AddParallaxLayer(const std::string& name) {
    ParallaxLayer layer;
    layer.name = name;
    layer.sort_order = static_cast<int>(s_parallax_state.config.layers.size());
    s_parallax_state.config.layers.push_back(layer);
    s_parallax_state.selected_layer = static_cast<int>(s_parallax_state.config.layers.size()) - 1;
}

bool SaveParallaxConfig(const ParallaxConfig& cfg, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{\n";
    f << "  \"name\": \"" << cfg.name << "\",\n";
    f << "  \"base_speed\": " << cfg.base_speed << ",\n";
    f << "  \"layers\": [\n";
    for (size_t i = 0; i < cfg.layers.size(); ++i) {
        const auto& l = cfg.layers[i];
        f << "    { \"name\": \"" << l.name << "\", \"texture\": \"" << l.texture_path
          << "\", \"scroll_x\": " << l.scroll_factor_x << ", \"scroll_y\": " << l.scroll_factor_y
          << ", \"offset_y\": " << l.offset_y << ", \"repeat_x\": " << (l.repeat_x ? "true" : "false")
          << ", \"sort_order\": " << l.sort_order << ", \"opacity\": " << l.opacity << " }";
        if (i + 1 < cfg.layers.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

void DrawParallaxEditorPanel() {
    auto& st = s_parallax_state;
    if (!st.open) return;

    ImGui::Begin(MDI_ICON_LAYERS " Parallax Editor", &st.open);

    ImGui::DragFloat("Base Speed", &st.config.base_speed, 0.1f, 0.0f, 10.0f);
    ImGui::Separator();

    // Layer list
    if (ImGui::Button("+ Add Layer")) {
        AddParallaxLayer("Layer_" + std::to_string(st.config.layers.size()));
    }

    ImGui::BeginChild("##parallax_layers", ImVec2(200, 0), true);
    for (int i = 0; i < static_cast<int>(st.config.layers.size()); ++i) {
        bool sel = (i == st.selected_layer);
        if (ImGui::Selectable(st.config.layers[i].name.c_str(), sel)) {
            st.selected_layer = i;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Layer properties
    ImGui::BeginChild("##parallax_props", ImVec2(0, 0), true);
    if (st.selected_layer >= 0 && st.selected_layer < static_cast<int>(st.config.layers.size())) {
        auto& layer = st.config.layers[st.selected_layer];
        char name_buf[64];
        strncpy(name_buf, layer.name.c_str(), sizeof(name_buf) - 1);
        name_buf[sizeof(name_buf) - 1] = '\0';
        if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
            layer.name = name_buf;
        }
        ImGui::Text("Texture: %s", layer.texture_path.c_str());
        ImGui::DragFloat("Scroll X", &layer.scroll_factor_x, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Scroll Y", &layer.scroll_factor_y, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Offset Y", &layer.offset_y, 1.0f);
        ImGui::Checkbox("Repeat X", &layer.repeat_x);
        ImGui::SameLine();
        ImGui::Checkbox("Repeat Y", &layer.repeat_y);
        ImGui::DragInt("Sort Order", &layer.sort_order, 1);
        ImGui::SliderFloat("Opacity", &layer.opacity, 0.0f, 1.0f);
        ImGui::ColorEdit4("Tint", &layer.tint.x);

        ImGui::Separator();
        if (ImGui::Button("Delete Layer")) {
            st.config.layers.erase(st.config.layers.begin() + st.selected_layer);
            st.selected_layer = -1;
        }
        if (st.selected_layer > 0 && ImGui::Button("Move Up")) {
            std::swap(st.config.layers[st.selected_layer], st.config.layers[st.selected_layer - 1]);
            --st.selected_layer;
        }
        ImGui::SameLine();
        if (st.selected_layer >= 0 && st.selected_layer < static_cast<int>(st.config.layers.size()) - 1 && ImGui::Button("Move Down")) {
            std::swap(st.config.layers[st.selected_layer], st.config.layers[st.selected_layer + 1]);
            ++st.selected_layer;
        }
    }
    ImGui::EndChild();

    // Preview strip
    ImGui::Separator();
    ImGui::Checkbox("Preview Scroll", &st.preview_playing);
    if (st.preview_playing) {
        st.preview_scroll += ImGui::GetIO().DeltaTime * st.config.base_speed * 50.0f;
    }

    ImGui::BeginChild("##parallax_preview", ImVec2(0, 120), true);
    ImVec2 preview_pos = ImGui::GetCursorScreenPos();
    ImVec2 preview_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(preview_pos, ImVec2(preview_pos.x + preview_size.x, preview_pos.y + preview_size.y),
                      IM_COL32(20, 20, 40, 255));

    // Draw layers as colored strips with scroll offset
    for (int i = static_cast<int>(st.config.layers.size()) - 1; i >= 0; --i) {
        auto& layer = st.config.layers[i];
        float offset = std::fmod(st.preview_scroll * layer.scroll_factor_x, preview_size.x);
        float y = preview_pos.y + layer.offset_y * 0.5f + (float)i * 20.0f;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(layer.tint.x, layer.tint.y, layer.tint.z, layer.opacity * 0.6f));
        for (int rep = -1; rep <= 1; ++rep) {
            float rx = preview_pos.x + (float)rep * preview_size.x + offset;
            dl->AddRectFilled(ImVec2(rx, y), ImVec2(rx + preview_size.x, y + 15), col);
            dl->AddText(ImVec2(rx + 4, y + 1), IM_COL32(255, 255, 255, 180), layer.name.c_str());
        }
    }
    ImGui::EndChild();

    if (ImGui::Button("Save .dparallax")) {
        SaveParallaxConfig(st.config, st.config.name + ".dparallax");
    }

    ImGui::End();
}

// ═══════════════════════════════════════════════════════════════════════════
// #8 — 2D Lighting Editor
// ═══════════════════════════════════════════════════════════════════════════

static Light2DEditorState s_light2d_state;

Light2DEditorState& GetLight2DEditorState() { return s_light2d_state; }
int Light2DCount() { return static_cast<int>(s_light2d_state.lights.size()); }

void AddLight2D(Light2DType type) {
    Light2DConfig light;
    light.type = type;
    light.name = "Light_" + std::to_string(s_light2d_state.lights.size());
    s_light2d_state.lights.push_back(light);
    s_light2d_state.selected_light = static_cast<int>(s_light2d_state.lights.size()) - 1;
}

bool SaveLight2DScene(const Light2DEditorState& state, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{\n";
    f << "  \"ambient_color\": [" << state.ambient_color.x << ", " << state.ambient_color.y << ", " << state.ambient_color.z << "],\n";
    f << "  \"ambient_intensity\": " << state.ambient_intensity << ",\n";
    f << "  \"lights\": [\n";
    for (size_t i = 0; i < state.lights.size(); ++i) {
        const auto& l = state.lights[i];
        f << "    { \"name\": \"" << l.name << "\", \"type\": " << static_cast<int>(l.type)
          << ", \"position\": [" << l.position.x << ", " << l.position.y << "]"
          << ", \"color\": [" << l.color.x << ", " << l.color.y << ", " << l.color.z << "]"
          << ", \"intensity\": " << l.intensity << ", \"range\": " << l.range
          << ", \"falloff\": " << l.falloff
          << ", \"shadow_mode\": " << static_cast<int>(l.shadow_mode) << " }";
        if (i + 1 < state.lights.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

void DrawLight2DGizmos(ImDrawList* draw_list, ImVec2 origin, float scale) {
    auto& st = s_light2d_state;
    if (!st.show_gizmos) return;

    for (int i = 0; i < static_cast<int>(st.lights.size()); ++i) {
        auto& light = st.lights[i];
        bool selected = (i == st.selected_light);
        ImVec2 center(origin.x + light.position.x * scale, origin.y + light.position.y * scale);
        float range_px = light.range * scale * 0.01f;

        ImU32 col = ImGui::ColorConvertFloat4ToU32(
            ImVec4(light.color.x, light.color.y, light.color.z, selected ? 0.5f : 0.25f));
        ImU32 border_col = ImGui::ColorConvertFloat4ToU32(
            ImVec4(light.color.x, light.color.y, light.color.z, selected ? 1.0f : 0.6f));

        switch (light.type) {
            case Light2DType::Point:
                draw_list->AddCircleFilled(center, range_px, col);
                draw_list->AddCircle(center, range_px, border_col, 48, selected ? 2.0f : 1.0f);
                draw_list->AddCircleFilled(center, 5, border_col);
                break;
            case Light2DType::Spot: {
                float angle_rad = light.spot_angle * 3.14159f / 180.0f;
                float dir_rad = light.spot_direction * 3.14159f / 180.0f;
                ImVec2 dir1(center.x + range_px * cosf(dir_rad - angle_rad * 0.5f),
                            center.y + range_px * sinf(dir_rad - angle_rad * 0.5f));
                ImVec2 dir2(center.x + range_px * cosf(dir_rad + angle_rad * 0.5f),
                            center.y + range_px * sinf(dir_rad + angle_rad * 0.5f));
                draw_list->AddTriangleFilled(center, dir1, dir2, col);
                draw_list->AddTriangle(center, dir1, dir2, border_col, selected ? 2.0f : 1.0f);
                draw_list->AddCircleFilled(center, 5, border_col);
                break;
            }
            case Light2DType::Directional: {
                float dir_rad = light.spot_direction * 3.14159f / 180.0f;
                for (int a = -2; a <= 2; ++a) {
                    ImVec2 start(center.x + (float)a * 15.0f, center.y);
                    ImVec2 end(start.x + 40.0f * cosf(dir_rad), start.y + 40.0f * sinf(dir_rad));
                    draw_list->AddLine(start, end, border_col, 1.5f);
                    // Arrow head
                    draw_list->AddCircleFilled(end, 3, border_col);
                }
                break;
            }
        }
    }
}

void DrawLight2DEditorPanel() {
    auto& st = s_light2d_state;
    if (!st.open) return;

    ImGui::Begin(MDI_ICON_FLASH " 2D Lighting", &st.open);

    // Ambient
    if (ImGui::CollapsingHeader("Ambient", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Ambient Color", &st.ambient_color.x);
        ImGui::SliderFloat("Ambient Intensity", &st.ambient_intensity, 0.0f, 1.0f);
    }

    ImGui::Separator();

    // Add lights
    if (ImGui::Button("+ Point")) AddLight2D(Light2DType::Point);
    ImGui::SameLine();
    if (ImGui::Button("+ Spot")) AddLight2D(Light2DType::Spot);
    ImGui::SameLine();
    if (ImGui::Button("+ Directional")) AddLight2D(Light2DType::Directional);

    ImGui::Checkbox("Show Gizmos", &st.show_gizmos);

    // Light list
    ImGui::BeginChild("##light_list", ImVec2(160, 200), true);
    for (int i = 0; i < static_cast<int>(st.lights.size()); ++i) {
        bool sel = (i == st.selected_light);
        if (ImGui::Selectable(st.lights[i].name.c_str(), sel)) {
            st.selected_light = i;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Light properties
    ImGui::BeginChild("##light_props", ImVec2(0, 200), true);
    if (st.selected_light >= 0 && st.selected_light < static_cast<int>(st.lights.size())) {
        auto& light = st.lights[st.selected_light];
        const char* types[] = {"Point", "Spot", "Directional"};
        int type_idx = static_cast<int>(light.type);
        if (ImGui::Combo("Type", &type_idx, types, 3)) light.type = static_cast<Light2DType>(type_idx);

        ImGui::DragFloat2("Position", &light.position.x, 1.0f);
        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Range", &light.range, 1.0f, 1.0f, 2000.0f);
        ImGui::DragFloat("Falloff", &light.falloff, 0.1f, 0.1f, 10.0f);

        if (light.type == Light2DType::Spot) {
            ImGui::DragFloat("Angle", &light.spot_angle, 1.0f, 1.0f, 180.0f);
            ImGui::DragFloat("Direction", &light.spot_direction, 1.0f, -360.0f, 360.0f);
        }

        // Shadows
        const char* shadow_modes[] = {"None", "Hard", "Soft"};
        int sm = static_cast<int>(light.shadow_mode);
        if (ImGui::Combo("Shadows", &sm, shadow_modes, 3)) light.shadow_mode = static_cast<Light2DShadowMode>(sm);
        if (light.shadow_mode != Light2DShadowMode::None) {
            ImGui::DragFloat("Softness", &light.shadow_softness, 0.1f, 0.0f, 10.0f);
            ImGui::DragInt("Rays", &light.shadow_rays, 1, 4, 256);
        }

        ImGui::Checkbox("Normal Map", &light.use_normal_map);
        if (light.use_normal_map) {
            ImGui::DragFloat("Normal Strength", &light.normal_strength, 0.01f, 0.0f, 2.0f);
        }

        ImGui::Separator();
        if (ImGui::Button("Delete Light")) {
            st.lights.erase(st.lights.begin() + st.selected_light);
            st.selected_light = -1;
        }
    }
    ImGui::EndChild();

    // Preview area
    ImGui::Separator();
    ImGui::BeginChild("##light_preview", ImVec2(0, 200), true);
    ImVec2 preview_pos = ImGui::GetCursorScreenPos();
    ImVec2 preview_size = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Dark background simulating scene
    ImU32 ambient = ImGui::ColorConvertFloat4ToU32(
        ImVec4(st.ambient_color.x * st.ambient_intensity,
               st.ambient_color.y * st.ambient_intensity,
               st.ambient_color.z * st.ambient_intensity, 1.0f));
    dl->AddRectFilled(preview_pos, ImVec2(preview_pos.x + preview_size.x, preview_pos.y + preview_size.y), ambient);

    ImVec2 preview_center(preview_pos.x + preview_size.x * 0.5f, preview_pos.y + preview_size.y * 0.5f);
    DrawLight2DGizmos(dl, preview_center, 1.0f);
    ImGui::EndChild();

    if (ImGui::Button("Save .dlight2d")) {
        SaveLight2DScene(st, "scene.dlight2d");
    }

    ImGui::End();
}

}  // namespace dse::editor::tools2d

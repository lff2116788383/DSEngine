#include "editor_aux_panels.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_console_panel.h"
#include "editor_icons.h"
#include "editor_tilemap_panel.h"

#include <glad/gl.h>
#include "stb/stb_image.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#include <Windows.h>
#include <shellapi.h>
#endif

namespace {

std::filesystem::path GetProjectRootPath() {
    return std::filesystem::current_path().lexically_normal();
}

std::filesystem::path GetProjectBaseDataPath() {
    static std::filesystem::path base_data_path = []() {
        std::filesystem::path p = GetProjectRootPath();
        std::filesystem::path target_path = p / "samples" / "lua" / "data";
        if (!std::filesystem::exists(target_path)) {
            try {
                std::filesystem::create_directories(target_path);
            } catch (...) {
                return p;
            }
        }
        return target_path;
    }();
    return base_data_path;
}

std::filesystem::path& GetCurrentProjectPanelPath() {
    static std::filesystem::path current_path = GetProjectBaseDataPath();
    return current_path;
}

// --- Thumbnail cache ---
struct ThumbnailEntry {
    unsigned int texture_id = 0;
    int width = 0;
    int height = 0;
};

std::unordered_map<std::string, ThumbnailEntry>& GetThumbnailCache() {
    static std::unordered_map<std::string, ThumbnailEntry> cache;
    return cache;
}

std::string& GetThumbnailCacheDir() {
    static std::string dir;
    return dir;
}

void ClearThumbnailCache() {
    auto& cache = GetThumbnailCache();
    for (auto& [path, entry] : cache) {
        if (entry.texture_id != 0) {
            glDeleteTextures(1, &entry.texture_id);
        }
    }
    cache.clear();
}

bool IsImageExtension(const std::string& ext) {
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}

unsigned int LoadThumbnailTexture(const std::filesystem::path& path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) return 0;

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    auto& cache = GetThumbnailCache();
    cache[path.string()] = {tex, w, h};
    return tex;
}

} // namespace

namespace dse::editor {

void DrawProjectPanel() {
    static char s_search_filter[128] = "";
    static bool s_grid_view = false;
    static std::filesystem::path s_rename_target;
    static char s_rename_buf[128] = "";

    ImGui::Begin("Project");

    const std::filesystem::path base_data_path = GetProjectBaseDataPath();
    std::filesystem::path& current_path = GetCurrentProjectPanelPath();

    // Toolbar: Search + View toggle
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
    ImGui::InputTextWithHint("##project_search", MDI_ICON_MAGNIFY " Search...", s_search_filter, sizeof(s_search_filter));
    ImGui::SameLine();
    if (ImGui::Button(s_grid_view ? "List" : "Grid", ImVec2(60, 0))) {
        s_grid_view = !s_grid_view;
    }
    ImGui::Separator();

    // Clear thumbnail cache when directory changes
    {
        std::string& cached_dir = GetThumbnailCacheDir();
        if (cached_dir != current_path.string()) {
            ClearThumbnailCache();
            cached_dir = current_path.string();
        }
    }

    // Breadcrumb / Back navigation
    if (current_path != base_data_path) {
        if (ImGui::Button("<- Back")) {
            current_path = current_path.parent_path();
        }
        ImGui::SameLine();
        std::string rel = std::filesystem::relative(current_path, base_data_path).string();
        ImGui::TextDisabled("/ %s", rel.c_str());
        ImGui::Separator();
    }

    // Background context menu (create new assets)
    if (ImGui::BeginPopupContextWindow("ProjectContextMenu")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                std::filesystem::create_directory(current_path / "NewFolder");
            }
            if (ImGui::MenuItem("Lua Script")) {
                std::ofstream ofs(current_path / "NewScript.lua");
                if (ofs.is_open()) ofs << "-- New Lua Script\n";
            }
            if (ImGui::MenuItem("Material")) {
                std::ofstream ofs(current_path / "NewMaterial.mat");
                if (ofs.is_open()) ofs << "{\n  \"shader\": \"default\",\n  \"color\": [1,1,1,1]\n}\n";
            }
            ImGui::EndMenu();
        }
#if defined(_WIN32)
        if (ImGui::MenuItem("Show in Explorer")) {
            ShellExecuteW(nullptr, L"open", current_path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
#endif
        ImGui::EndPopup();
    }

    if (!std::filesystem::exists(current_path)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Data path not found: %s", current_path.string().c_str());
    } else {
        // Collect and filter entries
        std::vector<std::filesystem::directory_entry> entries;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
                const std::string filename = entry.path().filename().string();
                if (s_search_filter[0] != '\0') {
                    std::string lower_name = filename;
                    std::string lower_filter = s_search_filter;
                    for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    for (auto& c : lower_filter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (lower_name.find(lower_filter) == std::string::npos) continue;
                }
                entries.push_back(entry);
            }
        } catch (const std::filesystem::filesystem_error&) {}

        // Sort: directories first, then alphabetical
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.is_directory() != b.is_directory()) return a.is_directory();
            return a.path().filename().string() < b.path().filename().string();
        });

        if (s_grid_view) {
            // Grid view
            const float cell_size = 80.0f;
            const float panel_width = ImGui::GetContentRegionAvail().x;
            int columns = (std::max)(1, static_cast<int>(panel_width / cell_size));
            int col = 0;

            for (const auto& entry : entries) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();
                ImGui::PushID(filename.c_str());

                ImGui::BeginGroup();
                ImVec2 p = ImGui::GetCursorScreenPos();
                const float thumb_w = cell_size - 8;
                const float thumb_h = cell_size - 24;

                // Check if this is an image file for thumbnail display
                std::string ext = path.extension().string();
                for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                bool is_image = !entry.is_directory() && IsImageExtension(ext);

                if (is_image) {
                    auto& cache = GetThumbnailCache();
                    auto it = cache.find(path.string());
                    unsigned int tex_id = 0;
                    if (it != cache.end()) {
                        tex_id = it->second.texture_id;
                    } else {
                        tex_id = LoadThumbnailTexture(path);
                    }
                    if (tex_id != 0) {
                        ImGui::Image((ImTextureID)(intptr_t)tex_id, ImVec2(thumb_w, thumb_h));
                    } else {
                        ImU32 bg_color = IM_COL32(80, 60, 80, 180);
                        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + thumb_w, p.y + thumb_h), bg_color, 4.0f);
                        ImGui::Dummy(ImVec2(thumb_w, thumb_h));
                    }
                } else {
                    ImU32 bg_color = entry.is_directory() ? IM_COL32(60, 80, 120, 180) : IM_COL32(60, 60, 70, 180);
                    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + thumb_w, p.y + thumb_h), bg_color, 4.0f);
                    ImGui::Dummy(ImVec2(thumb_w, thumb_h));
                }

                // Truncate long filenames
                std::string display_name = filename.size() > 10 ? filename.substr(0, 9) + "..." : filename;
                ImGui::TextUnformatted(display_name.c_str());
                ImGui::EndGroup();

                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0) && entry.is_directory()) {
                    current_path /= path.filename();
                }

                // Drag & Drop source for files
                if (!entry.is_directory() && ImGui::BeginDragDropSource()) {
                    const std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                    ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                    ImGui::Text("%s", filename.c_str());
                    ImGui::EndDragDropSource();
                }

                // Per-item context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        s_rename_target = path;
                        std::strncpy(s_rename_buf, filename.c_str(), sizeof(s_rename_buf) - 1);
                        s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
                    }
                    if (ImGui::MenuItem("Delete")) {
                        try { std::filesystem::remove_all(path); } catch (...) {}
                    }
                    if (ImGui::MenuItem("Copy Path")) {
                        ImGui::SetClipboardText(path.string().c_str());
                    }
#if defined(_WIN32)
                    if (ImGui::MenuItem("Show in Explorer")) {
                        std::wstring cmd = L"/select,\"" + path.wstring() + L"\"";
                        ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
                    }
#endif
                    ImGui::EndPopup();
                }

                ImGui::PopID();
                col++;
                if (col < columns) {
                    ImGui::SameLine();
                } else {
                    col = 0;
                }
            }
        } else {
            // List view
            for (const auto& entry : entries) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();

                // Inline rename
                if (s_rename_target == path) {
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::InputText("##rename_project", s_rename_buf, sizeof(s_rename_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        try {
                            std::filesystem::rename(path, path.parent_path() / s_rename_buf);
                        } catch (...) {}
                        s_rename_target.clear();
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        s_rename_target.clear();
                    }
                    continue;
                }

                if (entry.is_directory()) {
                    if (ImGui::Selectable((std::string(MDI_ICON_PACKAGE_VARIANT " ") + filename).c_str())) {
                        current_path /= path.filename();
                    }
                } else {
                    ImGui::Selectable((std::string(MDI_ICON_IMAGE " ") + filename).c_str());

                    if (ImGui::BeginDragDropSource()) {
                        const std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                        ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                        ImGui::Text("%s", filename.c_str());
                        ImGui::EndDragDropSource();
                    }
                }

                // Per-item context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        s_rename_target = path;
                        std::strncpy(s_rename_buf, filename.c_str(), sizeof(s_rename_buf) - 1);
                        s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
                    }
                    if (ImGui::MenuItem("Delete")) {
                        try { std::filesystem::remove_all(path); } catch (...) {}
                    }
                    if (ImGui::MenuItem("Copy Path")) {
                        ImGui::SetClipboardText(path.string().c_str());
                    }
#if defined(_WIN32)
                    if (ImGui::MenuItem("Show in Explorer")) {
                        std::wstring cmd = L"/select,\"" + path.wstring() + L"\"";
                        ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
                    }
#endif
                    ImGui::EndPopup();
                }
            }
        }
    }

    ImGui::End();
}

void DrawConsolePanel() {
    DrawConsolePanelImpl();
}

void DrawLocalizationPreviewPanel(EditorContext& ctx,
                                  char* key_buf, std::size_t key_size,
                                  char* fallback_buf, std::size_t fallback_size) {
    ImGui::Begin("Localization Preview");
    auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
    ImGui::Text("Current Language: %s", localization.GetCurrentLanguage().c_str());
    if (ctx.read_only) {
        ImGui::BeginDisabled(true);
    }
    ImGui::InputText("Preview Key", key_buf, key_size);
    ImGui::InputText("Fallback", fallback_buf, fallback_size);

    std::unordered_map<std::string, std::string> preview_params;
    preview_params["lang"] = localization.GetCurrentLanguage();
    preview_params["entity"] = ctx.selected_entity == entt::null
        ? std::string("None")
        : std::to_string(static_cast<uint32_t>(ctx.selected_entity));

    const std::string preview_text = localization.GetTextWithParams(
        key_buf,
        preview_params,
        fallback_buf);

    ImGui::Separator();
    ImGui::TextWrapped("%s", preview_text.c_str());

    if (ctx.selected_entity != entt::null &&
        ctx.registry.valid(ctx.selected_entity) &&
        ctx.registry.all_of<UILabelComponent>(ctx.selected_entity)) {
        if (ImGui::Button("Apply To Selected UILabel")) {
            auto& label = ctx.registry.get<UILabelComponent>(ctx.selected_entity);
            label.use_localization = true;
            label.localization_key = key_buf;
            label.fallback_text = fallback_buf;
            label.localization_params = preview_params;
            label.dirty = true;
        }
    } else {
        ImGui::TextDisabled("Select a UILabel entity to apply preview settings.");
    }

    if (ctx.read_only) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Play 模式下已禁用本地化预览写入。请退出 Play 后应用到 UILabel。");
    }

    ImGui::End();
}

void DrawAnimationPanel(EditorContext& ctx) {
    auto& registry = ctx.registry;
    auto selected_entity = ctx.selected_entity;
    ImGui::Begin("Animation");

    // Check if selected entity has Animator3DComponent
    bool has_animator = (selected_entity != entt::null &&
                         registry.valid(selected_entity) &&
                         registry.all_of<dse::Animator3DComponent>(selected_entity));

    if (!has_animator) {
        ImGui::TextDisabled("Select an entity with Animator3DComponent to view its timeline.");
        ImGui::End();
        return;
    }

    auto& animator = registry.get<dse::Animator3DComponent>(selected_entity);

    // Animation panel persistent state
    static bool s_playing = false;
    static float s_timeline_zoom = 1.0f;
    static float s_timeline_scroll_x = 0.0f;
    static float s_scrub_time = 0.0f;
    static bool s_dragging_playhead = false;

    // Clip info
    const float clip_duration = 2.0f; // Default 2 seconds if unknown
    const float fps = 30.0f;
    const int total_frames = static_cast<int>(clip_duration * fps);

    // Transport controls
    if (ImGui::Button(s_playing ? MDI_ICON_PAUSE : MDI_ICON_PLAY)) {
        s_playing = !s_playing;
    }
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_STOP)) {
        s_playing = false;
        s_scrub_time = 0.0f;
        animator.current_time = 0.0f;
    }
    ImGui::SameLine();
    if (ImGui::Button(MDI_ICON_SKIP_NEXT)) {
        s_scrub_time = clip_duration;
        animator.current_time = clip_duration;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("Speed", &animator.speed, 0.0f, 3.0f, "%.1fx");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("Zoom", &s_timeline_zoom, 0.5f, 4.0f, "%.1f");

    // Current time display
    ImGui::Text("Time: %.2fs / %.2fs  Frame: %d / %d",
                s_scrub_time, clip_duration,
                static_cast<int>(s_scrub_time * fps), total_frames);
    ImGui::Text("Clip: %s", animator.danim_path.empty() ? "(none)" : animator.danim_path.c_str());

    ImGui::Separator();

    // Timeline area
    ImVec2 timeline_pos = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float timeline_width = avail.x;
    float timeline_height = (std::max)(80.0f, avail.y - 4.0f);

    ImGui::InvisibleButton("##timeline_area", ImVec2(timeline_width, timeline_height));
    bool timeline_hovered = ImGui::IsItemHovered();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 timeline_end = ImVec2(timeline_pos.x + timeline_width, timeline_pos.y + timeline_height);

    // Background
    draw_list->AddRectFilled(timeline_pos, timeline_end, IM_COL32(30, 30, 35, 255));
    draw_list->AddRect(timeline_pos, timeline_end, IM_COL32(60, 60, 70, 255));

    // Ruler area (top 24px)
    const float ruler_height = 24.0f;
    draw_list->AddRectFilled(timeline_pos,
                             ImVec2(timeline_end.x, timeline_pos.y + ruler_height),
                             IM_COL32(40, 40, 50, 255));

    // Draw frame ticks
    float pixels_per_second = (timeline_width / clip_duration) * s_timeline_zoom;
    float visible_start = s_timeline_scroll_x / pixels_per_second;

    for (int f = 0; f <= total_frames; f++) {
        float time_at_frame = static_cast<float>(f) / fps;
        float x = timeline_pos.x + (time_at_frame - visible_start) * pixels_per_second;
        if (x < timeline_pos.x || x > timeline_end.x) continue;

        bool is_major = (f % 10 == 0);
        float tick_h = is_major ? ruler_height - 4.0f : 8.0f;
        ImU32 tick_color = is_major ? IM_COL32(180, 180, 180, 255) : IM_COL32(80, 80, 90, 255);
        draw_list->AddLine(ImVec2(x, timeline_pos.y + ruler_height - tick_h),
                           ImVec2(x, timeline_pos.y + ruler_height), tick_color);

        if (is_major) {
            char label[16];
            snprintf(label, sizeof(label), "%d", f);
            draw_list->AddText(ImVec2(x + 2, timeline_pos.y + 2), IM_COL32(200, 200, 200, 255), label);
        }
    }

    // Draw keyframe diamonds (simulated — show one every 10 frames)
    float track_y = timeline_pos.y + ruler_height + 20.0f;
    for (int f = 0; f <= total_frames; f += 10) {
        float time_at_frame = static_cast<float>(f) / fps;
        float x = timeline_pos.x + (time_at_frame - visible_start) * pixels_per_second;
        if (x < timeline_pos.x || x > timeline_end.x) continue;

        // Diamond shape
        const float diamond_size = 5.0f;
        ImVec2 center(x, track_y);
        draw_list->AddQuadFilled(
            ImVec2(center.x, center.y - diamond_size),
            ImVec2(center.x + diamond_size, center.y),
            ImVec2(center.x, center.y + diamond_size),
            ImVec2(center.x - diamond_size, center.y),
            IM_COL32(255, 200, 50, 255));
    }

    // Track label
    draw_list->AddText(ImVec2(timeline_pos.x + 4, track_y - 6),
                       IM_COL32(160, 160, 160, 255), "Transform");

    // Current frame indicator (red playhead)
    {
        float playhead_x = timeline_pos.x + (s_scrub_time - visible_start) * pixels_per_second;
        if (playhead_x >= timeline_pos.x && playhead_x <= timeline_end.x) {
            draw_list->AddLine(ImVec2(playhead_x, timeline_pos.y),
                               ImVec2(playhead_x, timeline_end.y),
                               IM_COL32(220, 50, 50, 255), 2.0f);
            // Playhead triangle at top
            draw_list->AddTriangleFilled(
                ImVec2(playhead_x - 5, timeline_pos.y),
                ImVec2(playhead_x + 5, timeline_pos.y),
                ImVec2(playhead_x, timeline_pos.y + 8),
                IM_COL32(220, 50, 50, 255));
        }
    }

    // Scrub interaction: click/drag on timeline to move playhead
    if (timeline_hovered && ImGui::IsMouseClicked(0)) {
        s_dragging_playhead = true;
    }
    if (s_dragging_playhead) {
        if (ImGui::IsMouseDown(0)) {
            float mouse_x = ImGui::GetMousePos().x;
            float rel = (mouse_x - timeline_pos.x) / pixels_per_second + visible_start;
            s_scrub_time = (std::max)(0.0f, (std::min)(rel, clip_duration));
            animator.current_time = s_scrub_time;
            s_playing = false;
        } else {
            s_dragging_playhead = false;
        }
    }

    // Pan with middle mouse
    if (timeline_hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        s_timeline_scroll_x -= ImGui::GetIO().MouseDelta.x;
        s_timeline_scroll_x = (std::max)(0.0f, s_timeline_scroll_x);
    }

    // Zoom with scroll wheel
    if (timeline_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            s_timeline_zoom += wheel * 0.15f;
            s_timeline_zoom = (std::max)(0.5f, (std::min)(s_timeline_zoom, 4.0f));
        }
    }

    // Advance playback
    if (s_playing) {
        s_scrub_time += ImGui::GetIO().DeltaTime * animator.speed;
        if (s_scrub_time >= clip_duration) {
            if (animator.loop) {
                s_scrub_time -= clip_duration;
            } else {
                s_scrub_time = clip_duration;
                s_playing = false;
            }
        }
        animator.current_time = s_scrub_time;
    }

    ImGui::End();
}

void DrawTilePalettePanel(EditorContext& ctx) {
    DrawTilemapEditorPanel(ctx.registry, ctx.selected_entity);
}

} // namespace dse::editor

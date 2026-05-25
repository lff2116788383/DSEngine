#include "editor_asset_browser.h"
#include "editor_asset_db.h"
#include "editor_icons.h"
#include "editor_project.h"

#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace dse::editor {

std::string& GetPendingAssetOpenPath() {
    static std::string s_path;
    return s_path;
}

namespace {

struct AssetBrowserState {
    char search_filter[128] = "";
    int view_mode = 0;           // 0=Grid, 1=List
    float thumbnail_size = 80.0f;
    int type_filter = -1;        // -1=All, else cast of AssetType
    std::string current_dir;     // relative path of current directory (empty = root)
    std::string drag_payload_guid;
};

// ─── Thumbnail Cache ─────────────────────────────────────────────────────────
struct ThumbnailEntry {
    unsigned int texture_id = 0;  // OpenGL texture handle
    bool loaded = false;
    bool failed = false;
};

std::unordered_map<std::string, ThumbnailEntry>& GetThumbnailCache() {
    static std::unordered_map<std::string, ThumbnailEntry> cache;
    return cache;
}

unsigned int GetThumbnailForAsset(const AssetInfo& asset) {
    if (asset.type != AssetType::Texture) return 0;
    auto& cache = GetThumbnailCache();
    auto it = cache.find(asset.relative_path);
    if (it != cache.end()) {
        return it->second.failed ? 0 : it->second.texture_id;
    }
    // Lazy load
    ThumbnailEntry entry{};
    auto* am = dse::core::ServiceLocator::Instance().Get<AssetManager>();
    if (am) {
        auto tex = am->LoadTexture(asset.relative_path);
        if (tex && tex->GetHandle() != 0) {
            entry.texture_id = tex->GetHandle();
            entry.loaded = true;
        } else {
            entry.failed = true;
        }
    } else {
        entry.failed = true;
    }
    cache[asset.relative_path] = entry;
    return entry.texture_id;
}

AssetBrowserState& GetState() {
    static AssetBrowserState state;
    return state;
}

const char* GetAssetTypeIcon(AssetType type) {
    switch (type) {
        case AssetType::Mesh:      return MDI_ICON_CUBE_OUTLINE;
        case AssetType::Material:  return MDI_ICON_PALETTE;
        case AssetType::Animation: return MDI_ICON_ANIMATION;
        case AssetType::Skeleton:  return MDI_ICON_BONE;
        case AssetType::Texture:   return MDI_ICON_IMAGE;
        case AssetType::Audio:     return MDI_ICON_VOLUME_HIGH;
        case AssetType::Scene:     return MDI_ICON_VIEW_IN_AR;
        case AssetType::Prefab:    return MDI_ICON_CONTENT_COPY;
        case AssetType::Script:    return MDI_ICON_FILE;
        case AssetType::Pak:       return MDI_ICON_ZIP_BOX;
        default:                   return MDI_ICON_FILE;
    }
}

ImU32 GetAssetTypeColor(AssetType type) {
    switch (type) {
        case AssetType::Mesh:      return IM_COL32(100, 180, 255, 255);
        case AssetType::Material:  return IM_COL32(200, 120, 255, 255);
        case AssetType::Animation: return IM_COL32(255, 180, 80, 255);
        case AssetType::Skeleton:  return IM_COL32(220, 220, 220, 255);
        case AssetType::Texture:   return IM_COL32(80, 220, 120, 255);
        case AssetType::Audio:     return IM_COL32(255, 100, 100, 255);
        case AssetType::Scene:     return IM_COL32(100, 255, 200, 255);
        case AssetType::Prefab:    return IM_COL32(180, 180, 255, 255);
        case AssetType::Script:    return IM_COL32(255, 220, 100, 255);
        case AssetType::Pak:       return IM_COL32(180, 180, 180, 255);
        default:                   return IM_COL32(160, 160, 160, 255);
    }
}

std::string FormatFileSize(int64_t size) {
    if (size < 1024) return std::to_string(size) + " B";
    if (size < 1024 * 1024) return std::to_string(size / 1024) + " KB";
    return std::to_string(size / (1024 * 1024)) + " MB";
}

bool MatchesFilter(const AssetInfo& asset, const char* filter, int type_filter,
                   const std::string& current_dir) {
    // Directory filter
    if (!current_dir.empty()) {
        if (asset.relative_path.find(current_dir) != 0) return false;
        // Only show immediate children (no deeper nesting)
        auto remaining = asset.relative_path.substr(current_dir.size());
        if (!remaining.empty() && remaining[0] == '/') remaining = remaining.substr(1);
        if (remaining.find('/') != std::string::npos) return false;
    }

    // Type filter
    if (type_filter >= 0 && static_cast<int>(asset.type) != type_filter) return false;

    // Text filter
    if (filter[0] != '\0') {
        std::string lower_name = asset.display_name;
        std::string lower_filter(filter);
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
        std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
        if (lower_name.find(lower_filter) == std::string::npos) return false;
    }
    return true;
}

/// Collect unique subdirectories under current_dir
std::vector<std::string> CollectSubdirs(const std::vector<AssetInfo>& all,
                                         const std::string& current_dir) {
    std::vector<std::string> dirs;
    for (const auto& a : all) {
        std::string rel = a.relative_path;
        if (!current_dir.empty()) {
            if (rel.find(current_dir) != 0) continue;
            rel = rel.substr(current_dir.size());
            if (!rel.empty() && rel[0] == '/') rel = rel.substr(1);
        }
        auto slash = rel.find('/');
        if (slash != std::string::npos) {
            std::string subdir = rel.substr(0, slash);
            if (std::find(dirs.begin(), dirs.end(), subdir) == dirs.end()) {
                dirs.push_back(subdir);
            }
        }
    }
    std::sort(dirs.begin(), dirs.end());
    return dirs;
}

void DrawGridItem(ImDrawList* dl, const AssetInfo& asset, float size) {
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 item_max(cursor.x + size, cursor.y + size + 20.0f);

    // Background
    bool hovered = ImGui::IsMouseHoveringRect(cursor, item_max);
    ImU32 bg = hovered ? IM_COL32(60, 60, 80, 200) : IM_COL32(40, 40, 50, 180);
    dl->AddRectFilled(cursor, item_max, bg, 4.0f);

    // Thumbnail or type icon
    unsigned int thumb_id = GetThumbnailForAsset(asset);
    if (thumb_id != 0) {
        // Render actual texture thumbnail
        float pad = 4.0f;
        ImVec2 uv0(0, 0), uv1(1, 1);
        dl->AddImage((ImTextureID)(intptr_t)(thumb_id),
                     ImVec2(cursor.x + pad, cursor.y + pad),
                     ImVec2(cursor.x + size - pad, cursor.y + size - pad),
                     uv0, uv1);
    } else {
        // Fallback: type icon (large, centered)
        ImU32 icon_color = GetAssetTypeColor(asset.type);
        const char* icon = GetAssetTypeIcon(asset.type);
        ImVec2 icon_size = ImGui::CalcTextSize(icon);
        float icon_scale_x = cursor.x + (size - icon_size.x) * 0.5f;
        float icon_scale_y = cursor.y + (size - icon_size.y) * 0.5f - 4.0f;
        dl->AddText(ImVec2(icon_scale_x, icon_scale_y), icon_color, icon);
    }

    // Type badge (small colored strip at bottom of icon area)
    ImU32 badge_color = GetAssetTypeColor(asset.type);
    ImVec2 badge_min(cursor.x, cursor.y + size - 3.0f);
    ImVec2 badge_max(cursor.x + size, cursor.y + size);
    dl->AddRectFilled(badge_min, badge_max, badge_color & 0x80FFFFFF, 0.0f);

    // Filename text (truncated)
    ImVec2 text_pos(cursor.x + 2.0f, cursor.y + size + 2.0f);
    ImGui::PushClipRect(ImVec2(cursor.x, cursor.y + size), ImVec2(cursor.x + size, item_max.y), true);
    dl->AddText(text_pos, IM_COL32(220, 220, 220, 255), asset.display_name.c_str());
    ImGui::PopClipRect();

    // Invisible button for interaction
    ImGui::SetCursorScreenPos(cursor);
    char id[64];
    snprintf(id, sizeof(id), "##asset_%s", asset.guid.c_str());
    ImGui::InvisibleButton(id, ImVec2(size, size + 20.0f));

    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ASSET_PATH", asset.relative_path.c_str(),
                                   asset.relative_path.size() + 1);
        ImGui::Text("%s %s", GetAssetTypeIcon(asset.type), asset.display_name.c_str());
        ImGui::EndDragDropSource();
    }

    // Tooltip
    if (hovered) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", GetAssetTypeIcon(asset.type), asset.display_name.c_str());
        ImGui::TextDisabled("Type: %s", AssetTypeToString(asset.type));
        ImGui::TextDisabled("Size: %s", FormatFileSize(asset.file_size).c_str());
        ImGui::TextDisabled("Path: %s", asset.relative_path.c_str());
        ImGui::TextDisabled("GUID: %s", asset.guid.c_str());
        ImGui::EndTooltip();
    }

    // Double click to open (scenes) — copies path; host loop handles actual open
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        if (asset.type == AssetType::Scene) {
            GetPendingAssetOpenPath() = asset.absolute_path;
        }
    }
}

} // namespace

void InvalidateThumbnailCache() {
    GetThumbnailCache().clear();
}

void DrawAssetBrowserPanel() {
    ImGui::Begin("Asset Browser");

    auto& db = AssetDatabase::Get();
    auto& state = GetState();

    if (!db.IsValid()) {
        ImGui::TextDisabled("No project open or asset database not initialized.");
        ImGui::End();
        return;
    }

    // Toolbar
    // Navigation breadcrumb
    {
        if (ImGui::SmallButton(MDI_ICON_HOME "##root")) {
            state.current_dir.clear();
        }
        if (!state.current_dir.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();

            // Split current_dir into parts for breadcrumb
            std::string accum;
            std::string remaining = state.current_dir;
            while (!remaining.empty()) {
                auto slash = remaining.find('/');
                std::string part = (slash != std::string::npos) ? remaining.substr(0, slash) : remaining;
                if (!accum.empty()) accum += "/";
                accum += part;

                if (slash != std::string::npos) {
                    std::string link_target = accum;
                    if (ImGui::SmallButton(part.c_str())) {
                        state.current_dir = link_target;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("/");
                    ImGui::SameLine();
                    remaining = remaining.substr(slash + 1);
                } else {
                    ImGui::Text("%s", part.c_str());
                    remaining.clear();
                }
            }
        }
    }

    // Search + filter bar
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputTextWithHint("##asset_search", "Search assets...", state.search_filter, sizeof(state.search_filter));
    ImGui::SameLine();

    // Type filter combo
    {
        const char* type_names[] = { "All", "Mesh", "Material", "Animation", "Skeleton",
                                      "Texture", "Audio", "Scene", "Prefab", "Script", "Pak" };
        int combo_idx = (state.type_filter < 0) ? 0 : state.type_filter + 1;
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::Combo("##type_filter", &combo_idx, type_names, IM_ARRAYSIZE(type_names))) {
            state.type_filter = (combo_idx == 0) ? -1 : combo_idx - 1;
        }
    }
    ImGui::SameLine();

    // View mode toggle
    if (ImGui::Button(state.view_mode == 0 ? MDI_ICON_VIEW_MODULE : MDI_ICON_VIEW_LIST)) {
        state.view_mode = 1 - state.view_mode;
    }
    if (state.view_mode == 0) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        ImGui::SliderFloat("##thumb_size", &state.thumbnail_size, 48.0f, 128.0f, "%.0f");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%d assets)", static_cast<int>(db.Count()));

    ImGui::Separator();

    // Content area
    ImGui::BeginChild("AssetContent", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    const auto& all_assets = db.GetAll();

    // Subdirectories first
    auto subdirs = CollectSubdirs(all_assets, state.current_dir);
    for (const auto& dir : subdirs) {
        if (state.view_mode == 1) {
            // List mode
            char lbl[256];
            snprintf(lbl, sizeof(lbl), "%s  %s/", MDI_ICON_FOLDER, dir.c_str());
            if (ImGui::Selectable(lbl, false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    state.current_dir = state.current_dir.empty()
                        ? dir : (state.current_dir + "/" + dir);
                }
            }
        } else {
            // Grid mode - folder card
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float sz = state.thumbnail_size;
            ImVec2 item_max(cursor.x + sz, cursor.y + sz + 20.0f);
            dl->AddRectFilled(cursor, item_max, IM_COL32(50, 50, 60, 200), 4.0f);
            ImVec2 icon_sz = ImGui::CalcTextSize(MDI_ICON_FOLDER);
            dl->AddText(ImVec2(cursor.x + (sz - icon_sz.x) * 0.5f, cursor.y + (sz - icon_sz.y) * 0.5f - 4.0f),
                        IM_COL32(255, 200, 80, 255), MDI_ICON_FOLDER);
            ImVec2 text_pos(cursor.x + 2.0f, cursor.y + sz + 2.0f);
            ImGui::PushClipRect(ImVec2(cursor.x, cursor.y + sz), ImVec2(cursor.x + sz, item_max.y), true);
            dl->AddText(text_pos, IM_COL32(220, 220, 220, 255), dir.c_str());
            ImGui::PopClipRect();

            ImGui::SetCursorScreenPos(cursor);
            char id[128];
            snprintf(id, sizeof(id), "##dir_%s", dir.c_str());
            if (ImGui::InvisibleButton(id, ImVec2(sz, sz + 20.0f))) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    state.current_dir = state.current_dir.empty()
                        ? dir : (state.current_dir + "/" + dir);
                }
            }

            // Grid layout
            float avail = ImGui::GetContentRegionAvail().x;
            float next_x = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
            if (next_x + sz < ImGui::GetWindowPos().x + avail) {
                ImGui::SameLine();
            }
        }
    }

    // Assets
    if (state.view_mode == 1) {
        // List view
        ImGui::Separator();
        for (const auto& asset : all_assets) {
            if (!MatchesFilter(asset, state.search_filter, state.type_filter, state.current_dir)) continue;

            ImU32 icon_col = GetAssetTypeColor(asset.type);
            ImGui::PushStyleColor(ImGuiCol_Text, ImColor(icon_col).Value);
            ImGui::TextUnformatted(GetAssetTypeIcon(asset.type));
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("%-30s", asset.display_name.c_str());
            ImGui::SameLine(0, 20);
            ImGui::TextDisabled("%-12s %8s", AssetTypeToString(asset.type),
                               FormatFileSize(asset.file_size).c_str());

            // Drag source in list view
            if (ImGui::IsItemHovered() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("ASSET_PATH", asset.relative_path.c_str(), asset.relative_path.size() + 1);
                ImGui::Text("%s %s", GetAssetTypeIcon(asset.type), asset.display_name.c_str());
                ImGui::EndDragDropSource();
            }
        }
    } else {
        // Grid view
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float sz = state.thumbnail_size;
        for (const auto& asset : all_assets) {
            if (!MatchesFilter(asset, state.search_filter, state.type_filter, state.current_dir)) continue;
            DrawGridItem(dl, asset, sz);

            // Grid layout
            float avail = ImGui::GetContentRegionAvail().x;
            float next_x = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
            if (next_x + sz < ImGui::GetWindowPos().x + avail) {
                ImGui::SameLine();
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace dse::editor

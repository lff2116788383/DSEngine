#include "editor_aux_panels.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_console_panel.h"
#include "editor_icons.h"
#include "editor_tilemap_panel.h"
#include "editor_project.h"
#include "editor_asset_db.h"

#include <glad/gl.h>
#include "stb/stb_image.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>

#include <rapidjson/document.h>
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/raw_scene_data.h"
#include "engine/runtime/engine_app.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif

namespace {

std::filesystem::path GetProjectRootPath() {
    return std::filesystem::current_path().lexically_normal();
}

std::filesystem::path GetProjectBaseDataPath() {
    auto& proj_mgr = dse::editor::ProjectManager::Get();
    if (proj_mgr.HasOpenProject()) {
        auto asset_dir = proj_mgr.GetAssetDir();
        if (!std::filesystem::exists(asset_dir)) {
            std::error_code ec;
            std::filesystem::create_directories(asset_dir, ec);
        }
        return asset_dir;
    }

    static std::filesystem::path fallback_path = []() {
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
    return fallback_path;
}

std::filesystem::path& GetCurrentProjectPanelPath() {
    static std::filesystem::path current_path = GetProjectBaseDataPath();
    static bool s_last_has_project = false;

    bool has_project = dse::editor::ProjectManager::Get().HasOpenProject();
    if (has_project != s_last_has_project) {
        current_path = GetProjectBaseDataPath();
        s_last_has_project = has_project;
    }
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

static constexpr uint32_t kThumbMagic   = 0x4D485444u; // 'DTHM'
static constexpr uint32_t kThumbVersion = 1u;
static constexpr int      kThumbSize    = 128;

static uint64_t FNV1aPath(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

static int64_t ThumbGetMtime(const std::filesystem::path& p) {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(p, ec);
    return ec ? -1LL : static_cast<int64_t>(t.time_since_epoch().count());
}

static std::filesystem::path GetThumbFilePath(const std::filesystem::path& src) {
    const auto& asset_root = dse::editor::AssetDatabase::Get().GetAssetRoot();
    if (asset_root.empty()) return {};
    const std::filesystem::path cache_dir = asset_root.parent_path() / "Cache" / "thumbnails";
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << FNV1aPath(src.string()) << ".thumb";
    return cache_dir / oss.str();
}

static void BoxResize(const uint8_t* src, int sw, int sh,
                      uint8_t* dst, int dw, int dh) {
    for (int dy = 0; dy < dh; ++dy) {
        for (int dx = 0; dx < dw; ++dx) {
            const int sx0 = dx * sw / dw;
            const int sx1 = (std::max)(sx0 + 1, (dx + 1) * sw / dw);
            const int sy0 = dy * sh / dh;
            const int sy1 = (std::max)(sy0 + 1, (dy + 1) * sh / dh);
            uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int sy = sy0; sy < sy1; ++sy)
                for (int sx = sx0; sx < sx1; ++sx) {
                    const uint8_t* p = src + (sy * sw + sx) * 4;
                    r += p[0]; g += p[1]; b += p[2]; a += p[3]; ++n;
                }
            uint8_t* d = dst + (dy * dw + dx) * 4;
            d[0] = uint8_t(r/n); d[1] = uint8_t(g/n); d[2] = uint8_t(b/n); d[3] = uint8_t(a/n);
        }
    }
}

// 生成 CPU Phong 着色球体预览（用于 .dmat / .dmesh 缩略图）
// base_color: [0,1] RGB, emissive: [0,1] RGB, roughness: [0,1]
static unsigned int GenerateSpherePreviewTexture(float r, float g, float b,
                                                 float er, float eg, float eb,
                                                 float roughness) {
    constexpr int sz = kThumbSize;
    std::vector<uint8_t> pixels(sz * sz * 4, 0);
    const float half = sz * 0.5f;
    const float lx = 0.5773f, ly = 0.5773f, lz = 0.5773f; // normalized light
    for (int y = 0; y < sz; ++y) {
        for (int x = 0; x < sz; ++x) {
            const float nx = (x - half) / half;
            const float ny = -(y - half) / half;
            const float r2 = nx*nx + ny*ny;
            uint8_t* p = pixels.data() + (y * sz + x) * 4;
            if (r2 > 1.0f) {
                // 棋盘格背景
                const uint8_t bg = ((x/8 + y/8) % 2 == 0) ? 58 : 42;
                p[0] = p[1] = p[2] = bg; p[3] = 255;
            } else {
                const float nz = sqrtf(1.0f - r2);
                const float ndotl = (std::max)(0.0f, nx*lx + ny*ly + nz*lz);
                const float refl_z = lz - 2.0f * ndotl * nz; // R = L - 2*(N.L)*N, dot with view (0,0,1)
                const float spec_exp = (std::max)(1.0f, (1.0f - roughness) * 64.0f);
                const float spec = powf((std::max)(0.0f, refl_z), spec_exp)
                                   * (1.0f - roughness * 0.8f) * 0.6f;
                const float ambient = 0.10f;
                auto clamp01 = [](float v){ return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
                p[0] = uint8_t(clamp01(r*(ambient + ndotl*0.9f) + er + spec) * 255.0f);
                p[1] = uint8_t(clamp01(g*(ambient + ndotl*0.9f) + eg + spec) * 255.0f);
                p[2] = uint8_t(clamp01(b*(ambient + ndotl*0.9f) + eb + spec) * 255.0f);
                p[3] = 255;
            }
        }
    }
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static unsigned int UploadThumbTexture(const uint8_t* rgba, int w, int h,
                                       const std::filesystem::path& path_key) {
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
    GetThumbnailCache()[path_key.string()] = {tex, w, h};
    return tex;
}

unsigned int LoadThumbnailTexture(const std::filesystem::path& path) {
    const int64_t src_mtime = ThumbGetMtime(path);
    const std::filesystem::path thumb_path = GetThumbFilePath(path);

    if (!thumb_path.empty()) {
        std::ifstream tf(thumb_path, std::ios::binary);
        if (tf.is_open()) {
            uint32_t magic = 0, ver = 0, tw = 0, th = 0;
            int64_t cached_mtime = 0;
            auto r32 = [&](uint32_t& v){ return static_cast<bool>(tf.read(reinterpret_cast<char*>(&v), 4)); };
            auto r64 = [&](int64_t&  v){ return static_cast<bool>(tf.read(reinterpret_cast<char*>(&v), 8)); };
            if (r32(magic) && magic == kThumbMagic &&
                r32(ver)   && ver   == kThumbVersion &&
                r32(tw) && r32(th)  && r64(cached_mtime) &&
                cached_mtime == src_mtime) {
                std::vector<uint8_t> rgba(tw * th * 4);
                if (tf.read(reinterpret_cast<char*>(rgba.data()),
                            static_cast<std::streamsize>(rgba.size()))) {
                    return UploadThumbTexture(rgba.data(), int(tw), int(th), path);
                }
            }
        }
    }

    // .dmat → Phong 球体预览（解析 JSON base_color / emissive / roughness）
    {
        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".dmat") {
            float cr = 0.8f, cg = 0.8f, cb = 0.8f;
            float er = 0.0f, eg = 0.0f, eb = 0.0f;
            float rough = 0.5f;
            std::ifstream f(path);
            if (f.is_open()) {
                std::string js((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
                rapidjson::Document doc;
                if (!doc.Parse(js.c_str()).HasParseError() && doc.IsArray() && doc.Size() > 0) {
                    // 选饱和度最高的材质作为预览色
                    int best_idx = 0;
                    float best_sat = -1.0f;
                    for (rapidjson::SizeType mi = 0; mi < doc.Size(); ++mi) {
                        const auto& m = doc[mi];
                        if (m.HasMember("base_color_factor") && m["base_color_factor"].IsArray()) {
                            const auto bc = m["base_color_factor"].GetArray();
                            if (bc.Size() >= 3) {
                                float r = bc[0].GetFloat(), g = bc[1].GetFloat(), b = bc[2].GetFloat();
                                float mx = std::max({r,g,b}), mn = std::min({r,g,b});
                                float sat = mx - mn;
                                if (sat > best_sat) { best_sat = sat; best_idx = static_cast<int>(mi); }
                            }
                        }
                    }
                    const auto& mat = doc[best_idx];
                    if (mat.HasMember("base_color_factor") && mat["base_color_factor"].IsArray()) {
                        const auto bc = mat["base_color_factor"].GetArray();
                        if (bc.Size() >= 3) { cr = bc[0].GetFloat(); cg = bc[1].GetFloat(); cb = bc[2].GetFloat(); }
                    }
                    if (mat.HasMember("emissive_factor") && mat["emissive_factor"].IsArray()) {
                        const auto em = mat["emissive_factor"].GetArray();
                        if (em.Size() >= 3) { er = em[0].GetFloat(); eg = em[1].GetFloat(); eb = em[2].GetFloat(); }
                    }
                    if (mat.HasMember("roughness_factor") && mat["roughness_factor"].IsNumber())
                        rough = mat["roughness_factor"].GetFloat();
                }
            }
            unsigned int tex = GenerateSpherePreviewTexture(cr, cg, cb, er, eg, eb, rough);
            GetThumbnailCache()[path.string()] = {tex, kThumbSize, kThumbSize};
            return tex;
        }
        if (ext == ".dmesh") {
            unsigned int tex = GenerateSpherePreviewTexture(0.58f, 0.60f, 0.65f, 0.0f, 0.0f, 0.0f, 0.72f);
            GetThumbnailCache()[path.string()] = {tex, kThumbSize, kThumbSize};
            return tex;
        }
    }

    int w, h, channels;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) return 0;

    const int dw = (std::min)(w, kThumbSize);
    const int dh = (std::min)(h, kThumbSize);
    std::vector<uint8_t> thumb(dw * dh * 4);
    BoxResize(data, w, h, thumb.data(), dw, dh);
    stbi_image_free(data);

    if (!thumb_path.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(thumb_path.parent_path(), ec);
        std::ofstream tf(thumb_path, std::ios::binary);
        if (tf.is_open()) {
            auto w32 = [&](uint32_t v){ tf.write(reinterpret_cast<const char*>(&v), 4); };
            auto w64 = [&](int64_t  v){ tf.write(reinterpret_cast<const char*>(&v), 8); };
            w32(kThumbMagic); w32(kThumbVersion);
            w32(uint32_t(dw)); w32(uint32_t(dh));
            w64(src_mtime);
            tf.write(reinterpret_cast<const char*>(thumb.data()),
                     static_cast<std::streamsize>(thumb.size()));
        }
    }

    return UploadThumbTexture(thumb.data(), dw, dh, path);
}

} // namespace

namespace dse::editor {

void DrawProjectPanel() {
    static char s_search_filter[128] = "";
    static bool s_grid_view = false;
    static float s_grid_size = 80.0f;
    static std::filesystem::path s_rename_target;
    static char s_rename_buf[128] = "";

    ImGui::Begin("Project");

    const std::filesystem::path base_data_path = GetProjectBaseDataPath();
    std::filesystem::path& current_path = GetCurrentProjectPanelPath();

    // Toolbar: Search + View toggles (List | Grid) + optional size slider
    const float right_w = s_grid_view ? 130.0f : 82.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - right_w);
    ImGui::InputTextWithHint("##project_search", MDI_ICON_MAGNIFY " Search...", s_search_filter, sizeof(s_search_filter));
    ImGui::SameLine();

    // List view toggle button
    if (!s_grid_view) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    if (ImGui::Button(MDI_ICON_VIEW_LIST "##list_view", ImVec2(26, 0))) s_grid_view = false;
    if (!s_grid_view) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("List View");
    ImGui::SameLine(0, 2);

    // Grid view toggle button
    if (s_grid_view) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    if (ImGui::Button(MDI_ICON_VIEW_GRID "##grid_view", ImVec2(26, 0))) s_grid_view = true;
    if (s_grid_view) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Grid View");

    // Grid size slider (only in grid view)
    if (s_grid_view) {
        ImGui::SameLine(0, 6);
        ImGui::SetNextItemWidth(48);
        ImGui::SliderFloat("##grid_sz", &s_grid_size, 48.0f, 128.0f, "");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thumbnail size: %.0f", s_grid_size);
    }

    ImGui::SameLine(0, 6);
    if (ImGui::Button(MDI_ICON_COG, ImVec2(24, 0))) {
        AssetDatabase::Get().Refresh();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh Asset Database (%zu assets)", AssetDatabase::Get().Count());
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
            const float cell_size = s_grid_size;
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
                bool is_image = !entry.is_directory() &&
                    (IsImageExtension(ext) || ext == ".dmat" || ext == ".dmesh");

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
                    // Background
                    ImU32 bg_color = entry.is_directory() ? IM_COL32(50, 75, 115, 220) : IM_COL32(55, 55, 65, 220);
                    auto* dl = ImGui::GetWindowDrawList();
                    dl->AddRectFilled(p, ImVec2(p.x + thumb_w, p.y + thumb_h), bg_color, 6.0f);

                    // Pick icon for this type
                    const char* type_icon2 = MDI_ICON_FILE_OUTLINE;
                    if (entry.is_directory())                        type_icon2 = MDI_ICON_PACKAGE_VARIANT;
                    else if (ext == ".lua")                          type_icon2 = MDI_ICON_SCRIPT_TEXT_OUTLINE;
                    else if (ext == ".dmesh")                        type_icon2 = MDI_ICON_SPHERE;
                    else if (ext == ".dscene")                       type_icon2 = MDI_ICON_IMAGE_MULTIPLE;
                    else if (ext == ".dprefab")                      type_icon2 = MDI_ICON_CUBE_OUTLINE;
                    else if (ext == ".danim")                        type_icon2 = MDI_ICON_ANIMATION;
                    else if (ext == ".dskel")                        type_icon2 = MDI_ICON_HUMAN;
                    else if (ext==".wav"||ext==".ogg"||ext==".mp3")  type_icon2 = MDI_ICON_MUSIC_NOTE;
                    else if (ext == ".dpak")                         type_icon2 = MDI_ICON_PACKAGE_VARIANT;

                    // Draw icon centered in thumbnail (scale up with cell)
                    const float icon_size = (std::max)(14.0f, thumb_h * 0.45f);
                    ImFont* cur_font = ImGui::GetFont();
                    ImVec2 icon_sz = cur_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, type_icon2);
                    ImVec2 icon_pos(p.x + (thumb_w - icon_sz.x) * 0.5f,
                                    p.y + (thumb_h - icon_sz.y) * 0.5f);
                    dl->AddText(cur_font, icon_size, icon_pos, IM_COL32(200, 210, 230, 200), type_icon2);

                    ImGui::Dummy(ImVec2(thumb_w, thumb_h));
                }

                // Truncate long filenames based on cell size
                const int max_chars = (std::max)(5, static_cast<int>(cell_size / 8));
                std::string display_name = static_cast<int>(filename.size()) > max_chars
                    ? filename.substr(0, max_chars - 1) + "..."
                    : filename;
                // Center filename text under thumbnail
                {
                    float tw = ImGui::CalcTextSize(display_name.c_str()).x;
                    float cx = p.x + (thumb_w - tw) * 0.5f;
                    if (cx > ImGui::GetCursorScreenPos().x)
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (thumb_w - tw) * 0.5f);
                }
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

                // Asset DB tooltip
                if (!entry.is_directory() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    const std::string rel = std::filesystem::relative(path, base_data_path).string();
                    const auto* info = AssetDatabase::Get().FindByPath(rel);
                    if (info) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", filename.c_str());
                        ImGui::TextDisabled("GUID: %s", info->guid.c_str());
                        ImGui::TextDisabled("Type: %s", AssetTypeToString(info->type));
                        ImGui::EndTooltip();
                    }
                }

                // Per-item context menu — explicit ID so PushID(filename) scope is used
                if (ImGui::BeginPopupContextItem("##ctx")) {
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
            // List view — two columns: icon+name | type/size
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 2));
            if (ImGui::BeginTable("project_list", 3,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthFixed, 72);
                ImGui::TableSetupColumn("Size",  ImGuiTableColumnFlags_WidthFixed, 56);
                ImGui::TableHeadersRow();

            for (const auto& entry : entries) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();
                ImGui::PushID(filename.c_str());
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                // Inline rename
                if (s_rename_target == path) {
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##rename_project", s_rename_buf, sizeof(s_rename_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        try {
                            std::filesystem::rename(path, path.parent_path() / s_rename_buf);
                        } catch (...) {}
                        s_rename_target.clear();
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        s_rename_target.clear();
                    }
                    ImGui::TableSetColumnIndex(1); ImGui::TableSetColumnIndex(2);
                    ImGui::PopID();
                    continue;
                }

                // Pick icon by type
                const char* type_icon = MDI_ICON_FILE_OUTLINE;
                const char* type_str  = "File";
                if (entry.is_directory()) {
                    type_icon = MDI_ICON_PACKAGE_VARIANT;
                    type_str  = "Folder";
                } else {
                    std::string ext2 = path.extension().string();
                    for (auto& c : ext2) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (ext2 == ".lua")                      { type_icon = MDI_ICON_SCRIPT_TEXT_OUTLINE;  type_str = "Script"; }
                    else if (ext2 == ".dmesh")               { type_icon = MDI_ICON_SPHERE;               type_str = "Mesh"; }
                    else if (ext2 == ".dscene")              { type_icon = MDI_ICON_IMAGE_MULTIPLE;       type_str = "Scene"; }
                    else if (ext2 == ".dprefab")             { type_icon = MDI_ICON_CUBE_OUTLINE;         type_str = "Prefab"; }
                    else if (ext2 == ".danim")               { type_icon = MDI_ICON_ANIMATION;            type_str = "Anim"; }
                    else if (ext2 == ".dskel")               { type_icon = MDI_ICON_HUMAN;                type_str = "Skeleton"; }
                    else if (IsImageExtension(ext2))         { type_icon = MDI_ICON_IMAGE;                type_str = "Texture"; }
                    else if (ext2==".wav"||ext2==".ogg"||ext2==".mp3") { type_icon = MDI_ICON_MUSIC_NOTE; type_str = "Audio"; }
                    else if (ext2 == ".dpak")                { type_icon = MDI_ICON_PACKAGE_VARIANT;      type_str = "Pak"; }
                }

                std::string label = std::string(type_icon) + "  " + filename;
                bool selected = false;
                if (entry.is_directory()) {
                    if (ImGui::Selectable(label.c_str(), &selected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (ImGui::IsMouseDoubleClicked(0))
                            current_path /= path.filename();
                    }
                } else {
                    ImGui::Selectable(label.c_str(), &selected,
                        ImGuiSelectableFlags_SpanAllColumns);

                    if (ImGui::BeginDragDropSource()) {
                        const std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                        ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                        ImGui::Text("%s", filename.c_str());
                        ImGui::EndDragDropSource();
                    }

                    // Asset DB tooltip
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                        const std::string rel = std::filesystem::relative(path, base_data_path).string();
                        const auto* info = AssetDatabase::Get().FindByPath(rel);
                        if (info) {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", filename.c_str());
                            ImGui::TextDisabled("GUID: %s", info->guid.c_str());
                            ImGui::TextDisabled("Type: %s", AssetTypeToString(info->type));
                            ImGui::EndTooltip();
                        }
                    }
                }

                // Type column
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("%s", type_str);

                // Size column
                ImGui::TableSetColumnIndex(2);
                if (!entry.is_directory()) {
                    std::error_code fec;
                    auto fsz = std::filesystem::file_size(path, fec);
                    if (!fec) {
                        if (fsz < 1024) ImGui::TextDisabled("%zu B", fsz);
                        else if (fsz < 1024*1024) ImGui::TextDisabled("%.0f KB", fsz/1024.0);
                        else ImGui::TextDisabled("%.1f MB", fsz/(1024.0*1024.0));
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
                ImGui::PopID();
            } // for entries
            ImGui::EndTable();
            } // if BeginTable
            ImGui::PopStyleVar();
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

// ─── Animation panel clip cache ─────────────────────────────────────────────
namespace {
struct AnimTrackData {
    std::string name;
    std::vector<float> key_times;
};
struct AnimClipCache {
    float duration = 2.0f;
    std::vector<AnimTrackData> tracks;
};
static std::unordered_map<std::string, AnimClipCache> s_anim_clip_cache;

const AnimClipCache* GetOrLoadAnimClipCache(const std::string& anim_path, AssetManager* am) {
    if (!am || anim_path.empty()) return nullptr;
    auto it = s_anim_clip_cache.find(anim_path);
    if (it != s_anim_clip_cache.end()) return &it->second;

    auto danim = am->LoadDanim(anim_path);
    if (!danim || danim->GetData().empty()) return nullptr;

    const uint8_t* data = danim->GetData().data();
    const size_t data_size = danim->GetData().size();
    if (data_size < sizeof(dse::asset::compiler::AnimHeader)) return nullptr;
    const auto* header = reinterpret_cast<const dse::asset::compiler::AnimHeader*>(data);
    if (header->magic[0]!='D'||header->magic[1]!='S'||header->magic[2]!='E'||header->magic[3]!='A') return nullptr;
    if (header->duration < 0.0f) return nullptr;

    AnimClipCache cache;
    cache.duration = header->duration > 0.0f ? header->duration : 2.0f;

    const auto* channels = reinterpret_cast<const dse::asset::compiler::AnimChannelDesc*>(
        data + sizeof(dse::asset::compiler::AnimHeader));

    // 读取 v2 channel 名称表
    std::vector<std::string> channel_names;
    if (header->version >= 2 && header->channel_count > 0) {
        const uint8_t* nt = data + sizeof(dse::asset::compiler::AnimHeader)
                          + header->channel_count * sizeof(dse::asset::compiler::AnimChannelDesc);
        if (nt + 4 <= data + data_size) {
            uint32_t nt_total = 0; std::memcpy(&nt_total, nt, 4);
            const uint8_t* np = nt + 4;
            const uint8_t* ne = nt + nt_total;
            if (ne > data + data_size) ne = data + data_size;
            channel_names.reserve(header->channel_count);
            for (uint32_t ci = 0; ci < header->channel_count && np + 2 <= ne; ++ci) {
                uint16_t nl = static_cast<uint16_t>(np[0]|(np[1]<<8)); np += 2;
                if (np + nl > ne) break;
                channel_names.emplace_back(reinterpret_cast<const char*>(np), nl);
                np += nl;
            }
        }
    }

    cache.tracks.reserve(header->channel_count);
    for (uint32_t i = 0; i < header->channel_count; ++i) {
        const auto& ch = channels[i];
        AnimTrackData t;
        t.name = (i < channel_names.size() && !channel_names[i].empty())
                   ? channel_names[i]
                   : ("CH_" + std::to_string(i));
        uint32_t kcount = std::max({ch.position_key_count, ch.rotation_key_count, ch.scale_key_count});
        if (kcount > 0 && ch.time_offset + kcount * sizeof(float) <= data_size) {
            t.key_times.resize(kcount);
            std::memcpy(t.key_times.data(), data + ch.time_offset, kcount * sizeof(float));
        }
        cache.tracks.push_back(std::move(t));
    }
    return &s_anim_clip_cache.emplace(anim_path, std::move(cache)).first->second;
}
} // namespace

void DrawAnimationPanel(EditorContext& ctx) {
    auto& registry = ctx.registry;
    auto selected_entity = ctx.selected_entity;
    ImGui::Begin("Animation");

    bool has_animator = (selected_entity != entt::null &&
                         registry.valid(selected_entity) &&
                         registry.all_of<dse::Animator3DComponent>(selected_entity));
    if (!has_animator) {
        ImGui::TextDisabled("Select an entity with Animator3DComponent to view its timeline.");
        ImGui::End();
        return;
    }

    auto& animator = registry.get<dse::Animator3DComponent>(selected_entity);

    static bool s_playing = false;
    static float s_timeline_zoom = 1.0f;
    static float s_timeline_scroll_x = 0.0f;
    static float s_scrub_time = 0.0f;
    static bool s_dragging_playhead = false;
    static std::string s_last_anim_path;
    if (s_last_anim_path != animator.danim_path) {
        s_last_anim_path = animator.danim_path;
        s_scrub_time = 0.0f; s_playing = false; s_timeline_scroll_x = 0.0f;
    }

    // 从二进制加载真实关键帧数据
    const AnimClipCache* clip = GetOrLoadAnimClipCache(animator.danim_path, ctx.engine.asset_manager());
    const float clip_duration = clip ? clip->duration : 2.0f;
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

    // 真实关键帧轨道（每个 channel 一行）
    const float track_height = 18.0f;
    const float label_width = 110.0f;
    const float diamond_size = 4.5f;
    constexpr ImU32 kTrackColors[] = {
        IM_COL32(255, 200, 50, 255), IM_COL32(80, 200, 120, 255),
        IM_COL32(80, 160, 255, 255), IM_COL32(255, 120, 80, 255),
        IM_COL32(200, 80, 255, 255), IM_COL32(80, 230, 220, 255)
    };
    constexpr int kColorCount = 6;

    if (clip && !clip->tracks.empty()) {
        int visible_count = 0;
        for (size_t ti = 0; ti < clip->tracks.size(); ++ti) {
            const auto& track = clip->tracks[ti];
            float row_y = timeline_pos.y + ruler_height + static_cast<float>(visible_count) * (track_height + 2.0f) + track_height * 0.5f;
            if (row_y > timeline_end.y - track_height) break;

            ImU32 col = kTrackColors[ti % kColorCount];

            // 行背景
            ImVec2 row_bg0(timeline_pos.x, row_y - track_height * 0.5f);
            ImVec2 row_bg1(timeline_end.x, row_y + track_height * 0.5f);
            draw_list->AddRectFilled(row_bg0, row_bg1, IM_COL32(40, 40, 50, 200));

            // 轨道标签（截断显示）
            std::string display_name = track.name;
            if (display_name.size() > 14) display_name = display_name.substr(0, 12) + "..";
            draw_list->AddText(ImVec2(timeline_pos.x + 2.0f, row_y - 7.0f),
                               IM_COL32(180, 180, 180, 255), display_name.c_str());

            // 关键帧菱形
            for (float kt : track.key_times) {
                float x = timeline_pos.x + label_width + (kt - visible_start) * pixels_per_second;
                if (x < timeline_pos.x + label_width || x > timeline_end.x) continue;
                ImVec2 c(x, row_y);
                draw_list->AddQuadFilled(
                    ImVec2(c.x, c.y - diamond_size), ImVec2(c.x + diamond_size, c.y),
                    ImVec2(c.x, c.y + diamond_size), ImVec2(c.x - diamond_size, c.y), col);
            }
            ++visible_count;
        }
        if (clip->tracks.size() > static_cast<size_t>(visible_count)) {
            ImGui::SetCursorScreenPos(ImVec2(timeline_pos.x, timeline_pos.y + timeline_height - 16.0f));
            ImGui::TextDisabled("(+%d channels hidden — zoom out or resize)", static_cast<int>(clip->tracks.size()) - visible_count);
        }
    } else {
        // 无 clip 数据时显示占位单轨道
        float track_y = timeline_pos.y + ruler_height + 20.0f;
        draw_list->AddText(ImVec2(timeline_pos.x + 4, track_y - 6),
                           IM_COL32(100, 100, 110, 255), "(no animation data)");
    }

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

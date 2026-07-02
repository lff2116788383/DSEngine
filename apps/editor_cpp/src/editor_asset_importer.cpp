#include "editor_asset_importer.h"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <limits>

#include "imgui.h"
#include "editor_file_dialog.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/compiler/importer.h"
#include "engine/runtime/engine_app.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace {

// ─── State ─────────────────────────────────────────────────────────────────
bool s_open = false;

enum class ImportType { Mesh3D, Texture, Audio };
const char* kImportTypeNames[] = { "3D Model (.gltf/.glb/.fbx/.obj/.blend/.dae)", "Texture (.png/.jpg/.hdr/.tga)", "Audio (.wav/.ogg/.mp3)" };

// 压缩格式下拉项；与 AssetBuilder --texture 的 --format 取值一一对应。
const char* kCompressFormatNames[] = {
    "BC1 (RGB, 4bpp)", "BC3 (RGBA, 8bpp)", "BC1 sRGB (color)", "BC3 sRGB (color+alpha)",
    "BC4 (single channel)", "BC5 (normal map)",
    "BC7 (high quality RGBA, 8bpp)", "BC7 sRGB (high quality color)"
};
const char* kCompressFormatArgs[] = { "bc1", "bc3", "bc1srgb", "bc3srgb", "bc4", "bc5", "bc7", "bc7srgb" };

// ─── Preview Info (#9) ─────────────────────────────────────────────────────
struct PreviewInfo {
    bool valid = false;
    uint32_t vertex_count = 0;
    uint32_t face_count = 0;
    uint32_t submesh_count = 0;
    uint32_t material_count = 0;
    uint32_t animation_count = 0;
    uint32_t bone_count = 0;
    glm::vec3 bbox_min{0.0f};
    glm::vec3 bbox_max{0.0f};
    std::string error;
};

PreviewInfo QuickParseSourceFile(const std::string& path) {
    PreviewInfo info;
    if (path.empty()) return info;

    fs::path p(path);
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    dse::asset::compiler::RawSceneData scene;
    bool loaded = false;

    if (ext == ".gltf" || ext == ".glb") {
        dse::asset::compiler::GltfImporter importer;
        loaded = importer.Import(path, scene);
    }
#ifdef DSE_HAS_ASSIMP
    else if (ext == ".fbx" || ext == ".obj" || ext == ".blend" || ext == ".dae"
             || ext == ".3ds" || ext == ".stl" || ext == ".ply") {
        dse::asset::compiler::FbxImporter importer;
        loaded = importer.Import(path, scene);
    }
#endif
    else {
        info.error = "Unsupported format: " + ext;
        return info;
    }

    if (!loaded) {
        info.error = "Failed to parse file";
        return info;
    }

    info.valid = true;
    info.submesh_count = static_cast<uint32_t>(scene.meshes.size());
    info.material_count = static_cast<uint32_t>(scene.materials.size());
    info.animation_count = static_cast<uint32_t>(scene.animations.size());
    info.bone_count = static_cast<uint32_t>(scene.skeleton.size());

    constexpr float kFloatMax = 3.402823466e+38f;
    glm::vec3 bmin(kFloatMax);
    glm::vec3 bmax(-kFloatMax);
    for (const auto& mesh : scene.meshes) {
        info.vertex_count += static_cast<uint32_t>(mesh.positions.size());
        info.face_count += static_cast<uint32_t>(mesh.indices.size() / 3);
        for (const auto& pos : mesh.positions) {
            bmin.x = (pos.x < bmin.x) ? pos.x : bmin.x;
            bmin.y = (pos.y < bmin.y) ? pos.y : bmin.y;
            bmin.z = (pos.z < bmin.z) ? pos.z : bmin.z;
            bmax.x = (pos.x > bmax.x) ? pos.x : bmax.x;
            bmax.y = (pos.y > bmax.y) ? pos.y : bmax.y;
            bmax.z = (pos.z > bmax.z) ? pos.z : bmax.z;
        }
    }
    if (info.vertex_count > 0) {
        info.bbox_min = bmin;
        info.bbox_max = bmax;
    }
    return info;
}

// ─── Import History (#10) ──────────────────────────────────────────────────
struct ImportHistoryEntry {
    std::string source_path;
    std::vector<std::string> output_files;
    std::string timestamp;
    bool undone = false;
};

std::vector<ImportHistoryEntry> s_import_history;

void RecordImport(const std::string& source, const std::vector<std::string>& outputs) {
    ImportHistoryEntry entry;
    entry.source_path = source;
    entry.output_files = outputs;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
    entry.timestamp = buf;
    s_import_history.push_back(entry);
}

bool UndoImport(ImportHistoryEntry& entry) {
    bool any_deleted = false;
    for (const auto& f : entry.output_files) {
        std::error_code ec;
        if (fs::exists(f, ec)) {
            fs::remove(f, ec);
            if (!ec) any_deleted = true;
        }
    }
    entry.undone = true;
    return any_deleted;
}

struct ImportState {
    ImportType type = ImportType::Mesh3D;
    char source_path[512] = "";
    char output_dir[512] = "";       // relative to project assets
    bool import_animations = true;
    bool import_skeleton = true;
    bool import_materials = true;
    bool anim_compress = true;        // 量化压缩 (v3 smallest-three + 定点)
    bool anim_reduce = true;          // 关键帧精简 (RDP 误差阈值)
    bool mesh_decimate = false;       // 网格减面
    float mesh_decimate_ratio = 0.5f; // 减面目标比例
    int mesh_lod_levels = 0;          // 自动 LOD 生成级数 (0=不生成)
    bool generate_mipmaps = true;
    bool compress_texture = false;
    int compress_format = 1;        // index into kCompressFormatNames (default BC3)
    bool compress_high_quality = false;
    // Preview
    PreviewInfo preview;
    bool preview_requested = false;
    char last_previewed_path[512] = "";
    // Results
    bool importing = false;
    bool import_done = false;
    bool import_success = false;
    std::string import_message;
    std::vector<std::string> imported_files;
};

ImportState s_state{};

// ─── Platform file dialog ──────────────────────────────────────────────────
std::string OpenFileDialogWithFilter(const char* title, const char* filter) {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return filename;
#endif
    return "";
}

const char* GetFilterForType(ImportType type) {
    switch (type) {
    case ImportType::Mesh3D:
        return "3D Models\0*.gltf;*.glb;*.fbx;*.obj;*.blend;*.dae;*.3ds;*.stl;*.ply\0All Files\0*.*\0";
    case ImportType::Texture:
        return "Images\0*.png;*.jpg;*.jpeg;*.hdr;*.tga;*.bmp\0All Files\0*.*\0";
    case ImportType::Audio:
        return "Audio\0*.wav;*.ogg;*.mp3;*.flac\0All Files\0*.*\0";
    }
    return "All Files\0*.*\0";
}

// ─── Locate AssetBuilder ───────────────────────────────────────────────────
fs::path FindAssetBuilder() {
    // Look next to the editor executable, then in bin/
    fs::path exe_dir = fs::path(".");
#ifdef _WIN32
    char module_path[MAX_PATH];
    if (GetModuleFileNameA(nullptr, module_path, MAX_PATH)) {
        exe_dir = fs::path(module_path).parent_path();
    }
#endif
    fs::path candidate = exe_dir / "AssetBuilder.exe";
    if (fs::exists(candidate)) return candidate;
    candidate = exe_dir / "AssetBuilder";
    if (fs::exists(candidate)) return candidate;
    // fallback: rely on PATH
    return fs::path("AssetBuilder");
}

// 运行 AssetBuilder 命令行；返回 true 表示退出码为 0。out_err 填充失败描述。
bool RunAssetBuilder(const std::string& cmd, std::string& out_err) {
#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');
    BOOL ok = CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        out_err = "Failed to launch AssetBuilder: " + cmd;
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (exit_code != 0) {
        out_err = "AssetBuilder failed (exit code " + std::to_string(exit_code) + ")";
        return false;
    }
    return true;
#else
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        out_err = "AssetBuilder failed (exit code " + std::to_string(ret) + ")";
        return false;
    }
    return true;
#endif
}

// ─── Import Logic ──────────────────────────────────────────────────────────
void DoImportMesh(ImportState& state, const std::string& project_asset_dir) {
    fs::path src(state.source_path);
    fs::path out_dir = fs::path(project_asset_dir) / state.output_dir;
    fs::create_directories(out_dir);

    fs::path asset_builder = FindAssetBuilder();
    std::string cmd = "\"" + asset_builder.string() + "\" "
                    + "\"" + std::string(state.source_path) + "\" "
                    + "--out-dir \"" + out_dir.string() + "\"";
    if (!state.anim_compress) cmd += " --no-anim-compress";
    if (!state.anim_reduce)   cmd += " --no-anim-reduce";
    if (state.mesh_decimate) {
        cmd += " --decimate " + std::to_string(state.mesh_decimate_ratio);
    }
    if (state.mesh_lod_levels > 0) {
        cmd += " --lod-levels " + std::to_string(state.mesh_lod_levels);
    }

#ifdef _WIN32
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::vector<char> cmd_buf(cmd.begin(), cmd.end());
    cmd_buf.push_back('\0');
    BOOL ok = CreateProcessA(nullptr, cmd_buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) {
        state.import_success = false;
        state.import_message = "Failed to launch AssetBuilder: " + cmd;
        return;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        state.import_success = false;
        state.import_message = "AssetBuilder failed (exit code " + std::to_string(exit_code) + ")";
        return;
    }
#else
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        state.import_success = false;
        state.import_message = "AssetBuilder failed (exit code " + std::to_string(ret) + ")";
        return;
    }
#endif

    // Collect produced files
    std::string base_name = src.stem().string();
    const char* extensions[] = { ".dmesh", ".dmat", ".danim", ".dskel" };
    for (auto ext : extensions) {
        fs::path out_file = out_dir / (base_name + ext);
        if (fs::exists(out_file)) {
            state.imported_files.push_back(out_file.string());
        }
    }

    state.import_success = !state.imported_files.empty();
    if (state.import_success) {
        state.import_message = "Successfully imported " + std::to_string(state.imported_files.size()) + " file(s)";
        RecordImport(state.source_path, state.imported_files);
    } else {
        state.import_message = "Import produced no output files. Check AssetBuilder output.";
    }
}

void DoImportTexture(ImportState& state, const std::string& project_asset_dir) {
    fs::path src(state.source_path);
    fs::path out_dir = fs::path(project_asset_dir) / state.output_dir;
    fs::create_directories(out_dir);

    if (state.compress_texture) {
        // 走 AssetBuilder --texture 编码为 .dtex（BCn + 可选 mip 链）。
        fs::path dest = out_dir / (src.stem().string() + ".dtex");
        fs::path asset_builder = FindAssetBuilder();
        int fmt_idx = state.compress_format;
        if (fmt_idx < 0 || fmt_idx >= static_cast<int>(IM_ARRAYSIZE(kCompressFormatArgs))) fmt_idx = 1;
        std::string cmd = "\"" + asset_builder.string() + "\" --texture "
                        + "\"" + std::string(state.source_path) + "\" "
                        + "\"" + dest.string() + "\" "
                        + "--format " + kCompressFormatArgs[fmt_idx];
        if (!state.generate_mipmaps) cmd += " --no-mips";
        if (state.compress_high_quality) cmd += " --hq";

        std::string err;
        if (!RunAssetBuilder(cmd, err) || !fs::exists(dest)) {
            state.import_success = false;
            state.import_message = err.empty() ? "Texture compression produced no output." : err;
            return;
        }
        state.import_success = true;
        state.imported_files.push_back(dest.string());
        state.import_message = "Texture compressed to: " + dest.string();
        RecordImport(state.source_path, state.imported_files);
        return;
    }

    fs::path dest = out_dir / src.filename();
    std::error_code ec;
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        state.import_success = false;
        state.import_message = "Copy failed: " + ec.message();
    } else {
        state.import_success = true;
        state.imported_files.push_back(dest.string());
        state.import_message = "Texture copied to: " + dest.string();
        RecordImport(state.source_path, state.imported_files);
    }
}

void DoImportAudio(ImportState& state, const std::string& project_asset_dir) {
    fs::path src(state.source_path);
    fs::path out_dir = fs::path(project_asset_dir) / state.output_dir;
    fs::create_directories(out_dir);

    fs::path dest = out_dir / src.filename();
    std::error_code ec;
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        state.import_success = false;
        state.import_message = "Copy failed: " + ec.message();
    } else {
        state.import_success = true;
        state.imported_files.push_back(dest.string());
        state.import_message = "Audio file copied to: " + dest.string();
        RecordImport(state.source_path, state.imported_files);
    }
}

} // anonymous namespace

namespace dse::editor {

void OpenAssetImporter() {
    s_open = true;
    s_state = ImportState{};
}

void SetAssetImporterSourcePath(const char* path) {
    std::strncpy(s_state.source_path, path, sizeof(s_state.source_path) - 1);
    s_state.source_path[sizeof(s_state.source_path) - 1] = '\0';
    // Auto-detect type from extension
    fs::path p(path);
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".obj") {
        s_state.type = ImportType::Mesh3D;
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr" || ext == ".tga" || ext == ".bmp") {
        s_state.type = ImportType::Texture;
    } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        s_state.type = ImportType::Audio;
    }
}

void DrawAssetImporterDialog(EditorContext& ctx) {
    if (!s_open) return;

    ImGui::SetNextWindowSize(ImVec2(560, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Asset Importer", &s_open)) {
        ImGui::End();
        return;
    }

    auto& state = s_state;

    // ─── Type selector ─────────────────────────────────────────────────────
    ImGui::Text("Import Type:");
    ImGui::SameLine();
    int type_idx = static_cast<int>(state.type);
    if (ImGui::Combo("##import_type", &type_idx, kImportTypeNames, IM_ARRAYSIZE(kImportTypeNames))) {
        state.type = static_cast<ImportType>(type_idx);
    }

    ImGui::Separator();

    // ─── Source file ────────────────────────────────────────────────────────
    ImGui::Text("Source File:");
    ImGui::InputText("##src_path", state.source_path, sizeof(state.source_path), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        std::string result = OpenFileDialogWithFilter("Select Source File", GetFilterForType(state.type));
        if (!result.empty()) {
            strncpy(state.source_path, result.c_str(), sizeof(state.source_path) - 1);
            state.source_path[sizeof(state.source_path) - 1] = '\0';
            state.preview_requested = true;
        }
    }

    // ─── Preview (#9) ───────────────────────────────────────────────────────
    if (state.type == ImportType::Mesh3D && strlen(state.source_path) > 0) {
        if (state.preview_requested || strcmp(state.source_path, state.last_previewed_path) != 0) {
            state.preview = QuickParseSourceFile(state.source_path);
            strncpy(state.last_previewed_path, state.source_path, sizeof(state.last_previewed_path) - 1);
            state.last_previewed_path[sizeof(state.last_previewed_path) - 1] = '\0';
            state.preview_requested = false;
        }
        if (state.preview.valid) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Preview:");
            ImGui::Columns(2, "##preview_cols", false);
            ImGui::SetColumnWidth(0, 140);
            ImGui::Text("Vertices:"); ImGui::NextColumn(); ImGui::Text("%u", state.preview.vertex_count); ImGui::NextColumn();
            ImGui::Text("Triangles:"); ImGui::NextColumn(); ImGui::Text("%u", state.preview.face_count); ImGui::NextColumn();
            ImGui::Text("Submeshes:"); ImGui::NextColumn(); ImGui::Text("%u", state.preview.submesh_count); ImGui::NextColumn();
            ImGui::Text("Materials:"); ImGui::NextColumn(); ImGui::Text("%u", state.preview.material_count); ImGui::NextColumn();
            if (state.preview.bone_count > 0) {
                ImGui::Text("Bones:"); ImGui::NextColumn(); ImGui::Text("%u", state.preview.bone_count); ImGui::NextColumn();
            }
            if (state.preview.animation_count > 0) {
                ImGui::Text("Animations:"); ImGui::NextColumn(); ImGui::Text("%u", state.preview.animation_count); ImGui::NextColumn();
            }
            if (state.preview.vertex_count > 0) {
                glm::vec3 size = state.preview.bbox_max - state.preview.bbox_min;
                ImGui::Text("Bounding Box:"); ImGui::NextColumn();
                ImGui::Text("%.2f x %.2f x %.2f", size.x, size.y, size.z); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else if (!state.preview.error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Preview: %s", state.preview.error.c_str());
        }
    }

    // ─── Output directory (relative to project assets) ─────────────────────
    ImGui::Text("Output Subdirectory (relative to project assets):");
    ImGui::InputText("##out_dir", state.output_dir, sizeof(state.output_dir));
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("e.g. 'models/knight' or 'textures/environment'");
    }

    ImGui::Separator();

    // ─── Type-specific options ──────────────────────────────────────────────
    if (state.type == ImportType::Mesh3D) {
        ImGui::Text("Mesh Import Options:");
        ImGui::Checkbox("Import Animations (.danim)", &state.import_animations);
        if (state.import_animations) {
            ImGui::Indent();
            ImGui::Checkbox("Compress (quantized .danim v3)", &state.anim_compress);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Rotation: smallest-three 48-bit; Position/Scale: 16-bit fixed-point per-track AABB.");
            }
            ImGui::Checkbox("Keyframe Reduction", &state.anim_reduce);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("RDP error-threshold decimation; static tracks collapse to single keyframe.");
            }
            ImGui::Unindent();
        }
        ImGui::Checkbox("Import Skeleton (.dskel)", &state.import_skeleton);
        ImGui::Checkbox("Import Materials (.dmat)", &state.import_materials);

        ImGui::Separator();
        ImGui::Text("Mesh Decimation:");
        ImGui::Checkbox("Decimate Mesh (QEM)", &state.mesh_decimate);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Quadric Error Metrics mesh simplification; reduces triangle count while preserving shape.");
        }
        if (state.mesh_decimate) {
            ImGui::Indent();
            ImGui::SliderFloat("Target Ratio", &state.mesh_decimate_ratio, 0.05f, 0.95f, "%.0f%%");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Fraction of original triangles to keep (e.g. 0.5 = 50%%).");
            }
            ImGui::Unindent();
        }
        ImGui::Text("LOD Generation:");
        ImGui::SliderInt("LOD Levels", &state.mesh_lod_levels, 0, 5);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Auto-generate N LOD meshes (50%%, 25%%, 12.5%%...). Output: base_lodN.dmesh");
        }
    } else if (state.type == ImportType::Texture) {
        ImGui::Text("Texture Import Options:");
        ImGui::Checkbox("Generate Mipmaps", &state.generate_mipmaps);
        ImGui::Checkbox("Compress (BCn -> .dtex)", &state.compress_texture);
        if (state.compress_texture) {
            ImGui::Indent();
            ImGui::Combo("Format", &state.compress_format, kCompressFormatNames, IM_ARRAYSIZE(kCompressFormatNames));
            ImGui::Checkbox("High Quality (slower)", &state.compress_high_quality);
            ImGui::TextDisabled("Encodes to GPU-ready BCn/ASTC blocks. KTX2 input also accepted.");
            ImGui::Unindent();
        }
    } else if (state.type == ImportType::Audio) {
        ImGui::Text("Audio Import Options:");
        ImGui::TextDisabled("Audio files are copied as-is to the project.");
    }

    ImGui::Separator();

    // ─── Import button ──────────────────────────────────────────────────────
    bool can_import = (strlen(state.source_path) > 0) && !state.importing;
    if (!can_import) ImGui::BeginDisabled();
    if (ImGui::Button("Import", ImVec2(120, 0))) {
        state.importing = true;
        state.import_done = false;
        state.import_success = false;
        state.import_message.clear();
        state.imported_files.clear();

        // Resolve project asset directory
        AssetManager* am = ctx.engine.asset_manager();
        std::string asset_dir = am ? am->GetDataRoot() : "data";
        if (asset_dir.empty()) asset_dir = "data";

        switch (state.type) {
        case ImportType::Mesh3D:  DoImportMesh(state, asset_dir); break;
        case ImportType::Texture: DoImportTexture(state, asset_dir); break;
        case ImportType::Audio:   DoImportAudio(state, asset_dir); break;
        }

        state.importing = false;
        state.import_done = true;
    }
    if (!can_import) ImGui::EndDisabled();

    // ─── Results ────────────────────────────────────────────────────────────
    if (state.import_done) {
        ImGui::Separator();
        if (state.import_success) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "%s", state.import_message.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "%s", state.import_message.c_str());
        }

        if (!state.imported_files.empty()) {
            ImGui::Text("Imported Files:");
            for (auto& f : state.imported_files) {
                ImGui::BulletText("%s", f.c_str());
            }
        }
    }

    // ─── Import History (#10) ───────────────────────────────────────────────
    if (!s_import_history.empty()) {
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Import History")) {
            for (int i = static_cast<int>(s_import_history.size()) - 1; i >= 0; --i) {
                auto& entry = s_import_history[i];
                ImGui::PushID(i);
                if (entry.undone) {
                    ImGui::TextDisabled("[Undone] %s", fs::path(entry.source_path).filename().string().c_str());
                } else {
                    ImGui::Text("%s", entry.timestamp.c_str());
                    ImGui::SameLine();
                    ImGui::Text("%s (%zu files)",
                        fs::path(entry.source_path).filename().string().c_str(),
                        entry.output_files.size());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Undo")) {
                        UndoImport(entry);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Delete generated files:");
                        for (const auto& f : entry.output_files) {
                            ImGui::BulletText("%s", fs::path(f).filename().string().c_str());
                        }
                        ImGui::EndTooltip();
                    }
                }
                ImGui::PopID();
            }
        }
    }

    ImGui::End();
}

} // namespace dse::editor

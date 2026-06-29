#include "editor_asset_importer.h"

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>

#include "imgui.h"
#include "editor_file_dialog.h"
#include "engine/assets/asset_manager.h"
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
const char* kImportTypeNames[] = { "3D Model (.gltf/.glb/.fbx)", "Texture (.png/.jpg/.hdr/.tga)", "Audio (.wav/.ogg/.mp3)" };

// 压缩格式下拉项；与 AssetBuilder --texture 的 --format 取值一一对应。
const char* kCompressFormatNames[] = {
    "BC1 (RGB, 4bpp)", "BC3 (RGBA, 8bpp)", "BC1 sRGB (color)", "BC3 sRGB (color+alpha)",
    "BC4 (single channel)", "BC5 (normal map)"
};
const char* kCompressFormatArgs[] = { "bc1", "bc3", "bc1srgb", "bc3srgb", "bc4", "bc5" };

struct ImportState {
    ImportType type = ImportType::Mesh3D;
    char source_path[512] = "";
    char output_dir[512] = "";       // relative to project assets
    bool import_animations = true;
    bool import_skeleton = true;
    bool import_materials = true;
    bool generate_mipmaps = true;
    bool compress_texture = false;
    int compress_format = 1;        // index into kCompressFormatNames (default BC3)
    bool compress_high_quality = false;
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
        return "3D Models\0*.gltf;*.glb;*.fbx;*.obj\0All Files\0*.*\0";
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
        ImGui::Checkbox("Import Skeleton (.dskel)", &state.import_skeleton);
        ImGui::Checkbox("Import Materials (.dmat)", &state.import_materials);
    } else if (state.type == ImportType::Texture) {
        ImGui::Text("Texture Import Options:");
        ImGui::Checkbox("Generate Mipmaps", &state.generate_mipmaps);
        ImGui::Checkbox("Compress (BCn -> .dtex)", &state.compress_texture);
        if (state.compress_texture) {
            ImGui::Indent();
            ImGui::Combo("Format", &state.compress_format, kCompressFormatNames, IM_ARRAYSIZE(kCompressFormatNames));
            ImGui::Checkbox("High Quality (slower)", &state.compress_high_quality);
            ImGui::TextDisabled("Encodes to GPU-ready BCn blocks (BC7/ASTC not yet supported).");
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

    ImGui::End();
}

} // namespace dse::editor

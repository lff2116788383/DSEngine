/**
 * @file editor_external_editor.cpp
 * @brief Opens script files in the user-configured external editor (default: VS Code)
 */

#include "editor_external_editor.h"
#include "editor_settings.h"

#include <algorithm>
#include <cstdio>
#include <string>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace dse::editor {

bool IsScriptExtension(const std::string& ext) {
    // Script/text file types that should open in external editor
    static const char* script_exts[] = {
        ".lua", ".py", ".dssl", ".glsl", ".hlsl", ".wgsl",
        ".vert", ".frag", ".comp", ".geom", ".tesc", ".tese",
        ".json", ".yaml", ".yml", ".toml", ".xml", ".txt",
        ".md", ".cfg", ".ini", ".csv", ".h", ".hpp", ".cpp", ".c"
    };
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (auto& e : script_exts) {
        if (lower_ext == e) return true;
    }
    return false;
}

bool OpenInExternalEditor(const std::string& file_path, int line) {
    EditorSettings settings = LoadEditorSettings();

    if (settings.external_editor_path.empty()) {
        settings.external_editor_path = "code";
        settings.external_editor_args = "--goto \"{file}:{line}\"";
    }

    // Build argument string by replacing {file} and {line} placeholders
    std::string args = settings.external_editor_args;
    std::string line_str = std::to_string(line > 0 ? line : 1);

    // Replace {file}
    {
        size_t pos = args.find("{file}");
        while (pos != std::string::npos) {
            args.replace(pos, 6, file_path);
            pos = args.find("{file}", pos + file_path.size());
        }
    }
    // Replace {line}
    {
        size_t pos = args.find("{line}");
        while (pos != std::string::npos) {
            args.replace(pos, 6, line_str);
            pos = args.find("{line}", pos + line_str.size());
        }
    }

#if defined(_WIN32)
    // Convert to wide strings for ShellExecuteW
    auto toWide = [](const std::string& s) -> std::wstring {
        if (s.empty()) return {};
        int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (wlen <= 0) return {};
        std::wstring ws(wlen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), wlen);
        return ws;
    };

    std::wstring wide_exe = toWide(settings.external_editor_path);
    std::wstring wide_args = toWide(args);

    HINSTANCE hr = ShellExecuteW(nullptr, L"open", wide_exe.c_str(),
        wide_args.c_str(), nullptr, SW_HIDE);
    if (reinterpret_cast<intptr_t>(hr) > 32) {
        return true;
    }

    // Fallback: open file with system default application
    std::wstring wide_file = toWide(file_path);
    ShellExecuteW(nullptr, L"open", wide_file.c_str(), nullptr, nullptr, SW_SHOW);
    return true;
#else
    // Linux/Mac: use system() as fallback
    std::string cmd = settings.external_editor_path + " " + args + " &";
    return system(cmd.c_str()) == 0;
#endif
}

} // namespace dse::editor

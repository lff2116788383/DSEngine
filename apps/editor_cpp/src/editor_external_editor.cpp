/**
 * @file editor_external_editor.cpp
 * @brief Opens script files in the user-configured external editor (default: VS Code)
 */

#include "editor_external_editor.h"
#include "editor_settings.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/platform/process.h"

namespace dse::editor {

namespace {
// Split a settings argument string into an argv, honoring double-quoted spans so
// entries like  --goto "C:/a b/x.lua:10"  become a single argument.
std::vector<std::string> SplitArgs(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quotes = false;
    bool have = false;
    for (char c : s) {
        if (c == '"') { in_quotes = !in_quotes; have = true; }
        else if ((c == ' ' || c == '\t') && !in_quotes) {
            if (have) { out.push_back(cur); cur.clear(); have = false; }
        } else { cur.push_back(c); have = true; }
    }
    if (have) out.push_back(cur);
    return out;
}
}  // namespace

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

    // Launch the configured editor as a detached GUI process via the shared,
    // shell-free process service (no std::system / ShellExecute string concat).
    platform::ProcessOptions opts;
    opts.executable = settings.external_editor_path;
    opts.args = SplitArgs(args);

    std::string err;
    if (platform::LaunchDetached(opts, &err)) {
        return true;
    }

    // Fallback: open the file with the OS default handler.
    platform::ProcessOptions fallback;
#if defined(_WIN32)
    // "cmd /c start" resolves the default application for the file type.
    fallback.executable = "cmd";
    fallback.args = {"/c", "start", "", file_path};
#elif defined(__APPLE__)
    fallback.executable = "open";
    fallback.args = {file_path};
#else
    fallback.executable = "xdg-open";
    fallback.args = {file_path};
#endif
    return platform::LaunchDetached(fallback, nullptr);
}

} // namespace dse::editor

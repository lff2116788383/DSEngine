#include "editor_autosave_core.h"

#include <filesystem>

namespace dse::editor {

std::string SanitizeSceneName(std::string scene_name) {
    if (scene_name.empty()) return "Untitled";
    for (char& c : scene_name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
    }
    return scene_name;
}

std::string MakeAutoSaveFileName(const std::string& scene_name) {
    return SanitizeSceneName(scene_name) + ".autosave.dscene";
}

bool IsAutoSaveRecoveryFile(const std::string& filename) {
    std::filesystem::path p(filename);
    return p.extension() == ".dscene" &&
           p.stem().string().find(".autosave") != std::string::npos;
}

double ClampAutoSaveInterval(double interval_sec) {
    return interval_sec < 10.0 ? 10.0 : interval_sec;
}

AutoSaveDecision DecideAutoSave(bool in_play_mode,
                               bool enabled,
                               bool dirty,
                               double last_save_time,
                               double now,
                               double interval_sec) {
    if (in_play_mode || !enabled || !dirty) return AutoSaveDecision::Skip;
    if (last_save_time == 0.0) return AutoSaveDecision::InitTimer;
    double interval = ClampAutoSaveInterval(interval_sec);
    if ((now - last_save_time) < interval) return AutoSaveDecision::Skip;
    return AutoSaveDecision::Save;
}

std::string MakeAtomicTempPath(const std::string& final_path) {
    return final_path + ".tmp";
}

bool AtomicWriteFile(const std::string& final_path,
                     const std::function<bool(const std::string&)>& write_to,
                     std::error_code& ec) {
    ec.clear();
    const std::string tmp = MakeAtomicTempPath(final_path);

    // 清掉上次中断留下的陈旧临时文件，避免误当作有效内容。
    std::error_code cleanup_ec;
    std::filesystem::remove(tmp, cleanup_ec);

    bool ok = false;
    try {
        ok = write_to ? write_to(tmp) : false;
    } catch (...) {
        ok = false;
    }
    if (!ok) {
        std::filesystem::remove(tmp, cleanup_ec);
        ec = std::make_error_code(std::errc::io_error);
        return false;
    }

    // rename 覆盖：同卷下为原子替换（MSVC 走 ReplaceIfExists，POSIX 走 rename(2)）。
    std::filesystem::rename(tmp, final_path, ec);
    if (ec) {
        std::filesystem::remove(tmp, cleanup_ec);
        return false;
    }
    return true;
}

}  // namespace dse::editor

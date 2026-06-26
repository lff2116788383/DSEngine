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

}  // namespace dse::editor

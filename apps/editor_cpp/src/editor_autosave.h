#pragma once

#include <string>
#include <entt/entt.hpp>

namespace dse::editor {

/// Manages automatic scene saving and crash recovery.
/// - Periodically writes a .autosave copy when the scene is dirty
/// - On startup, detects leftover autosave files and offers recovery
class AutoSaveManager {
public:
    static AutoSaveManager& Get();

    /// Call once at startup after settings are loaded.
    /// Returns true if a recoverable autosave was found.
    bool CheckRecovery();

    /// Show the ImGui recovery dialog. Call every frame until it returns false.
    /// When accepted, loads the autosave into registry.
    bool DrawRecoveryDialog(entt::registry& registry);

    /// Call every frame (in Edit mode). Checks timer and saves if needed.
    void Tick(entt::registry& registry);

    /// Call after a successful manual save to remove the autosave file.
    void OnManualSave();

    /// Call on normal editor exit to clean up autosave files.
    void OnExit();

    /// Get the last auto-save timestamp as a display string (e.g. "14:30:05")
    const std::string& GetLastSaveTimeStr() const { return last_save_time_str_; }
    bool HasAutoSaved() const { return has_auto_saved_; }

private:
    AutoSaveManager() = default;

    std::string GetAutoSavePath() const;
    std::string GetAutoSaveDir() const;

    bool recovery_pending_ = false;
    std::string recovery_path_;

    double last_save_time_ = 0.0;
    std::string last_save_time_str_;
    bool has_auto_saved_ = false;
};

} // namespace dse::editor

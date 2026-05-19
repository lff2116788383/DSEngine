#pragma once

namespace dse::editor {

void DrawPreferencesPanel(bool* p_open);

/// 从 EditorSettings 初始化 Preferences 面板状态（启动时调用一次）
void InitPreferencesFromSettings();

/// Grid / Snap settings getters (persisted in Preferences panel)
bool GetShowGrid();
float GetSnapTranslate();
float GetSnapRotate();
float GetSnapScale();

} // namespace dse::editor

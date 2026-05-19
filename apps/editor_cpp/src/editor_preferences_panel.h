#pragma once

namespace dse::editor {

void DrawPreferencesPanel(bool* p_open);

/// 从 EditorSettings 初始化 Preferences 面板状态（启动时调用一次）
void InitPreferencesFromSettings();

/// Theme API: 0=Dark, 1=Light
int  GetCurrentThemeIndex();
void ToggleEditorTheme();

/// Grid / Snap settings getters & setters (persisted in Preferences panel)
bool GetShowGrid();
void SetShowGrid(bool v);
float GetGridSize();
int GetGridLines();
float GetSnapTranslate();
float GetSnapRotate();
float GetSnapScale();

} // namespace dse::editor

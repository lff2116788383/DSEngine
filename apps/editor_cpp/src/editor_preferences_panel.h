#pragma once

namespace dse::editor {

void DrawPreferencesPanel(bool* p_open);

/// Grid / Snap settings getters (persisted in Preferences panel)
bool GetShowGrid();
float GetSnapTranslate();
float GetSnapRotate();
float GetSnapScale();

} // namespace dse::editor

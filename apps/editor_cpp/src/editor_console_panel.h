#pragma once

#include <string>

namespace dse::editor {

enum class LogLevel { Info, Warning, Error };

/// Add a log entry to the editor console
void EditorLog(LogLevel level, const std::string& message);

/// Install spdlog sink to capture engine logs into the editor console
void InstallEditorLogSink();

/// Draw the Console panel (replaces placeholder in editor_aux_panels)
void DrawConsolePanelImpl();

} // namespace dse::editor

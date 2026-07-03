#pragma once

#include <string>

namespace dse::editor {

enum class LogLevel { Info, Warning, Error };

/// Add a log entry to the editor console (with optional category tag).
void EditorLog(LogLevel level, const std::string& message);
void EditorLogCat(LogLevel level, const char* category, const std::string& message);

/// Install engine log callback + spdlog sink to capture all logs into the editor console.
void InstallEditorLogSink();

/// Draw the Console panel.
void DrawConsolePanelImpl();

/// Export all current log entries to a file. Returns the path written, or empty on failure.
std::string ExportConsoleLogs(const std::string& directory = "logs");

} // namespace dse::editor

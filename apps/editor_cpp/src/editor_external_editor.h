#pragma once

#include <string>

namespace dse::editor {

/// Open a file in the configured external script editor.
/// @param file_path Absolute path to the file to open.
/// @param line      Line number to jump to (1-based, 0 means don't specify).
/// @return true if the editor was launched successfully.
bool OpenInExternalEditor(const std::string& file_path, int line = 0);

/// Check if a file extension is a script type that should open in external editor.
bool IsScriptExtension(const std::string& ext);

} // namespace dse::editor

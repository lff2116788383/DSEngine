#pragma once

#include <string>

namespace dse::editor {

/// Open a file dialog to select a .json scene file. Returns empty string if cancelled.
std::string OpenSceneFileDialog();

/// Save-as file dialog for .json scene file. Returns empty string if cancelled.
std::string SaveSceneFileDialog();

} // namespace dse::editor

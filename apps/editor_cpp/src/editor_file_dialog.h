#pragma once

#include <string>

namespace dse::editor {

/// Open a file dialog to select a .json scene file. Returns empty string if cancelled.
std::string OpenSceneFileDialog();

/// Save-as file dialog for .json scene file. Returns empty string if cancelled.
std::string SaveSceneFileDialog();

/// Generic open dialog. `filter` is a Win32 filter string like "Blueprint (*.dbp)\0*.dbp\0".
/// `def_ext` is the default extension without dot (e.g. "dbp"). Returns empty if cancelled.
std::string OpenFileDialog(const char* title, const char* filter, const char* def_ext,
                           const char* initial_dir = nullptr);

/// Generic save-as dialog. `default_name` is the initial file name shown.
std::string SaveFileDialog(const char* title, const char* filter, const char* def_ext,
                           const char* default_name, const char* initial_dir = nullptr);

/// Browse for a folder. Returns empty string if cancelled.
std::string BrowseFolderDialog(const char* title = "Select Folder");

} // namespace dse::editor

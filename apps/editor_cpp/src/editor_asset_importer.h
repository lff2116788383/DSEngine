#pragma once

#include "editor_context.h"

namespace dse::editor {

/// Draw the Asset Importer dialog (modal)
void DrawAssetImporterDialog(EditorContext& ctx);

/// Open the importer dialog from the menu
void OpenAssetImporter();

/// Pre-fill the source path (used by drag-and-drop)
void SetAssetImporterSourcePath(const char* path);

} // namespace dse::editor

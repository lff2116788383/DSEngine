#pragma once

#include "editor_context.h"

namespace dse::editor {

/// Draw the Asset Importer dialog (modal)
void DrawAssetImporterDialog(EditorContext& ctx);

/// Open the importer dialog from the menu
void OpenAssetImporter();

} // namespace dse::editor

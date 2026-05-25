#pragma once
#include <string>

namespace dse::editor {

/// Draw the enhanced Asset Browser panel with grid/list view, type icons, search,
/// and drag-to-Inspector support.
void DrawAssetBrowserPanel();

/// Returns a reference to a pending asset open path (e.g. scene).
/// If non-empty after DrawAssetBrowserPanel(), the host loop should open the scene and clear it.
std::string& GetPendingAssetOpenPath();

} // namespace dse::editor

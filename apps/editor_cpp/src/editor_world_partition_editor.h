#pragma once

#include "editor_context.h"
#include <vector>

namespace dse::editor {

/// Draw the World Partition visual cell boundary editing tool
void DrawWorldPartitionEditor(EditorContext& ctx);

// ─── Test accessors ─────────────────────────────────────────────────────
enum class WpOverlayMode { None, LOD, Memory, EntityCount, StreamState, Gameplay, Navigation };

struct WpTestCell {
    int grid_x = 0;
    int grid_z = 0;
};

struct WorldPartitionTestState {
    std::vector<WpTestCell> cells;
    int grid_cols = 8;
    int grid_rows = 8;
    int selected_cell = -1;
    WpOverlayMode overlay_mode = WpOverlayMode::StreamState;
};

WorldPartitionTestState& GetWorldPartitionState();

} // namespace dse::editor

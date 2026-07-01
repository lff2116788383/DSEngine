#pragma once

#include "editor_context.h"
#include <vector>

namespace dse::editor {

/// Draw the Terrain Sculpt Preview panel (real-time brush visualization)
void DrawTerrainSculptPreview(EditorContext& ctx);

// ─── Test accessors ─────────────────────────────────────────────────────
enum class TerrainSculptBrushMode { Raise, Lower, Smooth, Flatten, Paint, Erosion, Noise };

struct TerrainSculptTestBrush {
    TerrainSculptBrushMode mode = TerrainSculptBrushMode::Raise;
    float radius = 5.0f;
    float strength = 0.5f;
    float opacity = 1.0f;
};

struct TerrainSculptTestState {
    TerrainSculptTestBrush brush;
    std::vector<float> heightmap;
    int heightmap_size = 32;
};

TerrainSculptTestState& GetTerrainSculptState();

} // namespace dse::editor

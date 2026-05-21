#pragma once

namespace dse::editor {

enum class SceneViewMode {
    Shaded = 0,
    Wireframe,
    ShadedWireframe,
    Unlit,
    Normals,
    Depth,
    AO,
    Overdraw,
    Count
};

const char* SceneViewModeName(SceneViewMode mode);

SceneViewMode& GetCurrentSceneViewMode();

/// Apply the current view mode as an ImGui overlay on the scene texture.
/// Called after the scene texture is drawn, renders additional overlay passes.
void DrawSceneViewModeOverlay(unsigned int scene_texture,
                               unsigned int depth_texture,
                               unsigned int ssao_texture,
                               unsigned int gbuffer_normal_texture);

/// Draw the view mode combo selector (for use in viewport toolbar).
void DrawSceneViewModeSelector();

} // namespace dse::editor

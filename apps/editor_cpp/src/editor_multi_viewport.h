#pragma once

#include <string>
#include <glm/glm.hpp>

namespace dse::editor {

struct EditorContext;

/// Configuration for additional viewport cameras
struct ViewportCameraConfig {
    std::string name = "Viewport";
    glm::vec3 position{0, 5, -10};
    glm::vec3 target{0, 0, 0};
    float fov = 60.0f;
    bool active = false;
    bool ortho = false;
    float ortho_size = 10.0f;
    int render_mode = 0; // 0=Shaded, 1=Wireframe, 2=Unlit
};

/// Global state for multi-viewport management
struct MultiViewportState {
    bool enabled = false;
    int layout = 0;              // 0=Single, 1=Side-by-Side, 2=Top-Bottom, 3=Quad
    ViewportCameraConfig cameras[4];
    int active_camera = 0;
};

MultiViewportState& GetMultiViewportState();

/// Draw the multi-viewport configuration panel
void DrawMultiViewportConfigPanel();

/// Get the view/projection matrices for a specific viewport camera index
void GetViewportCameraMatrices(int camera_index,
                                float aspect,
                                glm::mat4& out_view,
                                glm::mat4& out_proj);

} // namespace dse::editor

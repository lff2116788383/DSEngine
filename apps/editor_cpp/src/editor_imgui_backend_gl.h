#pragma once
#include "editor_imgui_backend.h"

namespace dse::editor {

/// OpenGL3 + GLFW ImGui backend.
class ImGuiBackendGL final : public ImGuiBackend {
public:
    void Init(GLFWwindow* window) override;
    void NewFrame() override;
    void RenderDrawData(ImDrawData* draw_data) override;
    void Shutdown() override;
};

} // namespace dse::editor

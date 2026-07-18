#pragma once
#include "editor_imgui_backend.h"

namespace dse::editor {

/// OpenGL3 + GLFW ImGui backend.
class ImGuiBackendGL final : public ImGuiBackend {
public:
    bool Init(GLFWwindow* window, dse::render::RhiDevice* device) override;
    void NewFrame() override;
    void PrepareFrame(int width, int height, const float clear_color[4]) override;
    void RenderDrawData(ImDrawData* draw_data) override;
    ImTextureID GetTextureId(unsigned int texture_handle) override;
    bool UsesOpenGLContext() const override { return true; }
    void Shutdown() override;
};

} // namespace dse::editor

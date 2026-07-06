#pragma once

struct GLFWwindow;
struct ImDrawData;

namespace dse::editor {

/// Abstract ImGui rendering backend.
/// Hides platform-specific ImGui_Impl* calls behind a uniform interface.
class ImGuiBackend {
public:
    virtual ~ImGuiBackend() = default;

    /// One-time init. Called after ImGui context creation.
    virtual void Init(GLFWwindow* window) = 0;

    /// Begin a new frame (call before ImGui::NewFrame).
    virtual void NewFrame() = 0;

    /// Render draw data (call after ImGui::Render).
    virtual void RenderDrawData(ImDrawData* draw_data) = 0;

    /// Shutdown and release resources.
    virtual void Shutdown() = 0;
};

} // namespace dse::editor

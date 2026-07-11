#pragma once

#include "imgui.h"
#include <memory>

struct GLFWwindow;
struct ImDrawData;
namespace dse::render { class RhiDevice; }

namespace dse::editor {

std::unique_ptr<class ImGuiBackend> CreateImGuiBackend(dse::render::RhiDevice* device);

/// Abstract ImGui rendering backend.
/// Hides platform-specific ImGui_Impl* calls behind a uniform interface.
class ImGuiBackend {
public:
    virtual ~ImGuiBackend() = default;

    /// One-time init. Called after ImGui context creation.
    virtual bool Init(GLFWwindow* window, dse::render::RhiDevice* device) = 0;

    /// Begin a new frame (call before ImGui::NewFrame).
    virtual void NewFrame() = 0;

    virtual void PrepareFrame(int width, int height, const float clear_color[4]) = 0;

    /// Render draw data (call after ImGui::Render).
    virtual void RenderDrawData(ImDrawData* draw_data) = 0;

    virtual ImTextureID GetTextureId(unsigned int texture_handle) = 0;
    virtual void ReleaseTexture(unsigned int texture_handle) { (void)texture_handle; }
    virtual bool UsesOpenGLContext() const = 0;

    /// Shutdown and release resources.
    virtual void Shutdown() = 0;
};

} // namespace dse::editor

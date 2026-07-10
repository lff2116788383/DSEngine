#pragma once

#include "editor_imgui_backend.h"

#if defined(_WIN32) && defined(DSE_ENABLE_D3D11)

namespace dse::render { class DX11RhiDevice; }

namespace dse::editor {

class ImGuiBackendDX11 final : public ImGuiBackend {
public:
    bool Init(GLFWwindow* window, dse::render::RhiDevice* device) override;
    void NewFrame() override;
    void PrepareFrame(int width, int height, const float clear_color[4]) override;
    void RenderDrawData(ImDrawData* draw_data) override;
    ImTextureID GetTextureId(unsigned int texture_handle) override;
    bool UsesOpenGLContext() const override { return false; }
    void Shutdown() override;

private:
    dse::render::DX11RhiDevice* device_ = nullptr;
};

} // namespace dse::editor

#endif

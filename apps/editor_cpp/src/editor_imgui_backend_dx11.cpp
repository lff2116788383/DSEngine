#include "editor_imgui_backend_dx11.h"

#if defined(_WIN32) && defined(DSE_ENABLE_D3D11)

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_glfw.h"
#include "engine/render/rhi/dx11/dx11_rhi_device.h"

namespace dse::editor {

bool ImGuiBackendDX11::Init(GLFWwindow* window, dse::render::RhiDevice* device) {
    device_ = dynamic_cast<dse::render::DX11RhiDevice*>(device);
    if (!device_) return false;
    auto& context = device_->context();
    return ImGui_ImplGlfw_InitForOther(window, true) &&
           ImGui_ImplDX11_Init(context.device(), context.device_context());
}

void ImGuiBackendDX11::NewFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void ImGuiBackendDX11::PrepareFrame(int width, int height, const float clear_color[4]) {
    auto& context = device_->context();
    ID3D11DeviceContext* dc = context.device_context();
    ID3D11RenderTargetView* rtv = context.backbuffer_rtv();
    if (!dc || !rtv) return;
    dc->OMSetRenderTargets(1, &rtv, nullptr);
    dc->ClearRenderTargetView(rtv, clear_color);
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    dc->RSSetViewports(1, &viewport);
}

void ImGuiBackendDX11::RenderDrawData(ImDrawData* draw_data) {
    ImGui_ImplDX11_RenderDrawData(draw_data);
}

ImTextureID ImGuiBackendDX11::GetTextureId(unsigned int texture_handle) {
    const auto* texture = device_->resource_mgr().GetTexture(texture_handle);
    return texture && texture->srv
        ? reinterpret_cast<ImTextureID>(texture->srv.Get())
        : ImTextureID_Invalid;
}

void ImGuiBackendDX11::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    device_ = nullptr;
}

} // namespace dse::editor

#endif

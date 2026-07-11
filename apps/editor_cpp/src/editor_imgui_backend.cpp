#include "editor_imgui_backend.h"

#include "editor_imgui_backend_gl.h"
#include "engine/render/rhi/rhi_device.h"

#if defined(_WIN32) && defined(DSE_ENABLE_D3D11)
#include "editor_imgui_backend_dx11.h"
#endif
#ifdef DSE_ENABLE_VULKAN
#include "editor_imgui_backend_vulkan.h"
#endif

#include <memory>

namespace dse::editor {

std::unique_ptr<ImGuiBackend> CreateImGuiBackend(dse::render::RhiDevice* device) {
    if (!device) return {};
    switch (device->GetBackend()) {
        case RhiBackend::D3D11:
#if defined(_WIN32) && defined(DSE_ENABLE_D3D11)
            return std::make_unique<ImGuiBackendDX11>();
#else
            return {};
#endif
        case RhiBackend::Vulkan:
#ifdef DSE_ENABLE_VULKAN
            return std::make_unique<ImGuiBackendVulkan>();
#else
            return {};
#endif
        case RhiBackend::OpenGL:
            return std::make_unique<ImGuiBackendGL>();
        default:
            return {};
    }
}

} // namespace dse::editor

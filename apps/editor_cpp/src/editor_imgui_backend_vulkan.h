#pragma once

#include "editor_imgui_backend.h"

#ifdef DSE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <unordered_map>

namespace dse::render { class VulkanRhiDevice; }

namespace dse::editor {

class ImGuiBackendVulkan final : public ImGuiBackend {
public:
    bool Init(GLFWwindow* window, dse::render::RhiDevice* device) override;
    void NewFrame() override;
    void PrepareFrame(int width, int height, const float clear_color[4]) override;
    void RenderDrawData(ImDrawData* draw_data) override;
    ImTextureID GetTextureId(unsigned int texture_handle) override;
    void ReleaseTexture(unsigned int texture_handle) override;
    bool UsesOpenGLContext() const override { return false; }
    void Shutdown() override;

private:
    struct TextureBinding {
        VkImageView image_view = VK_NULL_HANDLE;
        ImTextureID texture_id = ImTextureID_Invalid;
    };

    dse::render::VulkanRhiDevice* device_ = nullptr;
    std::unordered_map<unsigned int, TextureBinding> textures_;
    VkRenderPass render_pass_ = VK_NULL_HANDLE;
    uint32_t image_count_ = 0;
    float clear_color_[4] = {0.05f, 0.05f, 0.05f, 1.0f};
};

} // namespace dse::editor

#endif

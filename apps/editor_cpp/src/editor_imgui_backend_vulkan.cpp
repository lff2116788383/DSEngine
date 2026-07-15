#include "editor_imgui_backend_vulkan.h"

#ifdef DSE_ENABLE_VULKAN

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "engine/render/rhi/vulkan/vulkan_command_buffer.h"
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"

namespace dse::editor {

bool ImGuiBackendVulkan::Init(GLFWwindow* window, dse::render::RhiDevice* device) {
    device_ = dynamic_cast<dse::render::VulkanRhiDevice*>(device);
    if (!device_) return false;

    auto& context = device_->context();
    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_2;
    info.Instance = context.instance();
    info.PhysicalDevice = context.physical_device();
    info.Device = context.device();
    info.QueueFamily = context.queue_families().graphics.value();
    info.Queue = context.graphics_queue();
    info.DescriptorPoolSize = 1024;
    info.MinImageCount = 2;
    info.ImageCount = context.swapchain_image_count();
    info.PipelineCache = context.pipeline_cache();
    info.PipelineInfoMain.RenderPass = context.swapchain_render_pass();
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    render_pass_ = context.swapchain_render_pass();
    image_count_ = context.swapchain_image_count();

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) return false;
    if (!ImGui_ImplVulkan_Init(&info)) {
        ImGui_ImplGlfw_Shutdown();
        return false;
    }
    return true;
}

void ImGuiBackendVulkan::NewFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void ImGuiBackendVulkan::PrepareFrame(int, int, const float clear_color[4]) {
    for (int i = 0; i < 4; ++i) clear_color_[i] = clear_color[i];
    auto& context = device_->context();
    if (context.swapchain_image_count() != image_count_) {
        image_count_ = context.swapchain_image_count();
        ImGui_ImplVulkan_SetMinImageCount(2);
    }
    if (context.swapchain_render_pass() != render_pass_) {
        device_->WaitIdle();
        render_pass_ = context.swapchain_render_pass();
        ImGui_ImplVulkan_PipelineInfo pipeline_info{};
        pipeline_info.RenderPass = render_pass_;
        pipeline_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_CreateMainPipeline(&pipeline_info);
    }
}

void ImGuiBackendVulkan::RenderDrawData(ImDrawData* draw_data) {
    auto command_buffer = device_->CreateCommandBuffer();
    auto vulkan_command_buffer =
        std::dynamic_pointer_cast<dse::render::VulkanCommandBuffer>(command_buffer);
    if (!vulkan_command_buffer) return;

    RenderPassDesc render_pass{};
    render_pass.render_target = {};
    render_pass.clear_color =
        glm::vec4(clear_color_[0], clear_color_[1], clear_color_[2], clear_color_[3]);
    render_pass.clear_color_enabled = true;
    command_buffer->BeginRenderPass(render_pass);
    ImGui_ImplVulkan_RenderDrawData(
        draw_data, vulkan_command_buffer->GetVkCommandBuffer());
    command_buffer->EndRenderPass();
    device_->Submit(command_buffer);
}

ImTextureID ImGuiBackendVulkan::GetTextureId(unsigned int texture_handle) {
    if (texture_handle == 0) return ImTextureID_Invalid;
    const auto* texture = device_->resource_mgr().GetTexture(texture_handle);
    if (!texture || texture->image_view == VK_NULL_HANDLE) return ImTextureID_Invalid;
    if (auto it = textures_.find(texture_handle); it != textures_.end()) {
        if (it->second.image_view == texture->image_view) return it->second.texture_id;
        ImGui_ImplVulkan_RemoveTexture(
            reinterpret_cast<VkDescriptorSet>(it->second.texture_id));
        textures_.erase(it);
    }
    const VkSampler sampler =
        texture->sampler != VK_NULL_HANDLE
            ? texture->sampler
            : device_->resource_mgr().default_sampler();
    const auto descriptor = ImGui_ImplVulkan_AddTexture(
        sampler, texture->image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    const ImTextureID id = reinterpret_cast<ImTextureID>(descriptor);
    textures_[texture_handle] = TextureBinding{texture->image_view, id};
    return id;
}

void ImGuiBackendVulkan::ReleaseTexture(unsigned int texture_handle) {
    auto it = textures_.find(texture_handle);
    if (it == textures_.end()) return;
    ImGui_ImplVulkan_RemoveTexture(
        reinterpret_cast<VkDescriptorSet>(it->second.texture_id));
    textures_.erase(it);
}

void ImGuiBackendVulkan::Shutdown() {
    device_->WaitIdle();
    textures_.clear();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    device_ = nullptr;
}

} // namespace dse::editor

#endif

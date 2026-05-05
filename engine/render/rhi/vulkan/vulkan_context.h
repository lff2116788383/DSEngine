/**
 * @file vulkan_context.h
 * @brief Vulkan 上下文 — 管理 Instance、PhysicalDevice、LogicalDevice 和 Swapchain
 *
 * 职责：
 * 1. VkInstance 创建与校验层配置
 * 2. Physical Device 选择与队列族查询
 * 3. Logical Device 与队列获取
 * 4. Swapchain 创建/重建/销毁
 * 5. 同步原语（信号量/围栏）管理
 */

#ifndef DSE_RENDER_VULKAN_CONTEXT_H
#define DSE_RENDER_VULKAN_CONTEXT_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <optional>

namespace dse {
namespace render {

/// 队列族索引
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    std::optional<uint32_t> transfer;

    bool IsComplete() const {
        return graphics.has_value() && present.has_value();
    }
};

/// Swapchain 支持详情
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

/**
 * @class VulkanContext
 * @brief Vulkan 上下文，持有 Instance/Device/Swapchain 的完整生命周期
 *
 * 使用模式：
 *   VulkanContext ctx;
 *   ctx.Init(window_handle, width, height);
 *   // ... 渲染循环 ...
 *   ctx.WaitIdle();
 *   ctx.Shutdown();
 */
class VulkanContext {
public:
    VulkanContext() = default;
    ~VulkanContext() = default;

    // 禁止拷贝
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    /// 初始化 Vulkan Instance/Device/Swapchain
    /// @param window_handle 平台窗口句柄（Win32 HWND）
    /// @param width 窗口宽度
    /// @param height 窗口高度
    /// @param enable_validation 是否启用校验层
    bool Init(void* window_handle, int width, int height, bool enable_validation = true);

    /// 等待设备空闲
    void WaitIdle();

    /// 关闭并释放所有 Vulkan 资源
    void Shutdown();

    /// 窗口大小变化时重建 Swapchain
    bool RecreateSwapchain(int width, int height);

    // --- 访问器 ---
    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physical_device() const { return physical_device_; }
    VkDevice device() const { return device_; }
    VkSurfaceKHR surface() const { return surface_; }
    VkSwapchainKHR swapchain() const { return swapchain_; }
    VkQueue graphics_queue() const { return graphics_queue_; }
    VkQueue present_queue() const { return present_queue_; }
    VkQueue transfer_queue() const { return transfer_queue_; }

    const QueueFamilyIndices& queue_families() const { return queue_families_; }
    const VkSurfaceFormatKHR& surface_format() const { return surface_format_; }
    VkExtent2D swapchain_extent() const { return swapchain_extent_; }
    const std::vector<VkImage>& swapchain_images() const { return swapchain_images_; }
    const std::vector<VkImageView>& swapchain_image_views() const { return swapchain_image_views_; }
    uint32_t swapchain_image_count() const { return static_cast<uint32_t>(swapchain_images_.size()); }

    /// 获取当前帧的 swapchain Framebuffer（由 AcquireNextImage 填充后可用）
    VkFramebuffer current_swapchain_framebuffer() const {
        return current_image_index_ < swapchain_framebuffers_.size()
            ? swapchain_framebuffers_[current_image_index_] : VK_NULL_HANDLE;
    }

    /// 获取 swapchain 关联的 RenderPass（用于 BeginRenderPass 到屏幕）
    VkRenderPass swapchain_render_pass() const { return swapchain_render_pass_; }

    /// 获取当前帧的 image index（由 AcquireNextImage 填充）
    uint32_t current_image_index() const { return current_image_index_; }

    /// 获取每帧同步用的信号量/围栏
    VkSemaphore image_available_semaphore() const { return image_available_semaphores_[current_frame_]; }
    VkSemaphore render_finished_semaphore() const { return render_finished_semaphores_[current_frame_]; }
    VkFence in_flight_fence() const { return in_flight_fences_[current_frame_]; }
    uint32_t current_frame() const { return current_frame_; }
    bool hdr_enabled() const { return hdr_enabled_; }

    /// 获取下一帧 swapchain image（阻塞直到可用）
    /// @return 是否成功获取（VK_SUCCESS 或 VK_SUBOPTIMAL）
    VkResult AcquireNextImage();

    /// 提交渲染完毕的命令缓冲并 present
    /// @param command_buffers 本帧要提交的命令缓冲
    /// @return Present 结果
    VkResult PresentFrame(const std::vector<VkCommandBuffer>& command_buffers);

    /// 推进到下一帧（更新 frame index 并等待围栏）
    void AdvanceFrame();

    /// 最大同时在飞帧数
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

private:
    // --- Instance ---
    bool CreateInstance(bool enable_validation);
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions(bool enable_validation);
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info);

    // --- Surface ---
    bool CreateSurface(void* window_handle);

    // --- Physical Device ---
    bool PickPhysicalDevice();
    int RateDeviceSuitability(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device);

    // --- Logical Device ---
    bool CreateLogicalDevice();

    // --- Swapchain ---
    bool CreateSwapchain(int width, int height);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height);
    bool CreateImageViews();
    bool CreateSwapchainFramebuffers();
    void CleanupSwapchain();

    // --- 同步 ---
    bool CreateSyncObjects();

    // --- 成员 ---
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkQueue graphics_queue_ = VK_NULL_HANDLE;
    VkQueue present_queue_ = VK_NULL_HANDLE;
    VkQueue transfer_queue_ = VK_NULL_HANDLE;

    QueueFamilyIndices queue_families_;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchain_images_;
    std::vector<VkImageView> swapchain_image_views_;
    std::vector<VkFramebuffer> swapchain_framebuffers_;  ///< 每个 swapchain image 对应的 Framebuffer
    VkRenderPass swapchain_render_pass_ = VK_NULL_HANDLE; ///< swapchain 渲染用 RenderPass
    VkSurfaceFormatKHR surface_format_ = {};
    VkExtent2D swapchain_extent_ = {};

    // 帧同步
    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> in_flight_fences_;
    uint32_t current_frame_ = 0;
    uint32_t current_image_index_ = 0;

    bool enable_validation_ = false;
    bool hdr_enabled_ = false;
    bool initialized_ = false;

    static const std::vector<const char*> kValidationLayers;
    static const std::vector<const char*> kDeviceExtensions;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_CONTEXT_H

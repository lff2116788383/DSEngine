/**
 * @file vulkan_context.cpp
 * @brief VulkanContext 实现 — Instance/Device/Swapchain 的创建与管理
 */

#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/base/debug.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>

#include <filesystem>
#include <fstream>
#include <vector>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan_android.h>
#endif

#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IOS
      #define VK_USE_PLATFORM_IOS_MVK
      #include <vulkan/vulkan_ios.h>
    #else
      #define VK_USE_PLATFORM_MACOS_MVK
      #include <vulkan/vulkan_macos.h>
    #endif
  #endif
#endif

#ifdef DSE_ENABLE_HARMONY_PLATFORM
  #if defined(__OHOS__)
    #define VK_USE_PLATFORM_OHOS_OPENHARMONY
    #include <vulkan/vulkan_ohos.h>
  #endif
#endif

namespace dse {
namespace render {

const std::vector<const char*> VulkanContext::kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> VulkanContext::kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    "VK_KHR_portability_subset",
  #endif
#endif
};

// ============================================================
// 公开接口
// ============================================================

bool VulkanContext::Init(void* window_handle, int width, int height, bool enable_validation) {
    if (initialized_) return true;

    enable_validation_ = enable_validation;

    if (!CreateInstance(enable_validation)) return false;
    if (!CreateSurface(window_handle)) return false;
    if (!PickPhysicalDevice()) return false;
    if (!CreateLogicalDevice()) return false;
    if (!CreatePipelineCache()) return false;
    if (!CreateSwapchain(width, height)) return false;
    if (!CreateSyncObjects()) return false;

    initialized_ = true;
    DEBUG_LOG_INFO("[Vulkan] Context initialized: {}x{} validation={}", width, height, enable_validation);
    return true;
}

void VulkanContext::WaitIdle() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
}

void VulkanContext::Shutdown() {
    if (!initialized_) return;

    vkDeviceWaitIdle(device_);

    SavePipelineCache();

    // 同步对象
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (render_finished_semaphores_[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
        if (image_available_semaphores_[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
        if (in_flight_fences_[i] != VK_NULL_HANDLE)
            vkDestroyFence(device_, in_flight_fences_[i], nullptr);
    }

    CleanupSwapchain();

    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (device_ != VK_NULL_HANDLE) {
        if (pipeline_cache_ != VK_NULL_HANDLE) {
            vkDestroyPipelineCache(device_, pipeline_cache_, nullptr);
            pipeline_cache_ = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (enable_validation_ && debug_messenger_ != VK_NULL_HANDLE) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(instance_, debug_messenger_, nullptr);
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    initialized_ = false;
    DEBUG_LOG_INFO("[Vulkan] Context shutdown complete");
}

bool VulkanContext::RecreateSwapchain(int width, int height) {
    vkDeviceWaitIdle(device_);
    CleanupSwapchain();
    if (!CreateSwapchain(width, height)) return false;
    DEBUG_LOG_INFO("[Vulkan] Swapchain recreated: {}x{}", width, height);
    return true;
}

VkResult VulkanContext::AcquireNextImage() {
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX,
        image_available_semaphores_[current_frame_],
        VK_NULL_HANDLE, &current_image_index_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // 调用方需要重建 swapchain
        return result;
    }

    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);
    return result;
}

VkResult VulkanContext::PresentFrame(const std::vector<VkCommandBuffer>& command_buffers) {
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { image_available_semaphores_[current_frame_] };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.empty() ? nullptr : command_buffers.data();

    VkSemaphore signal_semaphores[] = { render_finished_semaphores_[current_frame_] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    DEBUG_LOG_TRACE("[Vulkan] PresentFrame: vkQueueSubmit frame={} imgIdx={} cmdCount={}",
                   current_frame_, current_image_index_, submit_info.commandBufferCount);

    VkResult result = vkQueueSubmit(graphics_queue_, 1, &submit_info, in_flight_fences_[current_frame_]);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] vkQueueSubmit failed: {}", static_cast<int>(result));
        return result;
    }
    DEBUG_LOG_TRACE("[Vulkan] PresentFrame: vkQueueSubmit OK");

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swapchains[] = { swapchain_ };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &current_image_index_;

    DEBUG_LOG_TRACE("[Vulkan] PresentFrame: vkQueuePresentKHR");
    result = vkQueuePresentKHR(present_queue_, &present_info);
    DEBUG_LOG_TRACE("[Vulkan] PresentFrame: vkQueuePresentKHR result={}", static_cast<int>(result));
    return result;
}

void VulkanContext::AdvanceFrame() {
    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ============================================================
// Instance 创建
// ============================================================

bool VulkanContext::CreateInstance(bool enable_validation) {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "DSEngine App";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "DSEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    auto extensions = GetRequiredExtensions(enable_validation);

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  #endif
#endif

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (enable_validation) {
        if (!CheckValidationLayerSupport()) {
            DEBUG_LOG_WARN("[Vulkan] Validation layers requested but not available, disabling");
            enable_validation_ = false;
        } else {
            create_info.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
            create_info.ppEnabledLayerNames = kValidationLayers.data();
            PopulateDebugMessengerCreateInfo(debug_create_info);
            create_info.pNext = &debug_create_info;
        }
    }

    VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create instance: {}", static_cast<int>(result));
        return false;
    }

    // 创建 debug messenger
    if (enable_validation_) {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        if (func) {
            func(instance_, &debug_create_info, nullptr, &debug_messenger_);
        }
    }

    return true;
}

bool VulkanContext::CheckValidationLayerSupport() {
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* layer_name : kValidationLayers) {
        bool found = false;
        for (const auto& layer_props : available_layers) {
            if (strcmp(layer_name, layer_props.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

std::vector<const char*> VulkanContext::GetRequiredExtensions(bool enable_validation) {
    std::vector<const char*> extensions;
    // Win32 表面扩展
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef _WIN32
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
    extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

#ifdef DSE_ENABLE_APPLE_PLATFORM
  #if defined(__APPLE__)
    #if TARGET_OS_IOS
    extensions.push_back("VK_MVK_ios_surface");
    #else
    extensions.push_back("VK_MVK_macos_surface");
    #endif
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  #endif
#endif

#ifdef DSE_ENABLE_HARMONY_PLATFORM
  #if defined(__OHOS__)
    extensions.push_back("VK_OHOS_surface");
  #endif
#endif

    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT /*severity*/,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/) {
    DEBUG_LOG_WARN("[Vulkan][Validation] {}", callback_data->pMessage);
    return VK_FALSE;
}

void VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info) {
    create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = DebugCallback;
}

// ============================================================
// Surface
// ============================================================

bool VulkanContext::CreateSurface(void* window_handle) {
#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.hwnd = static_cast<HWND>(window_handle);
    create_info.hinstance = GetModuleHandle(nullptr);

    VkResult result = vkCreateWin32SurfaceKHR(instance_, &create_info, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create Win32 surface: {}", static_cast<int>(result));
        return false;
    }
#elif defined(__ANDROID__)
    VkAndroidSurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    create_info.window = static_cast<ANativeWindow*>(window_handle);

    VkResult result = vkCreateAndroidSurfaceKHR(instance_, &create_info, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create Android surface: {}", static_cast<int>(result));
        return false;
    }
#elif defined(DSE_ENABLE_APPLE_PLATFORM) && defined(__APPLE__) && TARGET_OS_IOS
    VkIOSSurfaceCreateInfoMVK create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
    create_info.pView = window_handle; // CAMetalLayer*

    VkResult result = vkCreateIOSSurfaceMVK(instance_, &create_info, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create iOS surface: {}", static_cast<int>(result));
        return false;
    }
#elif defined(DSE_ENABLE_HARMONY_PLATFORM) && defined(__OHOS__)
    VkOHOSSurfaceCreateInfoOpenHarmony create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_OHOS_SURFACE_CREATE_INFO_OPENHARMONY;
    create_info.window = static_cast<OHNativeWindow*>(window_handle);

    VkResult result = vkCreateOHOSSurfaceOpenHarmony(instance_, &create_info, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create OHOS surface: {}", static_cast<int>(result));
        return false;
    }
#else
    DEBUG_LOG_ERROR("[Vulkan] Unsupported platform for surface creation");
    return false;
#endif
    return true;
}

// ============================================================
// Physical Device
// ============================================================

bool VulkanContext::PickPhysicalDevice() {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        DEBUG_LOG_ERROR("[Vulkan] No GPU with Vulkan support found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());

    // 评分排序，选最优
    std::multimap<int, VkPhysicalDevice> candidates;
    for (const auto& device : devices) {
        int score = RateDeviceSuitability(device);
        candidates.insert(std::make_pair(score, device));
    }

    if (candidates.rbegin()->first > 0) {
        physical_device_ = candidates.rbegin()->second;
        queue_families_ = FindQueueFamilies(physical_device_);
    } else {
        DEBUG_LOG_ERROR("[Vulkan] No suitable GPU found");
        return false;
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device_, &props);
    DEBUG_LOG_INFO("[Vulkan] Selected GPU: {}", props.deviceName);
    return true;
}

int VulkanContext::RateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceFeatures(device, &features);

    int score = 0;

    // 独立显卡优先
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    // 最大纹理尺寸影响质量
    score += static_cast<int>(props.limits.maxImageDimension2D);

    // 必须支持几何着色器（用于粒子等）
    if (!features.geometryShader) return 0;

    // 必须有完整的队列族
    QueueFamilyIndices indices = FindQueueFamilies(device);
    if (!indices.IsComplete()) return 0;

    // 必须支持所需的设备扩展
    if (!CheckDeviceExtensionSupport(device)) return 0;

    // 必须有合适的 swapchain 格式
    SwapchainSupportDetails swapchain_support = QuerySwapchainSupport(device);
    if (swapchain_support.formats.empty() || swapchain_support.present_modes.empty()) return 0;

    return score;
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    uint32_t i = 0;
    for (const auto& family : queue_families) {
        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present_support);
        if (present_support) {
            indices.present = i;
        }

        // 专用传输队列（不同于图形队列时优先）
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
            if (!indices.transfer.has_value() ||
                (indices.graphics.has_value() && i != indices.graphics.value())) {
                indices.transfer = i;
            }
        }

        if (indices.IsComplete()) break;
        ++i;
    }

    // fallback: transfer = graphics
    if (!indices.transfer.has_value()) {
        indices.transfer = indices.graphics;
    }

    return indices;
}

bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : available_extensions) {
        required.erase(extension.extensionName);
    }
    return required.empty();
}

SwapchainSupportDetails VulkanContext::QuerySwapchainSupport(VkPhysicalDevice device) {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr);
    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, details.formats.data());
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, nullptr);
    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, details.present_modes.data());
    }

    return details;
}

// ============================================================
// Logical Device
// ============================================================

bool VulkanContext::CreateLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> unique_queue_families = {
        queue_families_.graphics.value(),
        queue_families_.present.value()
    };

    float queue_priority = 1.0f;
    for (uint32_t family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_info);
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.fillModeNonSolid = VK_TRUE; // wireframe
    device_features.wideLines = VK_TRUE;

    // GPU-Driven: drawCount > 1 需要 multiDrawIndirect 特性
    VkPhysicalDeviceFeatures supported_features{};
    vkGetPhysicalDeviceFeatures(physical_device_, &supported_features);
    if (supported_features.multiDrawIndirect) {
        device_features.multiDrawIndirect = VK_TRUE;
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    create_info.ppEnabledExtensionNames = kDeviceExtensions.data();

    VkResult result = vkCreateDevice(physical_device_, &create_info, nullptr, &device_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create logical device: {}", static_cast<int>(result));
        return false;
    }

    vkGetDeviceQueue(device_, queue_families_.graphics.value(), 0, &graphics_queue_);
    vkGetDeviceQueue(device_, queue_families_.present.value(), 0, &present_queue_);

    if (queue_families_.transfer.has_value() && queue_families_.transfer.value() != queue_families_.graphics.value()) {
        vkGetDeviceQueue(device_, queue_families_.transfer.value(), 0, &transfer_queue_);
    } else {
        transfer_queue_ = graphics_queue_;
    }

    return true;
}

// ============================================================
// Swapchain
// ============================================================

bool VulkanContext::CreateSwapchain(int width, int height) {
    SwapchainSupportDetails swapchain_support = QuerySwapchainSupport(physical_device_);

    surface_format_ = ChooseSwapSurfaceFormat(swapchain_support.formats);
    hdr_enabled_ = (surface_format_.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT);
    VkPresentModeKHR present_mode = ChooseSwapPresentMode(swapchain_support.present_modes);
    swapchain_extent_ = ChooseSwapExtent(swapchain_support.capabilities, width, height);

    // image 数量 = min(最大+1, 最大允许) 以支持三缓冲
    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
    if (swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format_.format;
    create_info.imageColorSpace = surface_format_.colorSpace;
    create_info.imageExtent = swapchain_extent_;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT; // transfer_dst 用于截图

    uint32_t queue_family_indices[] = {
        queue_families_.graphics.value(),
        queue_families_.present.value()
    };

    if (queue_families_.graphics.value() != queue_families_.present.value()) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create swapchain: {}", static_cast<int>(result));
        return false;
    }

    // 获取 swapchain images
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
    swapchain_images_.resize(image_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swapchain_images_.data());

    if (!CreateImageViews()) return false;
    if (!CreateSwapchainFramebuffers()) return false;

    DEBUG_LOG_INFO("[Vulkan] Swapchain created: {}x{} format={} colorspace={} hdr={} images={}",
        swapchain_extent_.width, swapchain_extent_.height,
        static_cast<int>(surface_format_.format),
        static_cast<int>(surface_format_.colorSpace),
        hdr_enabled_ ? "yes" : "no", image_count);
    return true;
}

VkSurfaceFormatKHR VulkanContext::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available) {
    // 优先 UNORM SDR（渲染管线在 sRGB 空间工作，与 OpenGL 一致，无自动 gamma 转换）
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_UNORM &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmt;
        }
    }
    // 次选 SRGB（不推荐，会导致双重 gamma）
    for (const auto& fmt : available) {
        if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return fmt;
        }
    }
    return available[0];
}

VkPresentModeKHR VulkanContext::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available) {
    const char* vsync_env = std::getenv("DSE_VSYNC");
    const bool vsync_off = vsync_env && std::string(vsync_env) == "0";

    if (vsync_off) {
        // VSync 关闭: 优先 IMMEDIATE（无帧率上限），其次 MAILBOX，再 FIFO_RELAXED
        for (const auto& mode : available) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                DEBUG_LOG_INFO("[Vulkan] Present mode: IMMEDIATE (VSync off)");
                return mode;
            }
        }
        for (const auto& mode : available) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                DEBUG_LOG_INFO("[Vulkan] Present mode: MAILBOX (VSync off, IMMEDIATE unavailable)");
                return mode;
            }
        }
        for (const auto& mode : available) {
            if (mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) {
                DEBUG_LOG_INFO("[Vulkan] Present mode: FIFO_RELAXED (VSync off fallback)");
                return mode;
            }
        }
    }
    DEBUG_LOG_INFO("[Vulkan] Present mode: FIFO (VSync on)");
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int width, int height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    VkExtent2D actual_extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };
    actual_extent.width = std::clamp(actual_extent.width,
        capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = std::clamp(actual_extent.height,
        capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actual_extent;
}

bool VulkanContext::CreateImageViews() {
    swapchain_image_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = swapchain_images_[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = surface_format_.format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(device_, &create_info, nullptr, &swapchain_image_views_[i]);
        if (result != VK_SUCCESS) {
            DEBUG_LOG_ERROR("[Vulkan] Failed to create image view {}", i);
            return false;
        }
    }
    return true;
}

void VulkanContext::CleanupSwapchain() {
    // 销毁 swapchain framebuffers
    for (auto fb : swapchain_framebuffers_) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(device_, fb, nullptr);
    }
    swapchain_framebuffers_.clear();

    // 销毁 swapchain RenderPass
    if (swapchain_render_pass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, swapchain_render_pass_, nullptr);
        swapchain_render_pass_ = VK_NULL_HANDLE;
    }

    for (auto view : swapchain_image_views_) {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(device_, view, nullptr);
    }
    swapchain_image_views_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

bool VulkanContext::CreateSwapchainFramebuffers() {
    // 1. 创建 swapchain 用的 RenderPass（颜色附件，无深度）
    VkAttachmentDescription color_att{};
    color_att.format = surface_format_.format;
    color_att.samples = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;

    // 入口依赖：确保前一个 pass 的写入（颜色+着色器）对本 pass 可见
    VkSubpassDependency dep_begin{};
    dep_begin.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep_begin.dstSubpass    = 0;
    dep_begin.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep_begin.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                            | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep_begin.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dep_begin.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                            | VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rp_ci{};
    rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_ci.attachmentCount = 1;
    rp_ci.pAttachments = &color_att;
    rp_ci.subpassCount = 1;
    rp_ci.pSubpasses = &subpass;
    rp_ci.dependencyCount = 1;
    rp_ci.pDependencies = &dep_begin;

    if (vkCreateRenderPass(device_, &rp_ci, nullptr, &swapchain_render_pass_) != VK_SUCCESS) {
        DEBUG_LOG_ERROR("[Vulkan] Failed to create swapchain render pass");
        return false;
    }

    // 2. 为每个 swapchain image 创建 Framebuffer
    swapchain_framebuffers_.resize(swapchain_image_views_.size());
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
        VkImageView attachments[] = { swapchain_image_views_[i] };

        VkFramebufferCreateInfo fb_ci{};
        fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_ci.renderPass = swapchain_render_pass_;
        fb_ci.attachmentCount = 1;
        fb_ci.pAttachments = attachments;
        fb_ci.width = swapchain_extent_.width;
        fb_ci.height = swapchain_extent_.height;
        fb_ci.layers = 1;

        if (vkCreateFramebuffer(device_, &fb_ci, nullptr, &swapchain_framebuffers_[i]) != VK_SUCCESS) {
            DEBUG_LOG_ERROR("[Vulkan] Failed to create swapchain framebuffer {}", i);
            return false;
        }
    }

    DEBUG_LOG_INFO("[Vulkan] Created {} swapchain framebuffers", swapchain_framebuffers_.size());
    return true;
}

// ============================================================
// 同步
// ============================================================

bool VulkanContext::CreateSyncObjects() {
    image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphore_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]) != VK_SUCCESS) {
            DEBUG_LOG_ERROR("[Vulkan] Failed to create sync objects for frame {}", i);
            return false;
        }
    }
    return true;
}

// ============================================================
// Pipeline Cache
// ============================================================

/// Pipeline cache 文件头，用于校验驱动兼容性
struct PipelineCacheFileHeader {
    uint32_t magic = 0x44534543;        // "DSEC"
    uint32_t header_size = sizeof(PipelineCacheFileHeader);
    uint32_t vendor_id = 0;
    uint32_t device_id = 0;
    uint8_t  pipeline_cache_uuid[VK_UUID_SIZE] = {};
};

static std::filesystem::path GetPipelineCachePath() {
    // 使用可执行文件同级目录下的 cache 子目录，避免依赖 CWD
    std::error_code ec;
    auto exe_path = std::filesystem::current_path(ec);  // fallback to cwd
    return exe_path / "bin" / "cache" / "vulkan_pipeline_cache.bin";
}

bool VulkanContext::CreatePipelineCache() {
    auto cache_path = GetPipelineCachePath();

    // 获取当前物理设备属性用于校验
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device_, &props);

    // 尝试从磁盘加载已有缓存
    std::ifstream cache_file(cache_path, std::ios::binary | std::ios::ate);
    if (cache_file.is_open()) {
        size_t file_size = static_cast<size_t>(cache_file.tellg());
        if (file_size > sizeof(PipelineCacheFileHeader)) {
            std::vector<uint8_t> file_data(file_size);
            cache_file.seekg(0);
            cache_file.read(reinterpret_cast<char*>(file_data.data()), file_size);
            cache_file.close();

            // 校验文件头：驱动 UUID / vendor / device 必须匹配
            auto* header = reinterpret_cast<const PipelineCacheFileHeader*>(file_data.data());
            bool compatible = (header->magic == 0x44534543) &&
                              (header->vendor_id == props.vendorID) &&
                              (header->device_id == props.deviceID) &&
                              (memcmp(header->pipeline_cache_uuid, props.pipelineCacheUUID, VK_UUID_SIZE) == 0);

            if (compatible) {
                size_t cache_offset = sizeof(PipelineCacheFileHeader);
                size_t cache_size = file_size - cache_offset;
                VkPipelineCacheCreateInfo ci{};
                ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
                ci.initialDataSize = cache_size;
                ci.pInitialData = file_data.data() + cache_offset;
                if (vkCreatePipelineCache(device_, &ci, nullptr, &pipeline_cache_) == VK_SUCCESS) {
                    DEBUG_LOG_INFO("[Vulkan] Pipeline cache loaded: {} bytes", cache_size);
                    return true;
                }
            } else {
                DEBUG_LOG_INFO("[Vulkan] Pipeline cache invalidated (driver/device mismatch), rebuilding");
            }
        }
    }

    // 无缓存或加载失败 — 创建空缓存
    VkPipelineCacheCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(device_, &ci, nullptr, &pipeline_cache_) != VK_SUCCESS) {
        DEBUG_LOG_WARN("[Vulkan] Failed to create pipeline cache");
        pipeline_cache_ = VK_NULL_HANDLE;
        return false;
    }
    DEBUG_LOG_INFO("[Vulkan] Pipeline cache created (empty)");
    return true;
}

void VulkanContext::SavePipelineCache() {
    if (pipeline_cache_ == VK_NULL_HANDLE || device_ == VK_NULL_HANDLE) return;

    size_t cache_size = 0;
    if (vkGetPipelineCacheData(device_, pipeline_cache_, &cache_size, nullptr) != VK_SUCCESS || cache_size == 0)
        return;

    std::vector<uint8_t> cache_data(cache_size);
    if (vkGetPipelineCacheData(device_, pipeline_cache_, &cache_size, cache_data.data()) != VK_SUCCESS)
        return;

    // 获取当前设备属性写入文件头
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device_, &props);

    PipelineCacheFileHeader header{};
    header.vendor_id = props.vendorID;
    header.device_id = props.deviceID;
    memcpy(header.pipeline_cache_uuid, props.pipelineCacheUUID, VK_UUID_SIZE);

    auto cache_path = GetPipelineCachePath();
    std::error_code ec;
    std::filesystem::create_directories(cache_path.parent_path(), ec);
    std::ofstream out_file(cache_path, std::ios::binary);
    if (out_file.is_open()) {
        out_file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out_file.write(reinterpret_cast<const char*>(cache_data.data()), cache_size);
        DEBUG_LOG_INFO("[Vulkan] Pipeline cache saved: {} bytes (+ {} header)", cache_size, sizeof(header));
    } else {
        DEBUG_LOG_WARN("[Vulkan] Failed to save pipeline cache to: {}", cache_path.string());
    }
}

} // namespace render
} // namespace dse


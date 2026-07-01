#ifndef DSE_VULKAN_RHI_DEVICE_INTERNAL_H
#define DSE_VULKAN_RHI_DEVICE_INTERNAL_H

#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#include <unordered_map>
#include <vector>

namespace dse {
namespace render {

struct VulkanRhiDevice::HiZImpl {
    struct HiZTextureInfo {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView full_view = VK_NULL_HANDLE;
        std::vector<VkImageView> mip_views;
        int width = 0;
        int height = 0;
        int mip_count = 0;
        unsigned int texture_handle = 0;
    };
    std::unordered_map<unsigned int, HiZTextureInfo> textures;
    unsigned int next_handle = 450000;
};

} // namespace render
} // namespace dse

#endif

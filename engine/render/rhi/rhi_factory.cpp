/**
 * @file rhi_factory.cpp
 * @brief RHI 设备工厂实现
 */

#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/base/debug.h"

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#endif

#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_rhi_device.h"
#endif

#include <cstdlib>
#include <algorithm>

namespace dse {
namespace render {

std::string RhiBackendToString(RhiBackend backend) {
    switch (backend) {
        case RhiBackend::OpenGL: return "OpenGL";
        case RhiBackend::Vulkan: return "Vulkan";
        case RhiBackend::D3D11:  return "D3D11";
        default: return "Unknown";
    }
}

RhiBackend ResolveRhiBackendFromEnv() {
    if (const char* env = std::getenv("DSE_RHI_BACKEND")) {
        std::string value(env);
        // 转小写比较
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        if (value == "vulkan" || value == "vk") {
            return RhiBackend::Vulkan;
        }
        if (value == "opengl" || value == "gl") {
            return RhiBackend::OpenGL;
        }
        if (value == "d3d11" || value == "dx11") {
            return RhiBackend::D3D11;
        }
        DEBUG_LOG_WARN("DSE_RHI_BACKEND 环境值 '{}' 无法识别，回退到 OpenGL", env);
    }
    return RhiBackend::Default;
}

std::unique_ptr<RhiDevice> CreateRhiDevice(RhiBackend backend) {
    switch (backend) {
#ifdef DSE_ENABLE_VULKAN
        case RhiBackend::Vulkan: {
            DEBUG_LOG_INFO("RHI Factory: 创建 Vulkan 后端");
            auto device = std::make_unique<dse::render::VulkanRhiDevice>();
            return device;
        }
#endif
#ifdef DSE_ENABLE_D3D11
        case RhiBackend::D3D11: {
            DEBUG_LOG_INFO("RHI Factory: 创建 D3D11 后端");
            auto device = std::make_unique<dse::render::DX11RhiDevice>();
            return device;
        }
#endif
        case RhiBackend::OpenGL:
            DEBUG_LOG_INFO("RHI Factory: 创建 OpenGL 后端");
            return std::make_unique<OpenGLRhiDevice>();

        default:
#ifdef DSE_ENABLE_VULKAN
            DEBUG_LOG_WARN("RHI Factory: 未知后端类型 {}，回退到 Vulkan",
                           static_cast<unsigned int>(backend));
            return std::make_unique<dse::render::VulkanRhiDevice>();
#else
            DEBUG_LOG_WARN("RHI Factory: 未知后端类型 {}，回退到 OpenGL",
                           static_cast<unsigned int>(backend));
            return std::make_unique<OpenGLRhiDevice>();
#endif
    }
}

} // namespace render
} // namespace dse

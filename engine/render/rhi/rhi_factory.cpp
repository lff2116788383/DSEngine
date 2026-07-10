/**
 * @file rhi_factory.cpp
 * @brief RHI 设备工厂实现
 */

#include "engine/render/rhi/rhi_factory.h"
#include "engine/render/rhi/opengl/gl_rhi_device.h"
#include "engine/base/debug.h"

#ifdef DSE_ENABLE_VULKAN
#include "engine/render/rhi/vulkan/vulkan_rhi_device.h"
#endif

#ifdef DSE_ENABLE_D3D11
#include "engine/render/rhi/dx11/dx11_rhi_device.h"
#endif

#ifdef DSE_ENABLE_WEBGPU
#include "engine/render/rhi/webgpu/webgpu_rhi_device.h"
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
        case RhiBackend::WebGPU: return "WebGPU";
        case RhiBackend::Invalid: return "Invalid";
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
        if (value == "webgpu" || value == "wgpu") {
            return RhiBackend::WebGPU;
        }
        // 显式设置了无法识别的后端名：明确报错，返回 Invalid，禁止静默回退到 OpenGL。
        DEBUG_LOG_ERROR("DSE_RHI_BACKEND 环境值 '{}' 无法识别（可选：opengl/gl、vulkan/vk、d3d11/dx11、webgpu/wgpu）", env);
        return RhiBackend::Invalid;
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
#ifdef DSE_ENABLE_WEBGPU
        case RhiBackend::WebGPU: {
            DEBUG_LOG_INFO("RHI Factory: 创建 WebGPU 后端");
            auto device = std::make_unique<dse::render::WebGPURhiDevice>();
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

RhiBackend ValidateRhiBackend(RhiBackend requested) {
    switch (requested) {
        case RhiBackend::Vulkan:
#ifdef DSE_ENABLE_VULKAN
            return RhiBackend::Vulkan;
#else
            // 显式请求 Vulkan 但未编译：明确报错，返回 Invalid，禁止静默回退。
            DEBUG_LOG_ERROR("请求的 Vulkan 后端未编译进本版本 (DSE_ENABLE_VULKAN=OFF)");
            return RhiBackend::Invalid;
#endif

        case RhiBackend::D3D11:
#ifdef DSE_ENABLE_D3D11
            return RhiBackend::D3D11;
#else
            DEBUG_LOG_ERROR("请求的 D3D11 后端未编译进本版本 (DSE_ENABLE_D3D11=OFF)");
            return RhiBackend::Invalid;
#endif

        case RhiBackend::WebGPU:
#ifdef DSE_ENABLE_WEBGPU
            return RhiBackend::WebGPU;
#else
            DEBUG_LOG_ERROR("请求的 WebGPU 后端未编译进本版本 (DSE_ENABLE_WEBGPU=OFF)");
            return RhiBackend::Invalid;
#endif

        default:
            return requested;
    }
}

} // namespace render
} // namespace dse

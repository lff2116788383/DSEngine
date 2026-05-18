/**
 * @file rhi_factory.h
 * @brief RHI 设备工厂，根据后端类型创建对应的 RhiDevice 实例
 *
 * 用法：
 * @code
 *   auto device = dse::render::CreateRhiDevice(RhiBackend::OpenGL);
 *   // 或通过环境变量 DSE_RHI_BACKEND=vulkan 指定
 * @endcode
 */

#ifndef DSE_RENDER_RHI_FACTORY_H
#define DSE_RENDER_RHI_FACTORY_H

#include "engine/render/rhi/rhi_types.h"
#include <memory>
#include <string>

namespace dse {
namespace render {

class RhiDevice;

/**
 * @brief 根据指定后端类型创建 RhiDevice 实例
 * @param backend 后端类型（OpenGL / Vulkan）
 * @return 对应后端的 RhiDevice 实例；若后端不可用则回退到 OpenGL
 *
 * Vulkan 后端仅在编译时启用 DSE_ENABLE_VULKAN 且运行时 Vulkan SDK 可用时才可用。
 */
std::unique_ptr<RhiDevice> CreateRhiDevice(RhiBackend backend);

/**
 * @brief 从环境变量 DSE_RHI_BACKEND 解析后端类型
 * @return 解析得到的后端枚举，未设置时返回 RhiBackend::Default
 */
RhiBackend ResolveRhiBackendFromEnv();

/**
 * @brief 将后端枚举转换为字符串（用于日志输出）
 */
std::string RhiBackendToString(RhiBackend backend);

/**
 * @brief 根据编译时后端可用性验证请求的后端，必要时回退
 * @param requested 请求的后端类型
 * @return 实际可用的后端类型
 */
RhiBackend ValidateRhiBackend(RhiBackend requested);

} // namespace render
} // namespace dse

#endif // DSE_RENDER_RHI_FACTORY_H

/**
 * @file module.h
 * @brief 引擎动态模块化接口定义，允许功能模块（如 3D/物理/网络）解耦并按需加载
 */

#pragma once

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

class AssetManager;

namespace dse {
namespace core {

/**
 * @class IModule
 * @brief 所有动态加载模块必须实现的基类接口
 */
class IModule {
public:
    virtual ~IModule() = default;

    /**
     * @brief 获取模块的唯一名称
     */
    virtual const char* GetName() const = 0;

    /**
     * @brief 模块初始化，引擎启动或模块被加载时调用
     */
    virtual bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) = 0;

    /**
     * @brief 模块帧更新 (逻辑阶段)
     */
    virtual void OnUpdate(World& world, float delta_time) = 0;

    /**
     * @brief 模块固定帧更新 (物理/固定逻辑阶段)
     */
    virtual void OnFixedUpdate(World& world, float fixed_delta_time) = 0;

    /**
     * @brief 模块渲染：PreZ (深度预渲染阶段)
     */
    virtual void OnRenderPreZ(World& world, CommandBuffer& cmd_buffer) {}

    /**
     * @brief 模块渲染：Shadow (阴影贴图渲染阶段)
     */
    virtual void OnRenderShadow(World& world, CommandBuffer& cmd_buffer, int cascade_index, const glm::mat4& light_view, const glm::mat4& light_proj) {}

    /**
     * @brief 模块渲染：Scene (主场景渲染阶段)
     */
    virtual void OnRenderScene(World& world, CommandBuffer& cmd_buffer) {}

    /**
     * @brief 模块渲染：UI (独立于场景的 UI 渲染阶段)
     *
     * UI 渲染通常使用独立的正交投影和 RenderTarget，
     * 与 Scene Pass 分离以保证 UI 不被深度测试影响
     */
    virtual void OnRenderUI(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height) {}

    /**
     * @brief 模块关闭，释放资源
     */
    virtual void OnShutdown(World& world) = 0;
};

} // namespace core
} // namespace dse

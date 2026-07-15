/**
 * @file spine_system.h
 * @brief Spine 2D 系统，处理骨骼动画更新及渲染数据的生成
 */

#ifndef DSE_SPINE_SYSTEM_H
#define DSE_SPINE_SYSTEM_H

#include <entt/entt.hpp>
#include <cstdint>
#include <vector>
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/mesh_renderer.h"
#include "engine/render/frame_context.h"
#include "engine/ecs/world.h"
class AssetManager;

namespace dse {
namespace gameplay2d {

/**
 * @class SpineSystem
 * @brief Spine骨骼动画更新系统
 */
class SpineSystem {
public:
    SpineSystem() = default;
    ~SpineSystem();

    /**
     * @brief 每帧更新 Spine 动画状态和骨骼矩阵
     * @param registry ECS 注册表
     * @param dt 增量时间
     */
    void Update(entt::registry& registry, float dt);

    /// Phase 1：主线程（Prepare）从 ECS 计算世界空间顶点，快照为逐 slot 绘制批次；
    /// 渲染线程 Render 仅消费快照，不访问 ECS/骨架。
    void ExtractFrameRenderData(World& world);

    void Render(CommandBuffer& cmd_buffer, const dse::render::FrameContext& frame);

    void Shutdown(entt::registry& registry);
    void SetAssetManager(AssetManager* asset_manager);

    /// 注入 RhiDevice（由所属模块初始化时调用）。spine 2D 渲染经 DrawUnlit2D 通用原语路径需要。
    void SetRhiDevice(RhiDevice* device) { rhi_device_ = device; }

private:
    /// 主线程提取的逐 slot 绘制批次（世界空间顶点已展开）。
    struct SpineDrawBatch {
        std::vector<dse::render::Unlit2DVertex> verts;
        std::vector<uint16_t> indices;
        dse::render::TextureHandle texture_handle;
        unsigned int blend_mode = 0;
    };

    void CleanupComponent(SpineRendererComponent& comp);
    AssetManager* asset_manager_ = nullptr;
    RhiDevice* rhi_device_ = nullptr;
    dse::render::MeshRenderer mesh_renderer_;
    std::vector<SpineDrawBatch> frame_batches_;
};

} // namespace gameplay2d
} // namespace dse

#endif

/**
 * @file hair_system.h
 * @brief TressFX 风格毛发系统 — ECS 驱动的 HairComponent 管理
 *
 * 职责：
 * 1. 管理 HairInstance（GPU 资源创建/销毁）
 * 2. 根据 HairComponent 参数同步 HairInstance 状态
 * 3. 提供 Update / Render 入口供 FramePipeline 调用
 *
 * 依赖方向: modules/ → engine/
 */

#ifndef DSE_GAMEPLAY3D_HAIR_SYSTEM_H
#define DSE_GAMEPLAY3D_HAIR_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/hair/hair_asset.h"
#include "engine/render/hair/hair_instance.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace gameplay3d {

class HairSystem {
public:
    HairSystem() = default;
    ~HairSystem() = default;

    /// 初始化（在 RhiDevice 就绪后调用）
    void Init(RhiDevice* rhi_device);

    /// 关闭并释放所有 GPU 资源
    void Shutdown(::World& world);

    /// 每帧更新：同步 HairComponent → HairInstance，更新 LOD
    /// @param world  ECS 世界
    /// @param camera_pos 当前相机位置
    /// @param dt 帧间隔时间
    void Update(::World& world, const glm::vec3& camera_pos, float dt);

    /// 渲染（将活跃 HairInstance 传递给 CommandBuffer）
    /// @param view       当前相机 view 矩阵
    /// @param projection 当前相机 projection 矩阵（含 clip_correction）
    void Render(::World& world, CommandBuffer& cmd_buffer,
                const glm::mat4& view, const glm::mat4& projection);

    /// 获取所有活跃的 HairInstance（供渲染 pass 使用）
    const std::vector<render::HairInstance>& instances() const { return instances_; }
    std::vector<render::HairInstance>& instances() { return instances_; }

    /// 获取资产缓存中的指定资产
    const render::HairAsset* GetCachedAsset(const std::string& path) const;

private:
    /// 加载或获取缓存的毛发资产
    const render::HairAsset* LoadOrGetAsset(const std::string& path);

    /// 初始化 / 销毁 compute shader 资源
    void InitComputeShaders();
    void ShutdownComputeResources();

    /// GPU compute 模拟一帧
    void SimulateCompute(float dt);

    RhiDevice* rhi_ = nullptr;

    /// 资产缓存 (path → asset)
    std::unordered_map<std::string, render::HairAsset> asset_cache_;

    /// 活跃 HairInstance 列表（包含已回收的空槽位）
    std::vector<render::HairInstance> instances_;

    /// 已回收的空闲槽位索引
    std::vector<int> free_slots_;

    /// Compute shader 句柄
    unsigned int cs_integrate_      = 0;
    unsigned int cs_length_         = 0;
    unsigned int cs_local_shape_    = 0;
    unsigned int cs_update_tangent_ = 0;
    bool gpu_compute_enabled_       = false;

    /// 累计时间（用于风场 phase）
    double accumulated_time_ = 0.0;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_GAMEPLAY3D_HAIR_SYSTEM_H

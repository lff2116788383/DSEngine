/**
 * @file impostor_system.h
 * @brief Impostor LOD 运行时系统 — 管理远景切换 + 帧选择 + 渲染驱动
 *
 * 职责：
 * 1. 每帧扫描带 ImpostorComponent 的实体，根据与相机距离决定是否渲染 impostor
 * 2. 计算相机方向到 atlas 帧映射（八面体/半球映射）
 * 3. 收集全部可见 impostor 实例，按 atlas 纹理分批
 * 4. 实现 ISceneRenderer 接口，在 Opaque 阶段经 ImpostorRenderer 绘制
 */

#ifndef DSE_RENDER_IMPOSTOR_SYSTEM_H
#define DSE_RENDER_IMPOSTOR_SYSTEM_H

#include "engine/render/scene_renderer.h"
#include "engine/render/impostor/impostor_renderer.h"
#include "engine/ecs/components_3d_impostor.h"

#include <unordered_map>
#include <vector>

class World;

namespace dse {
namespace render {

class RhiDevice;

/// Impostor LOD 运行时系统
class ImpostorSystem : public ISceneRenderer {
public:
    ImpostorSystem() = default;
    ~ImpostorSystem() override = default;

    /// 每帧在 BuildRenderQueues 之前调用：扫描 ECS，收集可见 impostor 实例
    void Update(World& world, const glm::vec3& camera_pos, RhiDevice& device);

    /// ISceneRenderer 接口：在 ForwardScenePass Opaque 阶段绘制 impostors
    void RenderOpaque(CommandBuffer& cmd, const RenderScenePassContext& ctx) override;

    /// 设置渲染参数（由 FramePipeline 在帧开始时注入）
    void SetRenderContext(RhiDevice* device,
                          const glm::mat4& view,
                          const glm::mat4& proj,
                          const glm::vec3& camera_pos,
                          const glm::vec3& light_dir,
                          const glm::vec3& ambient_color);

    /// 清理 GPU 资源
    void Shutdown(RhiDevice& device);

    /// 获取 ISceneRenderer 指针（注册到 RenderScene）
    ISceneRenderer* AsSceneRenderer() { return this; }

private:
    /// 计算相机方向到 atlas 帧索引（八面体半球映射）
    static void ComputeFrameIndex(const glm::vec3& view_dir,
                                  ImpostorFrameMode mode,
                                  int frames_x, int frames_y,
                                  int& out_frame_x, int& out_frame_y);

    ImpostorRenderer renderer_;

    // 每帧收集的绘制批次（按 atlas 纹理分组）
    std::vector<ImpostorBatchItem> batches_;

    // 渲染上下文（帧级别）
    RhiDevice* device_ = nullptr;
    glm::mat4 view_{1.0f};
    glm::mat4 proj_{1.0f};
    glm::vec3 camera_pos_{0.0f};
    glm::vec3 light_dir_{0.0f, -1.0f, 0.0f};
    glm::vec3 ambient_color_{0.15f};
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_IMPOSTOR_SYSTEM_H

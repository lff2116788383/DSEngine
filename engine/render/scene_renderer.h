#ifndef DSE_RENDER_SCENE_RENDERER_H
#define DSE_RENDER_SCENE_RENDERER_H

namespace dse {
namespace render {

class CommandBuffer;
struct RenderScenePassContext;

/// 场景渲染阶段，对应内建 Pass 的渲染作用域。
enum class SceneRenderStage {
    PreZ,
    Shadow,
    Opaque,
    Transparent,
};

/**
 * @brief 模块向内建渲染 Pass 贡献几何的强类型接口。
 *
 * 取代 RenderScene 的裸 lambda 回调桶（prez/shadow/opaque/transparent_callbacks）。
 * 实现者（如 Gameplay3D）把各阶段的绘制封装为方法；拥有渲染作用域的内建 Pass
 * （PreZPass / CSMShadowPass / ForwardScenePass / WBOITPass）在其
 * BeginRenderPass/EndRenderPass 内调用对应阶段方法。各方法默认空实现，
 * 实现者只重写自己参与的阶段。
 */
class ISceneRenderer {
public:
    virtual ~ISceneRenderer() = default;

    virtual void RenderPreZ(CommandBuffer& cmd, const RenderScenePassContext& ctx) { (void)cmd; (void)ctx; }
    virtual void RenderShadow(CommandBuffer& cmd, const RenderScenePassContext& ctx) { (void)cmd; (void)ctx; }
    virtual void RenderOpaque(CommandBuffer& cmd, const RenderScenePassContext& ctx) { (void)cmd; (void)ctx; }
    virtual void RenderTransparent(CommandBuffer& cmd, const RenderScenePassContext& ctx) { (void)cmd; (void)ctx; }
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SCENE_RENDERER_H

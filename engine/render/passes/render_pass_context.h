/**
 * @file render_pass_context.h
 * @brief 渲染 Pass 共享上下文，提供对运行时资源的只读访问
 */

#ifndef DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H
#define DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H

#include <functional>
#include <vector>
#include <glm/glm.hpp>

class World;
class AssetManager;
class RhiDevice;
class CommandBuffer;

namespace dse {
namespace core {
class IModule;
} // namespace core
} // namespace dse

namespace dse {
namespace render {

/**
 * @struct RenderPassContext
 * @brief 所有 Pass 共享的运行时上下文（非拥有型指针）
 */
struct RenderPassContext {
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
    RhiDevice* rhi_device = nullptr;
    bool editor_mode = false;

    /// 引擎内置系统的访问（通过 FramePipeline 注入）
    struct PipelineState {
        unsigned int sprite = 0;
        unsigned int mesh = 0;
        unsigned int prez = 0;
        unsigned int shadow = 0;
        unsigned int composite = 0;
    } pipeline_states;

    struct RenderTargets {
        unsigned int main = 0;
        unsigned int scene = 0;
        unsigned int ui = 0;
        unsigned int prez = 0;
        unsigned int shadow[3] = {0, 0, 0};     // CSM_CASCADES
        unsigned int spot_shadow[4] = {0, 0, 0, 0};
        unsigned int point_shadow[4] = {0, 0, 0, 0};
        unsigned int bloom_extract = 0;
        std::vector<unsigned int> bloom_mips;
    } render_targets;

    /// 已加载的动态模块实例列表
    struct ModuleRef {
        dse::core::IModule* instance = nullptr;
    };
    std::vector<ModuleRef> modules;

    /// 编辑器相机覆盖（编辑器模式下 Scene 视图使用编辑器相机替代游戏相机）
    bool use_editor_camera = false;
    glm::mat4 editor_view = glm::mat4(1.0f);
    glm::mat4 editor_projection = glm::mat4(1.0f);

    /// FramePipeline 拥有的子系统回调（避免 Pass 直接依赖 FramePipeline）
    std::function<void(World&, CommandBuffer&)> render_2d_scene;
    std::function<void(World&, CommandBuffer&, int, int)> render_2d_ui;
    std::function<void(World&, CommandBuffer&)> render_meshes;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H

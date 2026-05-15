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

class LightBuffer;
class ClusterGrid;

/**
 * @struct RenderPassContext
 * @brief 所有 Pass 共享的运行时上下文（非拥有型指针）
 */
struct RenderPassContext {
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
    RhiDevice* rhi_device = nullptr;
    LightBuffer* light_buffer = nullptr;
    ClusterGrid* cluster_grid = nullptr;
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
        unsigned int ssao = 0;
        unsigned int ssao_blur = 0;
        unsigned int contact_shadow = 0;
        unsigned int fxaa = 0;
        unsigned int taa = 0;               // TAA resolve 输出 RT
        unsigned int dof = 0;               // DOF 输出 RT
        unsigned int ssr = 0;               // SSR 输出 RT
        unsigned int motion_vector = 0;     // Motion Vector RT (RG16F)
        unsigned int outline = 0;            // Outline / Edge Detection RT
        unsigned int gbuffer = 0;           // GBuffer MRT (3 color + depth)
        unsigned int deferred_lighting = 0; // Deferred lighting output RT
        unsigned int lum_temp = 0;          // 64x64 log luminance
        unsigned int lum_adapted[2] = {0,0}; // 1x1 ping-pong
    } render_targets;

    /// 帧级缓存标志（由各 Pass 写入，后续 Pass 读取，避免重复 ECS 查询）
    bool fxaa_active = false;
    bool taa_active = false;

    /// TAA jitter（每帧由 TAAPass 更新，注入到投影矩阵）
    glm::vec2 taa_jitter = {};

    /// Auto Exposure 帧状态
    int lum_ping_pong_index = 0;          // 当前帧写入哪个 1x1 RT (0 or 1)
    float delta_time = 0.016f;            // 帧间隔（用于 EMA 平滑）
    bool auto_exposure_active = false;    // 本帧 auto exposure 是否启用

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
    std::function<void(World&, CommandBuffer&, int, int, const glm::mat4&)> render_2d_ui;
    std::function<void(World&, CommandBuffer&)> render_meshes;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_RENDER_PASS_CONTEXT_H

/**
 * @file scene_manager.h
 * @brief 场景管理器，管理多个 SubScene 实例，支持异步加载/卸载子场景
 */

#ifndef DSE_SCENE_MANAGER_H
#define DSE_SCENE_MANAGER_H

#include "engine/scene/sub_scene.h"
#include "engine/ecs/uuid_component.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/job_system.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <functional>

class AssetManager;

namespace scene {

/**
 * @enum TransitionMode
 * @brief 场景切换过渡模式
 */
enum class TransitionMode {
    Instant,    ///< 直接切换：立即卸载旧场景、加载新场景
    Additive,   ///< 叠加加载：不卸载旧场景，新场景叠加加载
    Fade,       ///< 淡入淡出：旧场景淡出 → 加载新场景 → 淡入
};

/**
 * @enum TransitionState
 * @brief 场景切换状态
 */
enum class TransitionState {
    Idle,        ///< 无过渡
    FadingOut,   ///< 淡出中
    Loading,     ///< 加载中
    FadingIn,    ///< 淡入中
};

/**
 * @class SceneManager
 * @brief 管理多个 SubScene 实例，提供异步加载/卸载接口
 *
 * - LoadSubSceneAsync() 利用 JobSystem 在工作线程读取文件 IO，主线程 pump 完成后注入 World
 * - 通过 EventBus 发送 kSubSceneLoaded / kSubSceneUnloaded 事件
 * - 通过 ServiceLocator 注册为引擎服务
 * - Update() 在 FramePipeline 每帧调用以 pump 异步完成的子场景
 */
class SceneManager {
public:
    SceneManager();
    ~SceneManager();

    /**
     * @brief 注入引擎依赖
     */
    void SetWorld(World* world);
    void SetAssetManager(AssetManager* asset_manager);
    void SetEventBus(dse::core::EventBus* event_bus);
    void SetJobSystem(dse::core::JobSystem* job_system);

    /**
     * @brief 异步加载子场景（IO 在工作线程，Entity 创建在主线程 pump）
     * @param path 子场景文件路径
     */
    void LoadSubSceneAsync(const std::string& path);

    /**
     * @brief 同步加载子场景
     * @param path 子场景文件路径
     * @return 成功返回 true
     */
    bool LoadSubScene(const std::string& path);

    /**
     * @brief 卸载指定子场景
     * @param path 子场景文件路径
     */
    void UnloadSubScene(const std::string& path);

    /**
     * @brief 卸载所有子场景
     */
    void UnloadAll();

    /**
     * @brief 每帧更新，处理异步加载完成的子场景和场景切换状态机
     * @param dt 帧间隔时间（秒），用于 Fade 过渡计时
     */
    void Update(float dt = 0.0f);

    /**
     * @brief 获取已加载的子场景路径列表
     */
    std::vector<std::string> GetLoadedSubScenes() const;

    /**
     * @brief 查询子场景是否已加载
     */
    bool IsSubSceneLoaded(const std::string& path) const;

    /**
     * @brief 获取已加载子场景总数
     */
    size_t LoadedCount() const;

    /**
     * @brief 获取待 pump 的异步加载数量
     */
    size_t PendingCount() const;

    /**
     * @brief 获取子场景（只读）
     */
    const SubScene* GetSubScene(const std::string& path) const;

    // ========== Phase 3: 场景切换 ==========

    /**
     * @brief 切换到目标场景
     * @param path 目标场景文件路径
     * @param mode 过渡模式
     * @param fade_duration Fade 模式下的过渡时长（秒）
     */
    void TransitionTo(const std::string& path, TransitionMode mode, float fade_duration = 0.5f);

    /**
     * @brief 获取当前场景切换状态
     */
    TransitionState GetTransitionState() const { return transition_state_; }

    /**
     * @brief 获取 Fade 过渡的归一化进度 [0, 1]
     * - FadingOut: 0→1 表示渐暗
     * - FadingIn:  0→1 表示渐亮
     */
    float GetFadeProgress() const { return fade_progress_; }

    /**
     * @brief 获取当前活跃（主）场景路径
     */
    const std::string& GetActiveScenePath() const { return active_scene_path_; }

    // ========== Phase 4: 跨场景 Entity 引用 ==========

    /**
     * @brief 在所有已加载 SubScene 中查找拥有指定 UUID 的 Entity
     * @param uuid 目标 UUID
     * @return 找到返回对应 Entity，否则返回 entt::null
     */
    Entity ResolveReference(uint64_t uuid) const;

private:
    void UpdateTransition(float dt);
    struct PendingLoad {
        std::string path;
        std::string json_data;   ///< 工作线程读取的文件内容
        bool success = false;
    };

    World* world_ = nullptr;
    AssetManager* asset_manager_ = nullptr;
    dse::core::EventBus* event_bus_ = nullptr;
    dse::core::JobSystem* job_system_ = nullptr;

    std::unordered_map<std::string, std::unique_ptr<SubScene>> sub_scenes_;

    mutable std::mutex pending_mutex_;
    std::vector<PendingLoad> pending_loads_;

    // Phase 3: 场景切换状态
    std::string active_scene_path_;
    TransitionState transition_state_ = TransitionState::Idle;
    TransitionMode pending_transition_mode_ = TransitionMode::Instant;
    std::string pending_transition_path_;
    float fade_duration_ = 0.5f;
    float fade_timer_ = 0.0f;
    float fade_progress_ = 0.0f;
    std::string previous_scene_path_;  ///< Instant/Fade 切换时需要卸载的旧场景
};

} // namespace scene

#endif // DSE_SCENE_MANAGER_H

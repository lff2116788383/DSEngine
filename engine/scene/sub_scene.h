/**
 * @file sub_scene.h
 * @brief 子场景（SubScene），支持 Level Streaming —— 共享 World 的 ECS registry 但独立管理自身 Entity 生命周期
 */

#ifndef DSE_SUB_SCENE_H
#define DSE_SUB_SCENE_H

#include "engine/ecs/world.h"
#include <string>
#include <vector>

class AssetManager;

namespace scene {

/**
 * @enum SubSceneState
 * @brief 子场景加载状态
 */
enum class SubSceneState {
    Unloaded,   ///< 未加载
    Loading,    ///< 加载中（异步）
    Loaded,     ///< 已加载
};

/**
 * @class SubScene
 * @brief 子场景，持有独立 Entity 列表，共享 World 的 ECS registry 但独立管理自身 Entity 的生命周期
 *
 * 用法：
 * - Load() 从文件反序列化并创建 Entity 到指定 World
 * - Unload() 销毁此 SubScene 拥有的所有 Entity
 * - 同一 World 可同时持有多个 SubScene
 */
class SubScene {
public:
    SubScene() = default;
    explicit SubScene(const std::string& path);
    ~SubScene();

    // 禁止拷贝，允许移动
    SubScene(const SubScene&) = delete;
    SubScene& operator=(const SubScene&) = delete;
    SubScene(SubScene&& other) noexcept;
    SubScene& operator=(SubScene&& other) noexcept;

    /**
     * @brief 从文件反序列化并将 Entity 创建到指定 World 中
     * @param world 目标世界
     * @param asset_manager 资产管理器（用于后续资源加载）
     * @param path 场景文件路径
     * @return 成功返回 true
     */
    bool Load(World& world, AssetManager& asset_manager, const std::string& path);

    /**
     * @brief 销毁此 SubScene 拥有的所有 Entity
     * @param world 当初加载时使用的世界
     */
    void Unload(World& world);

    /**
     * @brief 获取此 SubScene 的文件路径
     */
    const std::string& GetPath() const { return path_; }

    /**
     * @brief 获取此 SubScene 拥有的 Entity 列表
     */
    const std::vector<Entity>& GetEntities() const { return entities_; }

    /**
     * @brief 获取此 SubScene 拥有的 Entity 数量
     */
    size_t EntityCount() const { return entities_.size(); }

    /**
     * @brief 获取当前加载状态
     */
    SubSceneState GetState() const { return state_; }

    /**
     * @brief 检查是否已加载
     */
    bool IsLoaded() const { return state_ == SubSceneState::Loaded; }

private:
    std::string path_;
    std::vector<Entity> entities_;
    SubSceneState state_ = SubSceneState::Unloaded;
};

} // namespace scene

#endif // DSE_SUB_SCENE_H

/**
 * @file game_application.h
 * @brief C++ 宿主便捷基类，封装引擎生命周期与常用 ECS/资产操作。
 *
 * 用法示例:
 * @code
 * class MyGame : public dse::runtime::GameApplication {
 * protected:
 *     void OnInit() override {
 *         auto sun = CreateDirectionalLight({-0.5f, -1.0f, -0.3f});
 *         auto cam = CreateCamera3D({0, 5, 15});
 *         auto box = CreateMesh({0, 0, 0}, "models/cube.dmesh");
 *     }
 *     void OnUpdate(float dt) override { }
 * };
 * int main() { return MyGame().Run({.window_width=1280, .window_height=720}); }
 * @endcode
 */

#ifndef DSE_GAME_APPLICATION_H
#define DSE_GAME_APPLICATION_H

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "engine/input/input.h"

#include <glm/glm.hpp>
#include <string>

namespace dse::runtime {

class GameApplication {
public:
    virtual ~GameApplication() = default;

    /**
     * @brief 启动引擎并执行完整生命周期。
     * @param config 引擎配置（business_mode 会被强制设为 Cpp）
     * @return 退出码
     */
    int Run(EngineRunConfig config);

protected:
    // ─── 生命周期钩子（子类重写） ───────────────────

    virtual void OnInit() {}
    virtual void OnUpdate(float delta_time) {}
    virtual void OnShutdown() {}

    // ─── 服务访问 ──────────────────────────────────

    World& GetWorld() const { return *world_; }
    AssetManager& GetAssetManager() const { return *asset_manager_; }

    // ─── ECS 基础操作 ──────────────────────────────

    Entity CreateEntity();
    void DestroyEntity(Entity e);

    template<typename T, typename... Args>
    T& Emplace(Entity e, Args&&... args) {
        return world_->registry().emplace_or_replace<T>(e, std::forward<Args>(args)...);
    }

    template<typename T>
    T* Get(Entity e) {
        if (!world_->registry().valid(e)) return nullptr;
        return world_->registry().try_get<T>(e);
    }

    template<typename T>
    bool Has(Entity e) const {
        return world_->registry().valid(e) && world_->registry().all_of<T>(e);
    }

    template<typename T>
    void Remove(Entity e) {
        if (Has<T>(e)) world_->registry().remove<T>(e);
    }

    // ─── 实体工厂 ──────────────────────────────────

    /**
     * @brief 创建带 Transform 的空实体
     */
    Entity CreateEntityAt(const glm::vec3& position,
                          const glm::vec3& scale = glm::vec3(1.0f));

    /**
     * @brief 创建 3D 透视相机
     */
    Entity CreateCamera3D(const glm::vec3& position,
                          float fov = 60.0f,
                          float near_clip = 0.1f,
                          float far_clip = 1000.0f);

    /**
     * @brief 创建平行光
     */
    Entity CreateDirectionalLight(const glm::vec3& direction,
                                  const glm::vec3& color = glm::vec3(1.0f),
                                  float intensity = 1.0f,
                                  bool cast_shadow = true);

    /**
     * @brief 创建点光源
     */
    Entity CreatePointLight(const glm::vec3& position,
                            const glm::vec3& color = glm::vec3(1.0f),
                            float intensity = 1.0f,
                            float radius = 10.0f);

    /**
     * @brief 创建网格实体（自动添加 Transform + MeshRenderer）
     */
    Entity CreateMesh(const glm::vec3& position,
                      const std::string& mesh_path,
                      const glm::vec3& scale = glm::vec3(1.0f));

    // ─── 资产快捷方法 ──────────────────────────────

    unsigned int LoadTexture(const std::string& path);

    // ─── 内部生命周期（protected 以支持测试注入） ───

    void Bootstrap(World& world, AssetManager& asset_manager);
    void Tick(World& world, float delta_time);
    void ShutdownInternal();

private:
    World* world_ = nullptr;
    AssetManager* asset_manager_ = nullptr;
};

} // namespace dse::runtime

#endif

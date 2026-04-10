/**
 * @file scene.h
 * @brief 场景管理系统，处理场景图(Scene Graph)、节点变换和空间划分
 */

#ifndef DSE_SCENE_H
#define DSE_SCENE_H

#include "engine/ecs/world.h"
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Forward declare rapidjson or similar json library if needed, 
// for now we'll use a string representation for interface
namespace scene {

struct PrefabInstantiateOptions {
    bool override_position = false;
    bool override_rotation = false;
    bool override_scale = false;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
};

/**
 * @class Scene
 * @brief 场景类，负责组织和管理世界(World)中的实体，提供序列化和反序列化功能
 */
class Scene {
public:
    Scene(const std::string& name);
    ~Scene();

    const std::string& GetName() const { return name_; }
    /**
     * @brief 获取当前场景激活的世界实例
     * @return World 的引用
     */
    World& GetWorld() { return ActiveWorld(); }
    /**
     * @brief 绑定外部世界实例
     * @param world 外部世界指针
     */
    void BindWorld(World* world);
    /**
     * @brief 解绑外部世界实例，回退到使用内置的 owned_world_
     */
    void UnbindWorld();

    // Real serialization interfaces
    /**
     * @brief 将当前场景序列化到文件
     * @param filepath 目标文件路径
     * @return 成功返回 true，否则返回 false
     */
    bool Serialize(const std::string& filepath);
    /**
     * @brief 从文件反序列化加载场景
     * @param filepath 源文件路径
     * @return 成功返回 true，否则返回 false
     */
    bool Deserialize(const std::string& filepath);

private:
    /**
     * @brief 获取激活的世界（绑定的世界或内置世界）
     * @return World 引用
     */
    World& ActiveWorld();
    const World& ActiveWorld() const;
    std::string name_;
    World owned_world_;
    World* world_ = nullptr;
};

/**
 * @brief 运行场景序列化的往返回归测试
 * @param filepath 测试文件路径
 * @return 测试通过返回 true
 */
bool RunSceneRoundTripRegressionSample(const std::string& filepath);

/**
 * @brief 运行场景反向兼容性回归测试
 * @param filepath 测试文件路径
 * @return 测试通过返回 true
 */
bool RunSceneBackwardCompatibilityRegressionSample(const std::string& filepath);

/**
 * @brief 运行仓库内置最小 3D MVP 场景回归样例
 * @param filepath 已签入的场景文件路径
 * @return 样例通过返回 true
 */
bool RunMinimal3DMvpSceneRegressionSample(const std::string& filepath);

/**
 * @brief 将指定实体保存为预制体
 * @param world 实体所在的世界
 * @param entity 要保存的实体
 * @param filepath 保存的文件路径
 * @return 成功返回 true
 */
bool SaveEntityAsPrefab(World& world, Entity entity, const std::string& filepath);

/**
 * @brief 实例化一个预制体到世界中
 * @param world 目标世界
 * @param filepath 预制体文件路径
 * @return 实例化出的新实体
 */
Entity InstantiatePrefab(World& world, const std::string& filepath);

/**
 * @brief 带选项地实例化预制体（覆盖 Transform 等）
 * @param world 目标世界
 * @param filepath 预制体文件路径
 * @param options 实例化覆盖选项
 * @return 实例化出的新实体
 */
Entity InstantiatePrefab(World& world, const std::string& filepath, const PrefabInstantiateOptions& options);

} // namespace scene

#endif // DSE_SCENE_H

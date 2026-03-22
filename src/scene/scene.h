#ifndef DSE_SCENE_H
#define DSE_SCENE_H

#include "phase1/ecs/world.h"
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

class Scene {
public:
    Scene(const std::string& name);
    ~Scene();

    const std::string& GetName() const { return name_; }
    Phase1World& GetWorld() { return ActiveWorld(); }
    void BindWorld(Phase1World* world);
    void UnbindWorld();

    // Real serialization interfaces
    bool Serialize(const std::string& filepath);
    bool Deserialize(const std::string& filepath);

private:
    Phase1World& ActiveWorld();
    const Phase1World& ActiveWorld() const;
    std::string name_;
    Phase1World owned_world_;
    Phase1World* world_ = nullptr;
};

bool RunSceneRoundTripRegressionSample(const std::string& filepath);
bool RunSceneBackwardCompatibilityRegressionSample(const std::string& filepath);
bool SaveEntityAsPrefab(Phase1World& world, Entity entity, const std::string& filepath);
Entity InstantiatePrefab(Phase1World& world, const std::string& filepath);
Entity InstantiatePrefab(Phase1World& world, const std::string& filepath, const PrefabInstantiateOptions& options);

} // namespace scene

#endif // DSE_SCENE_H

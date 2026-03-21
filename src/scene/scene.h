#ifndef DSE_SCENE_H
#define DSE_SCENE_H

#include "phase1/ecs/world.h"
#include <string>

// Forward declare rapidjson or similar json library if needed, 
// for now we'll use a string representation for interface
namespace scene {

class Scene {
public:
    Scene(const std::string& name);
    ~Scene();

    const std::string& GetName() const { return name_; }
    Phase1World& GetWorld() { return world_; }

    // Real serialization interfaces
    bool Serialize(const std::string& filepath);
    bool Deserialize(const std::string& filepath);

private:
    std::string name_;
    Phase1World world_; // Each scene has its own ECS world
};

} // namespace scene

#endif // DSE_SCENE_H

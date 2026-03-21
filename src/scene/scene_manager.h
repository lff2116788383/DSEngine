#ifndef DSE_SCENE_MANAGER_H
#define DSE_SCENE_MANAGER_H

#include "scene/scene.h"
#include <memory>
#include <unordered_map>
#include <string>

namespace scene {

class SceneManager {
public:
    static SceneManager& Instance();

    std::shared_ptr<Scene> CreateScene(const std::string& name);
    void LoadScene(const std::string& name);
    std::shared_ptr<Scene> GetActiveScene();

private:
    SceneManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes_;
    std::shared_ptr<Scene> active_scene_;
};

} // namespace scene

#endif // DSE_SCENE_MANAGER_H

#include "scene/scene_manager.h"
#include "utils/debug.h"

namespace scene {

SceneManager& SceneManager::Instance() {
    static SceneManager instance;
    return instance;
}

std::shared_ptr<Scene> SceneManager::CreateScene(const std::string& name) {
    auto scene = std::make_shared<Scene>(name);
    scenes_[name] = scene;
    return scene;
}

void SceneManager::LoadScene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it != scenes_.end()) {
        active_scene_ = it->second;
        DEBUG_LOG_INFO("Loaded scene: {}", name);
    } else {
        DEBUG_LOG_ERROR("Scene not found: {}", name);
    }
}

std::shared_ptr<Scene> SceneManager::GetActiveScene() {
    return active_scene_;
}

} // namespace scene

#ifndef DSE_RESOURCE_MANAGER_H
#define DSE_RESOURCE_MANAGER_H

#include <unordered_map>
#include <string>
#include <memory>
#include <typeindex>
#include "resource.h"
#include "utils/debug.h"

class ResourceManager {
public:
    static ResourceManager& Instance() {
        static ResourceManager instance;
        return instance;
    }

    template<typename T>
    std::shared_ptr<T> Load(const std::string& path) {
        static_assert(std::is_base_of<Resource, T>::value, "T must inherit from Resource");
        
        std::string key = path; // In real engine, hash or normalize path
        auto it = resources_.find(key);
        if (it != resources_.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }

        std::shared_ptr<T> resource = std::make_shared<T>();
        if (resource->Load(path)) {
            resource->SetPath(path);
            resources_[key] = resource;
            return resource;
        }

        DEBUG_LOG_ERROR("Failed to load resource: {}", path);
        return nullptr;
    }

    void UnloadUnused() {
        for (auto it = resources_.begin(); it != resources_.end(); ) {
            if (it->second.use_count() <= 1) { // Only held by manager
                it = resources_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    ResourceManager() {}
    std::unordered_map<std::string, std::shared_ptr<Resource>> resources_;
};

#endif // DSE_RESOURCE_MANAGER_H

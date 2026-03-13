#include "prefab.h"
#include "utils/debug.h"
#include "core/serializer.h"
#include <fstream>
#include <sstream>

Prefab::Prefab() {
}

Prefab::~Prefab() {
}

bool Prefab::Load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("Failed to open prefab file: {}", path);
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    data_ = buffer.str();
    return true;
}

GameObject* Prefab::Instantiate() {
    if (data_.empty()) return nullptr;
    
    // Deserialize data_ to create GameObject hierarchy
    GameObject* go = Serializer::Deserialize(data_);
    if (go) {
        // Optionally mark it as a prefab instance in GameObject for override tracking
        // go->SetPrefabInstance(this);
    }
    return go;
}

bool Prefab::Create(GameObject* root, const std::string& path) {
    if (!root) return false;
    
    // Serialize root to string
    std::string data = Serializer::Serialize(root);
    
    // Write to file
    std::ofstream file(path);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("Failed to create prefab file: {}", path);
        return false;
    }
    file << data;
    file.close();
    
    return true;
}

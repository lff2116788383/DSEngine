#include "scene/scene.h"
#include "phase1/ecs/components_2d.h"
#include "utils/debug.h"
#include <fstream>
#include <sstream>

// If RapidJSON is available, we'd include it here. 
// We will mock the JSON logic using plain string building for demonstration of the architecture.

namespace scene {

Scene::Scene(const std::string& name) : name_(name) {
}

Scene::~Scene() {
}

bool Scene::Serialize(const std::string& filepath) {
    DEBUG_LOG_INFO("Serializing scene {} to {}", name_, filepath);
    
    std::stringstream json;
    json << "{\n";
    json << "  \"name\": \"" << name_ << "\",\n";
    json << "  \"entities\": [\n";
    
    auto view = world_.registry().view<TransformComponent>();
    int count = 0;
    for (auto entity : view) {
        if (count > 0) json << ",\n";
        auto& t = view.get<TransformComponent>(entity);
        json << "    {\n";
        json << "      \"id\": " << (uint32_t)entity << ",\n";
        json << "      \"components\": {\n";
        json << "        \"TransformComponent\": {\n";
        json << "          \"position\": [" << t.position.x << ", " << t.position.y << ", " << t.position.z << "]\n";
        json << "        }\n";
        
        if (world_.registry().all_of<SpriteRendererComponent>(entity)) {
            auto& s = world_.registry().get<SpriteRendererComponent>(entity);
            json << "        ,\"SpriteRendererComponent\": {\n";
            json << "          \"color\": [" << s.color.r << ", " << s.color.g << ", " << s.color.b << ", " << s.color.a << "],\n";
            json << "          \"sorting_layer\": " << s.sorting_layer << "\n";
            json << "        }\n";
        }
        json << "      }\n";
        json << "    }";
        count++;
    }
    
    json << "\n  ]\n";
    json << "}\n";

    std::ofstream out(filepath);
    if (out.is_open()) {
        out << json.str();
        out.close();
        return true;
    }
    return false;
}

bool Scene::Deserialize(const std::string& filepath) {
    DEBUG_LOG_INFO("Deserializing scene {} from {}", name_, filepath);
    std::ifstream in(filepath);
    if (!in.is_open()) return false;
    
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string json_str = buffer.str();
    
    // In a real engine, we'd parse json_str with rapidjson or nlohmann::json
    // and recreate entities:
    // Entity e = world_.CreateEntity();
    // world_.registry().emplace<TransformComponent>(e, parsed_pos);
    
    return true;
}

} // namespace scene

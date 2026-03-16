#include "serializer.h"
#include "utils/debug.h"
#include <rttr/registration>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

// Helper to convert variant to JSON string
std::string VariantToJson(rttr::variant& var) {
    rttr::type t = var.get_type();
    
    if (t.is_arithmetic()) {
        if (t == rttr::type::get<bool>()) {
            return var.to_bool() ? "true" : "false";
        }
        return var.to_string();
    }
    else if (t == rttr::type::get<std::string>()) {
        return "\"" + var.to_string() + "\"";
    }
    else if (t.is_enumeration()) {
        return "\"" + var.to_string() + "\"";
    }
    else if (t == rttr::type::get<glm::vec2>()) {
        glm::vec2 v = var.get_value<glm::vec2>();
        std::stringstream ss;
        ss << "[" << v.x << ", " << v.y << "]";
        return ss.str();
    }
    else if (t == rttr::type::get<glm::vec3>()) {
        glm::vec3 v = var.get_value<glm::vec3>();
        std::stringstream ss;
        ss << "[" << v.x << ", " << v.y << ", " << v.z << "]";
        return ss.str();
    }
    else if (t == rttr::type::get<glm::vec4>()) {
        glm::vec4 v = var.get_value<glm::vec4>();
        std::stringstream ss;
        ss << "[" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << "]";
        return ss.str();
    }
    else if (t.is_sequential_container()) {
        auto view = var.create_sequential_view();
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < view.get_size(); ++i) {
            rttr::variant item = view.get_value(i).extract_wrapped_value();
            ss << VariantToJson(item);
            if (i < view.get_size() - 1) ss << ", ";
        }
        ss << "]";
        return ss.str();
    }
    
    return "\"<unknown_type>\"";
}

std::string Serializer::Serialize(GameObject* game_object) {
    if (!game_object) return "{}";
    
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"name\": \"" << game_object->name() << "\",\n";
    ss << "  \"layer\": " << (int)game_object->layer() << ",\n";
    ss << "  \"active\": " << (game_object->active_self() ? "true" : "false") << ",\n";
    
    // Components
    ss << "  \"components\": [\n";
    bool first_comp = true;
    game_object->ForeachComponent([&](Component* component) {
        if (!first_comp) ss << ",\n";
        ss << "    " << SerializeObject(component);
        first_comp = false;
    });
    ss << "\n  ],\n";
    
    // Children
    ss << "  \"children\": [\n";
    bool first_child = true;
    auto& children = game_object->children();
    for (auto node : children) {
        GameObject* child = dynamic_cast<GameObject*>(node);
        if (child) {
            if (!first_child) ss << ",\n";
            // Indent appropriately in real implementation
            ss << Serializer::Serialize(child); 
            first_child = false;
        }
    }
    ss << "\n  ]\n";
    ss << "}";
    
    return ss.str();
}

GameObject* Serializer::Deserialize(const std::string& json_data) {
    // Requires a JSON parser (e.g., rapidjson or nlohmann/json)
    // For now, we return a dummy object to satisfy the interface
    DEBUG_LOG_ERROR("Serializer::Deserialize is not fully implemented (requires JSON parser).");
    return new GameObject("DeserializedObject");
}

std::string Serializer::SerializeObject(rttr::instance obj) {
    std::stringstream ss;
    rttr::type t = obj.get_type();
    // If it's a pointer, dereference it
    if (t.is_pointer()) {
        return SerializeObject(obj.get_wrapped_instance());
    }
    
    ss << "{ \"type\": \"" << t.get_name().to_string() << "\"";
    
    for (auto& prop : t.get_properties()) {
        ss << ", \"" << prop.get_name().to_string() << "\": ";
        rttr::variant val = prop.get_value(obj);
        ss << VariantToJson(val);
    }
    ss << " }";
    return ss.str();
}

void Serializer::DeserializeObject(rttr::instance obj, const std::string& json_data) {
    DEBUG_LOG_ERROR("Serializer::DeserializeObject is not fully implemented.");
}

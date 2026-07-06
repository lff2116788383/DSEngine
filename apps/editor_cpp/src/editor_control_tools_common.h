#pragma once


#include "editor_control_server.h"
#include <rapidjson/document.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse::editor {

inline JsonRpcResponse MakeOk(rapidjson::Document result = {}) {
    JsonRpcResponse resp;
    resp.is_error = false;
    resp.result = std::move(result);
    return resp;
}

inline JsonRpcResponse MakeToolError(int code, const std::string& msg) {
    JsonRpcResponse resp;
    resp.is_error = true;
    resp.error_code = code;
    resp.error_message = msg;
    return resp;
}

inline glm::vec3 ParseVec3(const rapidjson::Value& arr, glm::vec3 def = glm::vec3(0.0f)) {
    if (!arr.IsArray() || arr.Size() < 3) return def;
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

inline glm::vec4 ParseVec4(const rapidjson::Value& arr, glm::vec4 def = glm::vec4(1.0f)) {
    if (!arr.IsArray() || arr.Size() < 4) return def;
    return glm::vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

inline glm::vec2 ParseVec2(const rapidjson::Value& arr, glm::vec2 def = glm::vec2(0.0f)) {
    if (!arr.IsArray() || arr.Size() < 2) return def;
    return glm::vec2(arr[0].GetFloat(), arr[1].GetFloat());
}


inline int CountValidEntities(entt::registry& registry) {
    int count = 0;
    auto view = registry.view<entt::entity>();
    for (auto entity : view) {
        if (registry.valid(entity)) ++count;
    }
    return count;
}

} // namespace dse::editor

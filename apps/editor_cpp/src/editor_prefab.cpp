#include "editor_prefab.h"
#include "editor_prefab_marker.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "editor_shared_components.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <filesystem>

namespace dse::editor {

namespace {

void WriteVec3(rapidjson::PrettyWriter<rapidjson::StringBuffer>& w, const char* key, const glm::vec3& v) {
    w.Key(key);
    w.StartArray();
    w.Double(v.x); w.Double(v.y); w.Double(v.z);
    w.EndArray();
}

void WriteVec4(rapidjson::PrettyWriter<rapidjson::StringBuffer>& w, const char* key, const glm::vec4& v) {
    w.Key(key);
    w.StartArray();
    w.Double(v.x); w.Double(v.y); w.Double(v.z); w.Double(v.w);
    w.EndArray();
}

void WriteQuat(rapidjson::PrettyWriter<rapidjson::StringBuffer>& w, const char* key, const glm::quat& q) {
    w.Key(key);
    w.StartArray();
    w.Double(q.w); w.Double(q.x); w.Double(q.y); w.Double(q.z);
    w.EndArray();
}

glm::vec3 ReadVec3(const rapidjson::Value& arr) {
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

glm::vec4 ReadVec4(const rapidjson::Value& arr) {
    return glm::vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

glm::quat ReadQuat(const rapidjson::Value& arr) {
    return glm::quat(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

} // namespace

bool SaveEntityAsPrefab(entt::registry& registry, entt::entity entity, const std::string& file_path) {
    if (!registry.valid(entity)) return false;

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();

    writer.Key("type");
    writer.String("dprefab");
    writer.Key("version");
    writer.Int(1);

    // Name
    if (registry.all_of<EditorNameComponent>(entity)) {
        writer.Key("name");
        writer.String(registry.get<EditorNameComponent>(entity).name.c_str());
    }

    // Transform
    if (registry.all_of<TransformComponent>(entity)) {
        const auto& t = registry.get<TransformComponent>(entity);
        writer.Key("transform");
        writer.StartObject();
        WriteVec3(writer, "position", t.position);
        WriteQuat(writer, "rotation", t.rotation);
        WriteVec3(writer, "scale", t.scale);
        writer.EndObject();
    }

    // MeshRenderer
    if (registry.all_of<dse::MeshRendererComponent>(entity)) {
        const auto& m = registry.get<dse::MeshRendererComponent>(entity);
        writer.Key("mesh_renderer");
        writer.StartObject();
        writer.Key("mesh_path"); writer.String(m.mesh_path.c_str());
        writer.Key("shader_variant"); writer.String(m.shader_variant.c_str());
        WriteVec4(writer, "color", m.color);
        writer.Key("metallic"); writer.Double(m.metallic);
        writer.Key("roughness"); writer.Double(m.roughness);
        writer.Key("ao"); writer.Double(m.ao);
        writer.Key("visible"); writer.Bool(m.visible);
        writer.EndObject();
    }

    // Animator3D
    if (registry.all_of<dse::Animator3DComponent>(entity)) {
        const auto& a = registry.get<dse::Animator3DComponent>(entity);
        writer.Key("animator_3d");
        writer.StartObject();
        writer.Key("dskel_path"); writer.String(a.dskel_path.c_str());
        writer.Key("danim_path"); writer.String(a.danim_path.c_str());
        writer.Key("speed"); writer.Double(a.speed);
        writer.Key("loop"); writer.Bool(a.loop);
        writer.EndObject();
    }

    writer.EndObject();

    std::ofstream ofs(file_path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << buffer.GetString();
    return true;
}

entt::entity InstantiatePrefab(World& world, entt::registry& registry, const std::string& file_path) {
    std::ifstream ifs(file_path);
    if (!ifs.is_open()) return entt::null;

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        return entt::null;
    }

    if (!doc.HasMember("type") || std::string(doc["type"].GetString()) != "dprefab") {
        return entt::null;
    }

    entt::entity entity = world.CreateEntity();

    // Name
    if (doc.HasMember("name") && doc["name"].IsString()) {
        registry.emplace<EditorNameComponent>(entity, std::string(doc["name"].GetString()));
    } else {
        registry.emplace<EditorNameComponent>(entity, "Prefab Instance");
    }

    // Transform
    if (doc.HasMember("transform") && doc["transform"].IsObject()) {
        const auto& t = doc["transform"];
        TransformComponent tc;
        if (t.HasMember("position")) tc.position = ReadVec3(t["position"]);
        if (t.HasMember("rotation")) tc.rotation = ReadQuat(t["rotation"]);
        if (t.HasMember("scale")) tc.scale = ReadVec3(t["scale"]);
        tc.dirty = true;
        registry.emplace<TransformComponent>(entity, tc);
    } else {
        registry.emplace<TransformComponent>(entity);
    }

    // MeshRenderer
    if (doc.HasMember("mesh_renderer") && doc["mesh_renderer"].IsObject()) {
        const auto& m = doc["mesh_renderer"];
        dse::MeshRendererComponent mc;
        if (m.HasMember("mesh_path")) mc.mesh_path = m["mesh_path"].GetString();
        if (m.HasMember("shader_variant")) mc.shader_variant = m["shader_variant"].GetString();
        if (m.HasMember("color")) mc.color = ReadVec4(m["color"]);
        if (m.HasMember("metallic")) mc.metallic = m["metallic"].GetFloat();
        if (m.HasMember("roughness")) mc.roughness = m["roughness"].GetFloat();
        if (m.HasMember("ao")) mc.ao = m["ao"].GetFloat();
        if (m.HasMember("visible")) mc.visible = m["visible"].GetBool();
        registry.emplace<dse::MeshRendererComponent>(entity, mc);
    }

    // Animator3D
    if (doc.HasMember("animator_3d") && doc["animator_3d"].IsObject()) {
        const auto& a = doc["animator_3d"];
        dse::Animator3DComponent ac;
        if (a.HasMember("dskel_path")) ac.dskel_path = a["dskel_path"].GetString();
        if (a.HasMember("danim_path")) ac.danim_path = a["danim_path"].GetString();
        if (a.HasMember("speed")) ac.speed = a["speed"].GetFloat();
        if (a.HasMember("loop")) ac.loop = a["loop"].GetBool();
        registry.emplace<dse::Animator3DComponent>(entity, ac);
    }

    // Mark as prefab instance
    registry.emplace<PrefabMarkerComponent>(entity, PrefabMarkerComponent{file_path});

    return entity;
}

bool IsPrefabInstance(entt::registry& registry, entt::entity entity) {
    return registry.valid(entity) && registry.all_of<PrefabMarkerComponent>(entity);
}

} // namespace dse::editor

#include "editor_prefab.h"
#include "editor_prefab_marker.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/script.h"
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

// prefab 文件可能被手工编辑/损坏：非数组/长度不足/元素非数值时直接索引或 GetFloat
// 会触发 rapidjson 断言崩溃。校验失败时返回传入默认值，保持组件原值。
glm::vec3 ReadVec3(const rapidjson::Value& arr, glm::vec3 def = glm::vec3(0.0f)) {
    if (!arr.IsArray() || arr.Size() < 3 ||
        !arr[0].IsNumber() || !arr[1].IsNumber() || !arr[2].IsNumber())
        return def;
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

glm::vec4 ReadVec4(const rapidjson::Value& arr, glm::vec4 def = glm::vec4(1.0f)) {
    if (!arr.IsArray() || arr.Size() < 4 ||
        !arr[0].IsNumber() || !arr[1].IsNumber() || !arr[2].IsNumber() || !arr[3].IsNumber())
        return def;
    return glm::vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

glm::quat ReadQuat(const rapidjson::Value& arr, glm::quat def = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
    if (!arr.IsArray() || arr.Size() < 4 ||
        !arr[0].IsNumber() || !arr[1].IsNumber() || !arr[2].IsNumber() || !arr[3].IsNumber())
        return def;
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

    // DirectionalLight3D
    if (registry.all_of<dse::DirectionalLight3DComponent>(entity)) {
        const auto& l = registry.get<dse::DirectionalLight3DComponent>(entity);
        writer.Key("directional_light");
        writer.StartObject();
        writer.Key("enabled"); writer.Bool(l.enabled);
        WriteVec3(writer, "direction", l.direction);
        WriteVec3(writer, "color", l.color);
        writer.Key("intensity"); writer.Double(l.intensity);
        writer.Key("ambient_intensity"); writer.Double(l.ambient_intensity);
        writer.Key("cast_shadow"); writer.Bool(l.cast_shadow);
        writer.EndObject();
    }

    // PointLight
    if (registry.all_of<dse::PointLightComponent>(entity)) {
        const auto& l = registry.get<dse::PointLightComponent>(entity);
        writer.Key("point_light");
        writer.StartObject();
        writer.Key("enabled"); writer.Bool(l.enabled);
        WriteVec3(writer, "color", l.color);
        writer.Key("intensity"); writer.Double(l.intensity);
        writer.Key("radius"); writer.Double(l.radius);
        writer.Key("falloff"); writer.Double(l.falloff);
        writer.Key("cast_shadow"); writer.Bool(l.cast_shadow);
        writer.EndObject();
    }

    // SpotLight
    if (registry.all_of<dse::SpotLightComponent>(entity)) {
        const auto& l = registry.get<dse::SpotLightComponent>(entity);
        writer.Key("spot_light");
        writer.StartObject();
        writer.Key("enabled"); writer.Bool(l.enabled);
        WriteVec3(writer, "color", l.color);
        WriteVec3(writer, "direction", l.direction);
        writer.Key("intensity"); writer.Double(l.intensity);
        writer.Key("radius"); writer.Double(l.radius);
        writer.Key("falloff"); writer.Double(l.falloff);
        writer.Key("inner_cone_angle"); writer.Double(l.inner_cone_angle);
        writer.Key("outer_cone_angle"); writer.Double(l.outer_cone_angle);
        writer.Key("cast_shadow"); writer.Bool(l.cast_shadow);
        writer.EndObject();
    }

    // Camera3D
    if (registry.all_of<dse::Camera3DComponent>(entity)) {
        const auto& c = registry.get<dse::Camera3DComponent>(entity);
        writer.Key("camera_3d");
        writer.StartObject();
        writer.Key("enabled"); writer.Bool(c.enabled);
        writer.Key("priority"); writer.Int(c.priority);
        writer.Key("fov"); writer.Double(c.fov);
        writer.Key("near_clip"); writer.Double(c.near_clip);
        writer.Key("far_clip"); writer.Double(c.far_clip);
        writer.EndObject();
    }

    // RigidBody3D
    if (registry.all_of<dse::RigidBody3DComponent>(entity)) {
        const auto& r = registry.get<dse::RigidBody3DComponent>(entity);
        writer.Key("rigidbody_3d");
        writer.StartObject();
        writer.Key("type"); writer.Int(static_cast<int>(r.type));
        writer.Key("mass"); writer.Double(r.mass);
        writer.Key("drag"); writer.Double(r.drag);
        writer.Key("angular_drag"); writer.Double(r.angular_drag);
        writer.Key("use_gravity"); writer.Bool(r.use_gravity);
        writer.Key("gravity_scale"); writer.Double(r.gravity_scale);
        writer.Key("collision_layer"); writer.Uint(r.collision_layer);
        writer.Key("collision_mask"); writer.Uint(r.collision_mask);
        writer.EndObject();
    }

    // BoxCollider3D
    if (registry.all_of<dse::BoxCollider3DComponent>(entity)) {
        const auto& b = registry.get<dse::BoxCollider3DComponent>(entity);
        writer.Key("box_collider_3d");
        writer.StartObject();
        WriteVec3(writer, "size", b.size);
        WriteVec3(writer, "center", b.center);
        writer.Key("is_trigger"); writer.Bool(b.is_trigger);
        writer.Key("bounciness"); writer.Double(b.bounciness);
        writer.Key("friction"); writer.Double(b.friction);
        writer.EndObject();
    }

    // SphereCollider3D
    if (registry.all_of<dse::SphereCollider3DComponent>(entity)) {
        const auto& s = registry.get<dse::SphereCollider3DComponent>(entity);
        writer.Key("sphere_collider_3d");
        writer.StartObject();
        writer.Key("radius"); writer.Double(s.radius);
        WriteVec3(writer, "center", s.center);
        writer.Key("is_trigger"); writer.Bool(s.is_trigger);
        writer.Key("bounciness"); writer.Double(s.bounciness);
        writer.Key("friction"); writer.Double(s.friction);
        writer.EndObject();
    }

    // CapsuleCollider3D
    if (registry.all_of<dse::CapsuleCollider3DComponent>(entity)) {
        const auto& c = registry.get<dse::CapsuleCollider3DComponent>(entity);
        writer.Key("capsule_collider_3d");
        writer.StartObject();
        writer.Key("radius"); writer.Double(c.radius);
        writer.Key("height"); writer.Double(c.height);
        WriteVec3(writer, "center", c.center);
        writer.Key("direction"); writer.Int(c.direction);
        writer.Key("is_trigger"); writer.Bool(c.is_trigger);
        writer.Key("bounciness"); writer.Double(c.bounciness);
        writer.Key("friction"); writer.Double(c.friction);
        writer.EndObject();
    }

    // LuaScript
    if (registry.all_of<LuaScriptComponent>(entity)) {
        const auto& s = registry.get<LuaScriptComponent>(entity);
        writer.Key("lua_script");
        writer.StartObject();
        writer.Key("script_path"); writer.String(s.script_path.c_str());
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

    if (!doc.HasMember("type") || !doc["type"].IsString() ||
        std::string(doc["type"].GetString()) != "dprefab") {
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
        if (t.HasMember("position")) tc.position = ReadVec3(t["position"], tc.position);
        if (t.HasMember("rotation")) tc.rotation = ReadQuat(t["rotation"], tc.rotation);
        if (t.HasMember("scale")) tc.scale = ReadVec3(t["scale"], tc.scale);
        tc.dirty = true;
        registry.emplace<TransformComponent>(entity, tc);
    } else {
        registry.emplace<TransformComponent>(entity);
    }

    // MeshRenderer
    if (doc.HasMember("mesh_renderer") && doc["mesh_renderer"].IsObject()) {
        const auto& m = doc["mesh_renderer"];
        dse::MeshRendererComponent mc;
        if (m.HasMember("mesh_path") && m["mesh_path"].IsString()) mc.mesh_path = m["mesh_path"].GetString();
        if (m.HasMember("shader_variant") && m["shader_variant"].IsString()) mc.shader_variant = m["shader_variant"].GetString();
        if (m.HasMember("color")) mc.color = ReadVec4(m["color"], mc.color);
        if (m.HasMember("metallic") && m["metallic"].IsNumber()) mc.metallic = m["metallic"].GetFloat();
        if (m.HasMember("roughness") && m["roughness"].IsNumber()) mc.roughness = m["roughness"].GetFloat();
        if (m.HasMember("ao") && m["ao"].IsNumber()) mc.ao = m["ao"].GetFloat();
        if (m.HasMember("visible") && m["visible"].IsBool()) mc.visible = m["visible"].GetBool();
        registry.emplace<dse::MeshRendererComponent>(entity, mc);
    }

    // Animator3D
    if (doc.HasMember("animator_3d") && doc["animator_3d"].IsObject()) {
        const auto& a = doc["animator_3d"];
        dse::Animator3DComponent ac;
        if (a.HasMember("dskel_path") && a["dskel_path"].IsString()) ac.dskel_path = a["dskel_path"].GetString();
        if (a.HasMember("danim_path") && a["danim_path"].IsString()) ac.danim_path = a["danim_path"].GetString();
        if (a.HasMember("speed") && a["speed"].IsNumber()) ac.speed = a["speed"].GetFloat();
        if (a.HasMember("loop") && a["loop"].IsBool()) ac.loop = a["loop"].GetBool();
        registry.emplace<dse::Animator3DComponent>(entity, ac);
    }

    // DirectionalLight3D
    if (doc.HasMember("directional_light") && doc["directional_light"].IsObject()) {
        const auto& l = doc["directional_light"];
        dse::DirectionalLight3DComponent lc;
        if (l.HasMember("enabled") && l["enabled"].IsBool()) lc.enabled = l["enabled"].GetBool();
        if (l.HasMember("direction")) lc.direction = ReadVec3(l["direction"], lc.direction);
        if (l.HasMember("color")) lc.color = ReadVec3(l["color"], lc.color);
        if (l.HasMember("intensity") && l["intensity"].IsNumber()) lc.intensity = l["intensity"].GetFloat();
        if (l.HasMember("ambient_intensity") && l["ambient_intensity"].IsNumber()) lc.ambient_intensity = l["ambient_intensity"].GetFloat();
        if (l.HasMember("cast_shadow") && l["cast_shadow"].IsBool()) lc.cast_shadow = l["cast_shadow"].GetBool();
        registry.emplace<dse::DirectionalLight3DComponent>(entity, lc);
    }

    // PointLight
    if (doc.HasMember("point_light") && doc["point_light"].IsObject()) {
        const auto& l = doc["point_light"];
        dse::PointLightComponent lc;
        if (l.HasMember("enabled") && l["enabled"].IsBool()) lc.enabled = l["enabled"].GetBool();
        if (l.HasMember("color")) lc.color = ReadVec3(l["color"], lc.color);
        if (l.HasMember("intensity") && l["intensity"].IsNumber()) lc.intensity = l["intensity"].GetFloat();
        if (l.HasMember("radius") && l["radius"].IsNumber()) lc.radius = l["radius"].GetFloat();
        if (l.HasMember("falloff") && l["falloff"].IsNumber()) lc.falloff = l["falloff"].GetFloat();
        if (l.HasMember("cast_shadow") && l["cast_shadow"].IsBool()) lc.cast_shadow = l["cast_shadow"].GetBool();
        registry.emplace<dse::PointLightComponent>(entity, lc);
    }

    // SpotLight
    if (doc.HasMember("spot_light") && doc["spot_light"].IsObject()) {
        const auto& l = doc["spot_light"];
        dse::SpotLightComponent lc;
        if (l.HasMember("enabled") && l["enabled"].IsBool()) lc.enabled = l["enabled"].GetBool();
        if (l.HasMember("color")) lc.color = ReadVec3(l["color"], lc.color);
        if (l.HasMember("direction")) lc.direction = ReadVec3(l["direction"], lc.direction);
        if (l.HasMember("intensity") && l["intensity"].IsNumber()) lc.intensity = l["intensity"].GetFloat();
        if (l.HasMember("radius") && l["radius"].IsNumber()) lc.radius = l["radius"].GetFloat();
        if (l.HasMember("falloff") && l["falloff"].IsNumber()) lc.falloff = l["falloff"].GetFloat();
        if (l.HasMember("inner_cone_angle") && l["inner_cone_angle"].IsNumber()) lc.inner_cone_angle = l["inner_cone_angle"].GetFloat();
        if (l.HasMember("outer_cone_angle") && l["outer_cone_angle"].IsNumber()) lc.outer_cone_angle = l["outer_cone_angle"].GetFloat();
        if (l.HasMember("cast_shadow") && l["cast_shadow"].IsBool()) lc.cast_shadow = l["cast_shadow"].GetBool();
        registry.emplace<dse::SpotLightComponent>(entity, lc);
    }

    // Camera3D
    if (doc.HasMember("camera_3d") && doc["camera_3d"].IsObject()) {
        const auto& c = doc["camera_3d"];
        dse::Camera3DComponent cc;
        if (c.HasMember("enabled") && c["enabled"].IsBool()) cc.enabled = c["enabled"].GetBool();
        if (c.HasMember("priority") && c["priority"].IsInt()) cc.priority = c["priority"].GetInt();
        if (c.HasMember("fov") && c["fov"].IsNumber()) cc.fov = c["fov"].GetFloat();
        if (c.HasMember("near_clip") && c["near_clip"].IsNumber()) cc.near_clip = c["near_clip"].GetFloat();
        if (c.HasMember("far_clip") && c["far_clip"].IsNumber()) cc.far_clip = c["far_clip"].GetFloat();
        registry.emplace<dse::Camera3DComponent>(entity, cc);
    }

    // RigidBody3D
    if (doc.HasMember("rigidbody_3d") && doc["rigidbody_3d"].IsObject()) {
        const auto& r = doc["rigidbody_3d"];
        dse::RigidBody3DComponent rc;
        if (r.HasMember("type") && r["type"].IsInt())
            rc.type = static_cast<dse::RigidBody3DType>(r["type"].GetInt());
        if (r.HasMember("mass") && r["mass"].IsNumber()) rc.mass = r["mass"].GetFloat();
        if (r.HasMember("drag") && r["drag"].IsNumber()) rc.drag = r["drag"].GetFloat();
        if (r.HasMember("angular_drag") && r["angular_drag"].IsNumber()) rc.angular_drag = r["angular_drag"].GetFloat();
        if (r.HasMember("use_gravity") && r["use_gravity"].IsBool()) rc.use_gravity = r["use_gravity"].GetBool();
        if (r.HasMember("gravity_scale") && r["gravity_scale"].IsNumber()) rc.gravity_scale = r["gravity_scale"].GetFloat();
        if (r.HasMember("collision_layer") && r["collision_layer"].IsUint()) rc.collision_layer = static_cast<uint16_t>(r["collision_layer"].GetUint());
        if (r.HasMember("collision_mask") && r["collision_mask"].IsUint()) rc.collision_mask = static_cast<uint16_t>(r["collision_mask"].GetUint());
        registry.emplace<dse::RigidBody3DComponent>(entity, rc);
    }

    // BoxCollider3D
    if (doc.HasMember("box_collider_3d") && doc["box_collider_3d"].IsObject()) {
        const auto& b = doc["box_collider_3d"];
        dse::BoxCollider3DComponent bc;
        if (b.HasMember("size")) bc.size = ReadVec3(b["size"], bc.size);
        if (b.HasMember("center")) bc.center = ReadVec3(b["center"], bc.center);
        if (b.HasMember("is_trigger") && b["is_trigger"].IsBool()) bc.is_trigger = b["is_trigger"].GetBool();
        if (b.HasMember("bounciness") && b["bounciness"].IsNumber()) bc.bounciness = b["bounciness"].GetFloat();
        if (b.HasMember("friction") && b["friction"].IsNumber()) bc.friction = b["friction"].GetFloat();
        registry.emplace<dse::BoxCollider3DComponent>(entity, bc);
    }

    // SphereCollider3D
    if (doc.HasMember("sphere_collider_3d") && doc["sphere_collider_3d"].IsObject()) {
        const auto& s = doc["sphere_collider_3d"];
        dse::SphereCollider3DComponent sc;
        if (s.HasMember("radius") && s["radius"].IsNumber()) sc.radius = s["radius"].GetFloat();
        if (s.HasMember("center")) sc.center = ReadVec3(s["center"], sc.center);
        if (s.HasMember("is_trigger") && s["is_trigger"].IsBool()) sc.is_trigger = s["is_trigger"].GetBool();
        if (s.HasMember("bounciness") && s["bounciness"].IsNumber()) sc.bounciness = s["bounciness"].GetFloat();
        if (s.HasMember("friction") && s["friction"].IsNumber()) sc.friction = s["friction"].GetFloat();
        registry.emplace<dse::SphereCollider3DComponent>(entity, sc);
    }

    // CapsuleCollider3D
    if (doc.HasMember("capsule_collider_3d") && doc["capsule_collider_3d"].IsObject()) {
        const auto& c = doc["capsule_collider_3d"];
        dse::CapsuleCollider3DComponent cc;
        if (c.HasMember("radius") && c["radius"].IsNumber()) cc.radius = c["radius"].GetFloat();
        if (c.HasMember("height") && c["height"].IsNumber()) cc.height = c["height"].GetFloat();
        if (c.HasMember("center")) cc.center = ReadVec3(c["center"], cc.center);
        if (c.HasMember("direction") && c["direction"].IsInt()) cc.direction = c["direction"].GetInt();
        if (c.HasMember("is_trigger") && c["is_trigger"].IsBool()) cc.is_trigger = c["is_trigger"].GetBool();
        if (c.HasMember("bounciness") && c["bounciness"].IsNumber()) cc.bounciness = c["bounciness"].GetFloat();
        if (c.HasMember("friction") && c["friction"].IsNumber()) cc.friction = c["friction"].GetFloat();
        registry.emplace<dse::CapsuleCollider3DComponent>(entity, cc);
    }

    // LuaScript
    if (doc.HasMember("lua_script") && doc["lua_script"].IsObject()) {
        const auto& s = doc["lua_script"];
        LuaScriptComponent sc;
        if (s.HasMember("script_path") && s["script_path"].IsString()) sc.script_path = s["script_path"].GetString();
        registry.emplace<LuaScriptComponent>(entity, sc);
    }

    // Mark as prefab instance
    registry.emplace<PrefabMarkerComponent>(entity, PrefabMarkerComponent{file_path});

    return entity;
}

bool IsPrefabInstance(entt::registry& registry, entt::entity entity) {
    return registry.valid(entity) && registry.all_of<PrefabMarkerComponent>(entity);
}

} // namespace dse::editor

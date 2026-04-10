/**
 * @file scene.cpp
 * @brief 场景管理系统，处理场景图(Scene Graph)、节点变换和空间划分
 */

#include "engine/scene/scene.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/base/debug.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

namespace scene {

constexpr int kCurrentMaterialSchemaVersion = 2;
constexpr int kCurrentPrefabSchemaVersion = 1;

Scene::Scene(const std::string& name) : name_(name), world_(&owned_world_) {
}

Scene::~Scene() {
}

void Scene::BindWorld(World* world) {
    if (world) {
        world_ = world;
    }
}

void Scene::UnbindWorld() {
    world_ = &owned_world_;
}

World& Scene::ActiveWorld() {
    return world_ ? *world_ : owned_world_;
}

const World& Scene::ActiveWorld() const {
    return world_ ? *world_ : owned_world_;
}

bool Scene::Serialize(const std::string& filepath) {
    DEBUG_LOG_INFO("Serializing scene {} to {}", name_, filepath);
    auto& world = ActiveWorld();

    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    doc.AddMember("name", rapidjson::Value(name_.c_str(), allocator), allocator);
    doc.AddMember("material_schema_version", kCurrentMaterialSchemaVersion, allocator);

    rapidjson::Value entities(rapidjson::kArrayType);
    rapidjson::Value materials(rapidjson::kArrayType);

    auto material_view = world.registry().view<MaterialInstanceComponent>();
    for (auto entity : material_view) {
        const auto& m = material_view.get<MaterialInstanceComponent>(entity);
        rapidjson::Value material_json(rapidjson::kObjectType);
        material_json.AddMember("material_id", m.material_id, allocator);
        material_json.AddMember("name", rapidjson::Value(m.name.c_str(), allocator), allocator);
        material_json.AddMember("shader_variant", rapidjson::Value(m.shader_variant.c_str(), allocator), allocator);
        material_json.AddMember("blend_mode", static_cast<int>(m.blend_mode), allocator);
        material_json.AddMember("texture_handle", m.texture_handle, allocator);
        rapidjson::Value tint(rapidjson::kArrayType);
        tint.PushBack(m.tint.r, allocator).PushBack(m.tint.g, allocator).PushBack(m.tint.b, allocator).PushBack(m.tint.a, allocator);
        material_json.AddMember("tint", tint, allocator);
        rapidjson::Value uv_rect(rapidjson::kArrayType);
        uv_rect.PushBack(m.uv_rect.x, allocator).PushBack(m.uv_rect.y, allocator).PushBack(m.uv_rect.z, allocator).PushBack(m.uv_rect.w, allocator);
        material_json.AddMember("uv_rect", uv_rect, allocator);
        materials.PushBack(material_json, allocator);
    }

    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        auto& t = view.get<TransformComponent>(entity);

        rapidjson::Value entity_json(rapidjson::kObjectType);
        entity_json.AddMember("id", static_cast<uint32_t>(entity), allocator);

        rapidjson::Value components(rapidjson::kObjectType);
        rapidjson::Value transform_json(rapidjson::kObjectType);

        rapidjson::Value position(rapidjson::kArrayType);
        position.PushBack(t.position.x, allocator).PushBack(t.position.y, allocator).PushBack(t.position.z, allocator);
        transform_json.AddMember("position", position, allocator);

        rapidjson::Value rotation(rapidjson::kArrayType);
        rotation.PushBack(t.rotation.x, allocator).PushBack(t.rotation.y, allocator).PushBack(t.rotation.z, allocator).PushBack(t.rotation.w, allocator);
        transform_json.AddMember("rotation", rotation, allocator);

        rapidjson::Value scale(rapidjson::kArrayType);
        scale.PushBack(t.scale.x, allocator).PushBack(t.scale.y, allocator).PushBack(t.scale.z, allocator);
        transform_json.AddMember("scale", scale, allocator);

        components.AddMember("TransformComponent", transform_json, allocator);

        if (world.registry().all_of<SpriteRendererComponent>(entity)) {
            auto& s = world.registry().get<SpriteRendererComponent>(entity);

            rapidjson::Value sprite_json(rapidjson::kObjectType);
            sprite_json.AddMember("texture_handle", s.texture_handle, allocator);
            sprite_json.AddMember("material_instance_id", s.material_instance_id, allocator);
            sprite_json.AddMember("shader_variant", rapidjson::Value(s.shader_variant.c_str(), allocator), allocator);
            sprite_json.AddMember("blend_mode", static_cast<int>(s.blend_mode), allocator);

            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(s.color.r, allocator).PushBack(s.color.g, allocator).PushBack(s.color.b, allocator).PushBack(s.color.a, allocator);
            sprite_json.AddMember("color", color, allocator);

            rapidjson::Value uv(rapidjson::kArrayType);
            uv.PushBack(s.uv.x, allocator).PushBack(s.uv.y, allocator).PushBack(s.uv.z, allocator).PushBack(s.uv.w, allocator);
            sprite_json.AddMember("uv", uv, allocator);

            sprite_json.AddMember("sorting_layer", s.sorting_layer, allocator);
            sprite_json.AddMember("order_in_layer", s.order_in_layer, allocator);
            sprite_json.AddMember("visible", s.visible, allocator);
            components.AddMember("SpriteRendererComponent", sprite_json, allocator);
        }

        if (world.registry().all_of<ParentComponent>(entity)) {
            const auto& parent = world.registry().get<ParentComponent>(entity);
            rapidjson::Value parent_json(rapidjson::kObjectType);
            parent_json.AddMember("parent", static_cast<uint32_t>(parent.parent), allocator);
            components.AddMember("ParentComponent", parent_json, allocator);
        }

        if (world.registry().all_of<ScriptComponent>(entity)) {
            const auto& script = world.registry().get<ScriptComponent>(entity);
            rapidjson::Value script_json(rapidjson::kObjectType);
            script_json.AddMember("script_path", rapidjson::Value(script.script_path.c_str(), allocator), allocator);
            script_json.AddMember("enabled", script.enabled, allocator);
            components.AddMember("ScriptComponent", script_json, allocator);
        }

        if (world.registry().all_of<dse::MeshRendererComponent>(entity)) {
            const auto& mesh = world.registry().get<dse::MeshRendererComponent>(entity);
            rapidjson::Value mesh_json(rapidjson::kObjectType);
            mesh_json.AddMember("mesh_path", rapidjson::Value(mesh.mesh_path.c_str(), allocator), allocator);
            mesh_json.AddMember("material_instance_id", mesh.material_instance_id, allocator);
            mesh_json.AddMember("shader_variant", rapidjson::Value(mesh.shader_variant.c_str(), allocator), allocator);
            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(mesh.color.r, allocator).PushBack(mesh.color.g, allocator).PushBack(mesh.color.b, allocator).PushBack(mesh.color.a, allocator);
            mesh_json.AddMember("color", color, allocator);
            rapidjson::Value emissive(rapidjson::kArrayType);
            emissive.PushBack(mesh.emissive.x, allocator).PushBack(mesh.emissive.y, allocator).PushBack(mesh.emissive.z, allocator);
            mesh_json.AddMember("emissive", emissive, allocator);
            mesh_json.AddMember("metallic", mesh.metallic, allocator);
            mesh_json.AddMember("roughness", mesh.roughness, allocator);
            mesh_json.AddMember("ao", mesh.ao, allocator);
            mesh_json.AddMember("normal_strength", mesh.normal_strength, allocator);
            mesh_json.AddMember("receive_shadow", mesh.receive_shadow, allocator);
            mesh_json.AddMember("visible", mesh.visible, allocator);
            components.AddMember("MeshRendererComponent", mesh_json, allocator);
        }

        if (world.registry().all_of<dse::Camera3DComponent>(entity)) {
            const auto& camera = world.registry().get<dse::Camera3DComponent>(entity);
            rapidjson::Value camera_json(rapidjson::kObjectType);
            camera_json.AddMember("enabled", camera.enabled, allocator);
            camera_json.AddMember("priority", camera.priority, allocator);
            camera_json.AddMember("fov", camera.fov, allocator);
            camera_json.AddMember("aspect_ratio", camera.aspect_ratio, allocator);
            camera_json.AddMember("near_clip", camera.near_clip, allocator);
            camera_json.AddMember("far_clip", camera.far_clip, allocator);
            components.AddMember("Camera3DComponent", camera_json, allocator);
        }

        if (world.registry().all_of<dse::DirectionalLight3DComponent>(entity)) {
            const auto& light = world.registry().get<dse::DirectionalLight3DComponent>(entity);
            rapidjson::Value light_json(rapidjson::kObjectType);
            light_json.AddMember("enabled", light.enabled, allocator);
            rapidjson::Value direction(rapidjson::kArrayType);
            direction.PushBack(light.direction.x, allocator).PushBack(light.direction.y, allocator).PushBack(light.direction.z, allocator);
            light_json.AddMember("direction", direction, allocator);
            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(light.color.x, allocator).PushBack(light.color.y, allocator).PushBack(light.color.z, allocator);
            light_json.AddMember("color", color, allocator);
            light_json.AddMember("intensity", light.intensity, allocator);
            light_json.AddMember("ambient_intensity", light.ambient_intensity, allocator);
            light_json.AddMember("shadow_strength", light.shadow_strength, allocator);
            light_json.AddMember("cast_shadow", light.cast_shadow, allocator);
            rapidjson::Value cascade_splits(rapidjson::kArrayType);
            for (int i = 0; i < CSM_CASCADES; ++i) {
                cascade_splits.PushBack(light.cascade_splits[i], allocator);
            }
            light_json.AddMember("cascade_splits", cascade_splits, allocator);
            components.AddMember("DirectionalLight3DComponent", light_json, allocator);
        }

        if (world.registry().all_of<dse::SkyboxComponent>(entity)) {
            const auto& skybox = world.registry().get<dse::SkyboxComponent>(entity);
            rapidjson::Value skybox_json(rapidjson::kObjectType);
            skybox_json.AddMember("enabled", skybox.enabled, allocator);
            skybox_json.AddMember("cubemap_handle", skybox.cubemap_handle, allocator);
            skybox_json.AddMember("cubemap_path", rapidjson::Value(skybox.cubemap_path.c_str(), allocator), allocator);
            components.AddMember("SkyboxComponent", skybox_json, allocator);
        }

        if (world.registry().all_of<dse::PointLightComponent>(entity)) {
            const auto& light = world.registry().get<dse::PointLightComponent>(entity);
            rapidjson::Value light_json(rapidjson::kObjectType);
            light_json.AddMember("enabled", light.enabled, allocator);
            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(light.color.x, allocator).PushBack(light.color.y, allocator).PushBack(light.color.z, allocator);
            light_json.AddMember("color", color, allocator);
            light_json.AddMember("intensity", light.intensity, allocator);
            light_json.AddMember("radius", light.radius, allocator);
            light_json.AddMember("cast_shadow", light.cast_shadow, allocator);
            components.AddMember("PointLightComponent", light_json, allocator);
        }

        if (world.registry().all_of<dse::Animator3DComponent>(entity)) {
            const auto& animator = world.registry().get<dse::Animator3DComponent>(entity);
            rapidjson::Value animator_json(rapidjson::kObjectType);
            animator_json.AddMember("enabled", animator.enabled, allocator);
            animator_json.AddMember("dskel_path", rapidjson::Value(animator.dskel_path.c_str(), allocator), allocator);
            animator_json.AddMember("danim_path", rapidjson::Value(animator.danim_path.c_str(), allocator), allocator);
            animator_json.AddMember("speed", animator.speed, allocator);
            animator_json.AddMember("loop", animator.loop, allocator);
            animator_json.AddMember("use_anim_tree", animator.use_anim_tree, allocator);
            components.AddMember("Animator3DComponent", animator_json, allocator);
        }

        if (world.registry().all_of<dse::TerrainComponent>(entity)) {
            const auto& terrain = world.registry().get<dse::TerrainComponent>(entity);
            rapidjson::Value terrain_json(rapidjson::kObjectType);
            terrain_json.AddMember("enabled", terrain.enabled, allocator);
            terrain_json.AddMember("heightmap_path", rapidjson::Value(terrain.heightmap_path.c_str(), allocator), allocator);
            terrain_json.AddMember("texture_handle", terrain.texture_handle, allocator);
            terrain_json.AddMember("width", terrain.width, allocator);
            terrain_json.AddMember("depth", terrain.depth, allocator);
            terrain_json.AddMember("max_height", terrain.max_height, allocator);
            terrain_json.AddMember("resolution_x", terrain.resolution_x, allocator);
            terrain_json.AddMember("resolution_z", terrain.resolution_z, allocator);
            terrain_json.AddMember("use_dynamic_lod", terrain.use_dynamic_lod, allocator);
            terrain_json.AddMember("max_lod_levels", terrain.max_lod_levels, allocator);
            terrain_json.AddMember("lod_distance_factor", terrain.lod_distance_factor, allocator);
            terrain_json.AddMember("visible", terrain.visible, allocator);
            components.AddMember("TerrainComponent", terrain_json, allocator);
        }

        entity_json.AddMember("components", components, allocator);
        entities.PushBack(entity_json, allocator);
    }

    doc.AddMember("materials", materials, allocator);
    doc.AddMember("entities", entities, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream out(filepath);
    if (out.is_open()) {
        out << buffer.GetString();
        out.close();
        return true;
    }
    return false;
}

bool Scene::Deserialize(const std::string& filepath) {
    DEBUG_LOG_INFO("Deserializing scene {} from {}", name_, filepath);
    auto& world = ActiveWorld();
    std::ifstream in(filepath);
    if (!in.is_open()) return false;
    
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string json_str = buffer.str();

    rapidjson::Document doc;
    if (doc.Parse(json_str.c_str()).HasParseError() || !doc.IsObject()) {
        DEBUG_LOG_ERROR("Scene parse failed: {}", filepath);
        return false;
    }

    world.Clear();

    if (doc.HasMember("name") && doc["name"].IsString()) {
        name_ = doc["name"].GetString();
    }
    int material_schema_version = 1;
    if (doc.HasMember("material_schema_version") && doc["material_schema_version"].IsInt()) {
        material_schema_version = doc["material_schema_version"].GetInt();
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        if (!doc.HasMember("materials") || !doc["materials"].IsArray()) {
            return true;
        }
    }

    if (doc.HasMember("materials") && doc["materials"].IsArray()) {
        for (const auto& material_json : doc["materials"].GetArray()) {
            if (!material_json.IsObject()) {
                continue;
            }
            MaterialInstanceComponent material;
            if (material_json.HasMember("material_id") && material_json["material_id"].IsUint()) {
                material.material_id = material_json["material_id"].GetUint();
            }
            if (material_json.HasMember("name") && material_json["name"].IsString()) {
                material.name = material_json["name"].GetString();
            }
            if (material_json.HasMember("shader_variant") && material_json["shader_variant"].IsString()) {
                material.shader_variant = material_json["shader_variant"].GetString();
            }
            if (material_json.HasMember("blend_mode") && material_json["blend_mode"].IsInt()) {
                material.blend_mode = static_cast<SpriteBlendMode>(material_json["blend_mode"].GetInt());
            }
            if (material_json.HasMember("texture_handle") && material_json["texture_handle"].IsUint()) {
                material.texture_handle = material_json["texture_handle"].GetUint();
            }
            if (material_json.HasMember("tint") && material_json["tint"].IsArray() && material_json["tint"].Size() == 4) {
                const auto& tint = material_json["tint"].GetArray();
                material.tint = glm::vec4(tint[0].GetFloat(), tint[1].GetFloat(), tint[2].GetFloat(), tint[3].GetFloat());
            }
            if (material_json.HasMember("uv_rect") && material_json["uv_rect"].IsArray() && material_json["uv_rect"].Size() == 4) {
                const auto& uv_rect = material_json["uv_rect"].GetArray();
                material.uv_rect = glm::vec4(uv_rect[0].GetFloat(), uv_rect[1].GetFloat(), uv_rect[2].GetFloat(), uv_rect[3].GetFloat());
            }
            Entity material_entity = world.CreateEntity();
            world.registry().emplace<MaterialInstanceComponent>(material_entity, material);
        }
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        return true;
    }

    std::unordered_map<uint32_t, Entity> entity_id_map;
    std::vector<std::pair<Entity, uint32_t>> pending_parents;
    for (const auto& entity_json : doc["entities"].GetArray()) {
        if (!entity_json.IsObject()) {
            continue;
        }
        if (!entity_json.HasMember("components") || !entity_json["components"].IsObject()) {
            continue;
        }

        Entity entity = world.CreateEntity();
        if (entity_json.HasMember("id") && entity_json["id"].IsUint()) {
            entity_id_map[entity_json["id"].GetUint()] = entity;
        }
        const auto& components = entity_json["components"];

        if (components.HasMember("TransformComponent") && components["TransformComponent"].IsObject()) {
            const auto& transform_json = components["TransformComponent"];
            TransformComponent transform;

            if (transform_json.HasMember("position") && transform_json["position"].IsArray() && transform_json["position"].Size() == 3) {
                const auto& p = transform_json["position"].GetArray();
                transform.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
            }

            if (transform_json.HasMember("rotation") && transform_json["rotation"].IsArray() && transform_json["rotation"].Size() == 4) {
                const auto& r = transform_json["rotation"].GetArray();
                transform.rotation = glm::quat(r[3].GetFloat(), r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat());
            }

            if (transform_json.HasMember("scale") && transform_json["scale"].IsArray() && transform_json["scale"].Size() == 3) {
                const auto& s = transform_json["scale"].GetArray();
                transform.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
            }

            transform.dirty = true;
            world.registry().emplace<TransformComponent>(entity, transform);
        } else {
            world.registry().emplace<TransformComponent>(entity);
        }

        if (components.HasMember("SpriteRendererComponent") && components["SpriteRendererComponent"].IsObject()) {
            const auto& sprite_json = components["SpriteRendererComponent"];
            SpriteRendererComponent sprite;

            if (sprite_json.HasMember("texture_handle") && sprite_json["texture_handle"].IsUint()) {
                sprite.texture_handle = sprite_json["texture_handle"].GetUint();
            }
            if (sprite_json.HasMember("material_instance_id") && sprite_json["material_instance_id"].IsUint()) {
                sprite.material_instance_id = sprite_json["material_instance_id"].GetUint();
            }
            if (sprite_json.HasMember("shader_variant") && sprite_json["shader_variant"].IsString()) {
                sprite.shader_variant = sprite_json["shader_variant"].GetString();
            }
            if (sprite_json.HasMember("blend_mode") && sprite_json["blend_mode"].IsInt()) {
                sprite.blend_mode = static_cast<SpriteBlendMode>(sprite_json["blend_mode"].GetInt());
            }

            if (sprite_json.HasMember("color") && sprite_json["color"].IsArray() && sprite_json["color"].Size() == 4) {
                const auto& c = sprite_json["color"].GetArray();
                sprite.color = glm::vec4(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat(), c[3].GetFloat());
            }

            if (sprite_json.HasMember("uv") && sprite_json["uv"].IsArray() && sprite_json["uv"].Size() == 4) {
                const auto& uv = sprite_json["uv"].GetArray();
                sprite.uv = glm::vec4(uv[0].GetFloat(), uv[1].GetFloat(), uv[2].GetFloat(), uv[3].GetFloat());
            }

            if (sprite_json.HasMember("sorting_layer") && sprite_json["sorting_layer"].IsInt()) {
                sprite.sorting_layer = sprite_json["sorting_layer"].GetInt();
            }

            if (sprite_json.HasMember("order_in_layer") && sprite_json["order_in_layer"].IsInt()) {
                sprite.order_in_layer = sprite_json["order_in_layer"].GetInt();
            }

            if (sprite_json.HasMember("visible") && sprite_json["visible"].IsBool()) {
                sprite.visible = sprite_json["visible"].GetBool();
            }

            world.registry().emplace<SpriteRendererComponent>(entity, sprite);
        }

        if (components.HasMember("ParentComponent") && components["ParentComponent"].IsObject()) {
            const auto& parent_json = components["ParentComponent"];
            if (parent_json.HasMember("parent") && parent_json["parent"].IsUint()) {
                pending_parents.emplace_back(entity, parent_json["parent"].GetUint());
            }
        }

        if (components.HasMember("ScriptComponent") && components["ScriptComponent"].IsObject()) {
            const auto& script_json = components["ScriptComponent"];
            ScriptComponent script;
            if (script_json.HasMember("script_path") && script_json["script_path"].IsString()) {
                script.script_path = script_json["script_path"].GetString();
            }
            if (script_json.HasMember("enabled") && script_json["enabled"].IsBool()) {
                script.enabled = script_json["enabled"].GetBool();
            }
            world.registry().emplace<ScriptComponent>(entity, script);
        }

        if (components.HasMember("MeshRendererComponent") && components["MeshRendererComponent"].IsObject()) {
            const auto& mesh_json = components["MeshRendererComponent"];
            dse::MeshRendererComponent mesh;
            if (mesh_json.HasMember("mesh_path") && mesh_json["mesh_path"].IsString()) {
                mesh.mesh_path = mesh_json["mesh_path"].GetString();
            }
            if (mesh_json.HasMember("material_instance_id") && mesh_json["material_instance_id"].IsUint()) {
                mesh.material_instance_id = mesh_json["material_instance_id"].GetUint();
            }
            if (mesh_json.HasMember("shader_variant") && mesh_json["shader_variant"].IsString()) {
                mesh.shader_variant = mesh_json["shader_variant"].GetString();
            }
            if (mesh_json.HasMember("color") && mesh_json["color"].IsArray() && mesh_json["color"].Size() == 4) {
                const auto& c = mesh_json["color"].GetArray();
                mesh.color = glm::vec4(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat(), c[3].GetFloat());
            }
            if (mesh_json.HasMember("emissive") && mesh_json["emissive"].IsArray() && mesh_json["emissive"].Size() == 3) {
                const auto& e = mesh_json["emissive"].GetArray();
                mesh.emissive = glm::vec3(e[0].GetFloat(), e[1].GetFloat(), e[2].GetFloat());
            }
            if (mesh_json.HasMember("metallic") && mesh_json["metallic"].IsNumber()) {
                mesh.metallic = mesh_json["metallic"].GetFloat();
            }
            if (mesh_json.HasMember("roughness") && mesh_json["roughness"].IsNumber()) {
                mesh.roughness = mesh_json["roughness"].GetFloat();
            }
            if (mesh_json.HasMember("ao") && mesh_json["ao"].IsNumber()) {
                mesh.ao = mesh_json["ao"].GetFloat();
            }
            if (mesh_json.HasMember("normal_strength") && mesh_json["normal_strength"].IsNumber()) {
                mesh.normal_strength = mesh_json["normal_strength"].GetFloat();
            }
            if (mesh_json.HasMember("receive_shadow") && mesh_json["receive_shadow"].IsBool()) {
                mesh.receive_shadow = mesh_json["receive_shadow"].GetBool();
            }
            if (mesh_json.HasMember("visible") && mesh_json["visible"].IsBool()) {
                mesh.visible = mesh_json["visible"].GetBool();
            }
            world.registry().emplace<dse::MeshRendererComponent>(entity, mesh);
        }

        if (components.HasMember("Camera3DComponent") && components["Camera3DComponent"].IsObject()) {
            const auto& camera_json = components["Camera3DComponent"];
            dse::Camera3DComponent camera;
            if (camera_json.HasMember("enabled") && camera_json["enabled"].IsBool()) {
                camera.enabled = camera_json["enabled"].GetBool();
            }
            if (camera_json.HasMember("priority") && camera_json["priority"].IsInt()) {
                camera.priority = camera_json["priority"].GetInt();
            }
            if (camera_json.HasMember("fov") && camera_json["fov"].IsNumber()) {
                camera.fov = camera_json["fov"].GetFloat();
            }
            if (camera_json.HasMember("aspect_ratio") && camera_json["aspect_ratio"].IsNumber()) {
                camera.aspect_ratio = camera_json["aspect_ratio"].GetFloat();
            }
            if (camera_json.HasMember("near_clip") && camera_json["near_clip"].IsNumber()) {
                camera.near_clip = camera_json["near_clip"].GetFloat();
            }
            if (camera_json.HasMember("far_clip") && camera_json["far_clip"].IsNumber()) {
                camera.far_clip = camera_json["far_clip"].GetFloat();
            }
            world.registry().emplace<dse::Camera3DComponent>(entity, camera);
        }

        if (components.HasMember("DirectionalLight3DComponent") && components["DirectionalLight3DComponent"].IsObject()) {
            const auto& light_json = components["DirectionalLight3DComponent"];
            dse::DirectionalLight3DComponent light;
            if (light_json.HasMember("enabled") && light_json["enabled"].IsBool()) {
                light.enabled = light_json["enabled"].GetBool();
            }
            if (light_json.HasMember("direction") && light_json["direction"].IsArray() && light_json["direction"].Size() == 3) {
                const auto& d = light_json["direction"].GetArray();
                light.direction = glm::vec3(d[0].GetFloat(), d[1].GetFloat(), d[2].GetFloat());
            }
            if (light_json.HasMember("color") && light_json["color"].IsArray() && light_json["color"].Size() == 3) {
                const auto& c = light_json["color"].GetArray();
                light.color = glm::vec3(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat());
            }
            if (light_json.HasMember("intensity") && light_json["intensity"].IsNumber()) {
                light.intensity = light_json["intensity"].GetFloat();
            }
            if (light_json.HasMember("ambient_intensity") && light_json["ambient_intensity"].IsNumber()) {
                light.ambient_intensity = light_json["ambient_intensity"].GetFloat();
            }
            if (light_json.HasMember("shadow_strength") && light_json["shadow_strength"].IsNumber()) {
                light.shadow_strength = light_json["shadow_strength"].GetFloat();
            }
            if (light_json.HasMember("cast_shadow") && light_json["cast_shadow"].IsBool()) {
                light.cast_shadow = light_json["cast_shadow"].GetBool();
            }
            if (light_json.HasMember("cascade_splits") && light_json["cascade_splits"].IsArray()) {
                const auto& splits = light_json["cascade_splits"].GetArray();
                for (rapidjson::SizeType i = 0; i < splits.Size() && i < CSM_CASCADES; ++i) {
                    light.cascade_splits[i] = splits[i].GetFloat();
                }
            }
            world.registry().emplace<dse::DirectionalLight3DComponent>(entity, light);
        }

        if (components.HasMember("SkyboxComponent") && components["SkyboxComponent"].IsObject()) {
            const auto& skybox_json = components["SkyboxComponent"];
            dse::SkyboxComponent skybox;
            if (skybox_json.HasMember("enabled") && skybox_json["enabled"].IsBool()) {
                skybox.enabled = skybox_json["enabled"].GetBool();
            }
            if (skybox_json.HasMember("cubemap_handle") && skybox_json["cubemap_handle"].IsUint()) {
                skybox.cubemap_handle = skybox_json["cubemap_handle"].GetUint();
            }
            if (skybox_json.HasMember("cubemap_path") && skybox_json["cubemap_path"].IsString()) {
                skybox.cubemap_path = skybox_json["cubemap_path"].GetString();
            }
            world.registry().emplace<dse::SkyboxComponent>(entity, skybox);
        }

        if (components.HasMember("PointLightComponent") && components["PointLightComponent"].IsObject()) {
            const auto& light_json = components["PointLightComponent"];
            dse::PointLightComponent light;
            if (light_json.HasMember("enabled") && light_json["enabled"].IsBool()) {
                light.enabled = light_json["enabled"].GetBool();
            }
            if (light_json.HasMember("color") && light_json["color"].IsArray() && light_json["color"].Size() == 3) {
                const auto& c = light_json["color"].GetArray();
                light.color = glm::vec3(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat());
            }
            if (light_json.HasMember("intensity") && light_json["intensity"].IsNumber()) {
                light.intensity = light_json["intensity"].GetFloat();
            }
            if (light_json.HasMember("radius") && light_json["radius"].IsNumber()) {
                light.radius = light_json["radius"].GetFloat();
            }
            if (light_json.HasMember("cast_shadow") && light_json["cast_shadow"].IsBool()) {
                light.cast_shadow = light_json["cast_shadow"].GetBool();
            }
            world.registry().emplace<dse::PointLightComponent>(entity, light);
        }

        if (components.HasMember("Animator3DComponent") && components["Animator3DComponent"].IsObject()) {
            const auto& animator_json = components["Animator3DComponent"];
            dse::Animator3DComponent animator;
            if (animator_json.HasMember("enabled") && animator_json["enabled"].IsBool()) {
                animator.enabled = animator_json["enabled"].GetBool();
            }
            if (animator_json.HasMember("dskel_path") && animator_json["dskel_path"].IsString()) {
                animator.dskel_path = animator_json["dskel_path"].GetString();
            }
            if (animator_json.HasMember("danim_path") && animator_json["danim_path"].IsString()) {
                animator.danim_path = animator_json["danim_path"].GetString();
            }
            if (animator_json.HasMember("speed") && animator_json["speed"].IsNumber()) {
                animator.speed = animator_json["speed"].GetFloat();
            }
            if (animator_json.HasMember("loop") && animator_json["loop"].IsBool()) {
                animator.loop = animator_json["loop"].GetBool();
            }
            if (animator_json.HasMember("use_anim_tree") && animator_json["use_anim_tree"].IsBool()) {
                animator.use_anim_tree = animator_json["use_anim_tree"].GetBool();
            }
            world.registry().emplace<dse::Animator3DComponent>(entity, std::move(animator));
        }

        if (components.HasMember("TerrainComponent") && components["TerrainComponent"].IsObject()) {
            const auto& terrain_json = components["TerrainComponent"];
            dse::TerrainComponent terrain;
            if (terrain_json.HasMember("enabled") && terrain_json["enabled"].IsBool()) {
                terrain.enabled = terrain_json["enabled"].GetBool();
            }
            if (terrain_json.HasMember("heightmap_path") && terrain_json["heightmap_path"].IsString()) {
                terrain.heightmap_path = terrain_json["heightmap_path"].GetString();
            }
            if (terrain_json.HasMember("texture_handle") && terrain_json["texture_handle"].IsUint()) {
                terrain.texture_handle = terrain_json["texture_handle"].GetUint();
            }
            if (terrain_json.HasMember("width") && terrain_json["width"].IsNumber()) {
                terrain.width = terrain_json["width"].GetFloat();
            }
            if (terrain_json.HasMember("depth") && terrain_json["depth"].IsNumber()) {
                terrain.depth = terrain_json["depth"].GetFloat();
            }
            if (terrain_json.HasMember("max_height") && terrain_json["max_height"].IsNumber()) {
                terrain.max_height = terrain_json["max_height"].GetFloat();
            }
            if (terrain_json.HasMember("resolution_x") && terrain_json["resolution_x"].IsInt()) {
                terrain.resolution_x = terrain_json["resolution_x"].GetInt();
            }
            if (terrain_json.HasMember("resolution_z") && terrain_json["resolution_z"].IsInt()) {
                terrain.resolution_z = terrain_json["resolution_z"].GetInt();
            }
            if (terrain_json.HasMember("use_dynamic_lod") && terrain_json["use_dynamic_lod"].IsBool()) {
                terrain.use_dynamic_lod = terrain_json["use_dynamic_lod"].GetBool();
            }
            if (terrain_json.HasMember("max_lod_levels") && terrain_json["max_lod_levels"].IsInt()) {
                terrain.max_lod_levels = terrain_json["max_lod_levels"].GetInt();
            }
            if (terrain_json.HasMember("lod_distance_factor") && terrain_json["lod_distance_factor"].IsNumber()) {
                terrain.lod_distance_factor = terrain_json["lod_distance_factor"].GetFloat();
            }
            if (terrain_json.HasMember("visible") && terrain_json["visible"].IsBool()) {
                terrain.visible = terrain_json["visible"].GetBool();
            }
            terrain.is_dirty = true;
            world.registry().emplace<dse::TerrainComponent>(entity, std::move(terrain));
        }
    }

    for (const auto& pair : pending_parents) {
        auto it = entity_id_map.find(pair.second);
        ParentComponent parent_component;
        parent_component.parent = it != entity_id_map.end() ? it->second : entt::null;
        world.registry().emplace_or_replace<ParentComponent>(pair.first, parent_component);
        if (world.registry().all_of<TransformComponent>(pair.first)) {
            world.registry().get<TransformComponent>(pair.first).dirty = true;
        }
    }

    if (material_schema_version < kCurrentMaterialSchemaVersion) {
        std::unordered_map<unsigned int, MaterialInstanceComponent> material_map;
        auto material_view = world.registry().view<MaterialInstanceComponent>();
        for (auto entity : material_view) {
            const auto& material = material_view.get<MaterialInstanceComponent>(entity);
            material_map[material.material_id] = material;
        }
        auto sprite_view = world.registry().view<SpriteRendererComponent>();
        for (auto entity : sprite_view) {
            const auto& sprite = sprite_view.get<SpriteRendererComponent>(entity);
            if (sprite.material_instance_id == 0) {
                continue;
            }
            if (material_map.find(sprite.material_instance_id) != material_map.end()) {
                continue;
            }
            MaterialInstanceComponent material;
            material.material_id = sprite.material_instance_id;
            material.name = "migrated_material_" + std::to_string(sprite.material_instance_id);
            material.shader_variant = sprite.shader_variant;
            material.blend_mode = sprite.blend_mode;
            material.texture_handle = sprite.texture_handle;
            material.tint = sprite.color;
            material.uv_rect = sprite.uv;
            material_map[material.material_id] = material;
        }

        std::vector<Entity> legacy_material_entities;
        for (auto entity : material_view) {
            legacy_material_entities.push_back(entity);
        }
        for (auto entity : legacy_material_entities) {
            world.DestroyEntity(entity);
        }
        for (const auto& pair : material_map) {
            Entity material_entity = world.CreateEntity();
            world.registry().emplace<MaterialInstanceComponent>(material_entity, pair.second);
        }
    }

    return true;
}

bool RunSceneRoundTripRegressionSample(const std::string& filepath) {
    Scene source("regression_source");
    Entity entity = source.GetWorld().CreateEntity();

    TransformComponent transform;
    transform.position = glm::vec3(3.25f, -1.5f, 0.0f);
    transform.scale = glm::vec3(2.0f, 1.5f, 1.0f);
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    source.GetWorld().registry().emplace<TransformComponent>(entity, transform);

    SpriteRendererComponent sprite;
    sprite.texture_handle = 123456;
    sprite.color = glm::vec4(0.2f, 0.4f, 0.6f, 0.8f);
    sprite.uv = glm::vec4(0.1f, 0.2f, 0.7f, 0.8f);
    sprite.material_instance_id = 445566;
    sprite.shader_variant = "SPRITE_TINT";
    sprite.blend_mode = SpriteBlendMode::Multiply;
    sprite.sorting_layer = 9;
    sprite.order_in_layer = 11;
    sprite.visible = true;
    source.GetWorld().registry().emplace<SpriteRendererComponent>(entity, sprite);

    Entity material_entity = source.GetWorld().CreateEntity();
    MaterialInstanceComponent material;
    material.material_id = 778899;
    material.name = "regression_material";
    material.shader_variant = "SPRITE_TINT";
    material.blend_mode = SpriteBlendMode::Additive;
    material.texture_handle = 345678;
    material.tint = glm::vec4(0.6f, 0.7f, 0.8f, 0.9f);
    material.uv_rect = glm::vec4(0.05f, 0.1f, 0.9f, 0.95f);
    source.GetWorld().registry().emplace<MaterialInstanceComponent>(material_entity, material);

    if (!source.Serialize(filepath)) {
        return false;
    }

    Scene loaded("regression_loaded");
    if (!loaded.Deserialize(filepath)) {
        return false;
    }

    auto view = loaded.GetWorld().registry().view<TransformComponent, SpriteRendererComponent>();
    if (view.size_hint() != 1) {
        return false;
    }

    auto loaded_entity = *view.begin();
    const auto& loaded_transform = view.get<TransformComponent>(loaded_entity);
    const auto& loaded_sprite = view.get<SpriteRendererComponent>(loaded_entity);

    auto near_equal = [](float a, float b) {
        return std::fabs(a - b) < 0.0001f;
    };

    bool transform_ok = near_equal(loaded_transform.position.x, transform.position.x) &&
                        near_equal(loaded_transform.position.y, transform.position.y) &&
                        near_equal(loaded_transform.position.z, transform.position.z) &&
                        near_equal(loaded_transform.scale.x, transform.scale.x) &&
                        near_equal(loaded_transform.scale.y, transform.scale.y) &&
                        near_equal(loaded_transform.scale.z, transform.scale.z);

    bool sprite_ok = loaded_sprite.texture_handle == sprite.texture_handle &&
                     near_equal(loaded_sprite.color.r, sprite.color.r) &&
                     near_equal(loaded_sprite.color.g, sprite.color.g) &&
                     near_equal(loaded_sprite.color.b, sprite.color.b) &&
                     near_equal(loaded_sprite.color.a, sprite.color.a) &&
                     near_equal(loaded_sprite.uv.x, sprite.uv.x) &&
                     near_equal(loaded_sprite.uv.y, sprite.uv.y) &&
                     near_equal(loaded_sprite.uv.z, sprite.uv.z) &&
                     near_equal(loaded_sprite.uv.w, sprite.uv.w) &&
                     loaded_sprite.material_instance_id == sprite.material_instance_id &&
                     loaded_sprite.shader_variant == sprite.shader_variant &&
                     loaded_sprite.blend_mode == sprite.blend_mode &&
                     loaded_sprite.sorting_layer == sprite.sorting_layer &&
                     loaded_sprite.order_in_layer == sprite.order_in_layer &&
                     loaded_sprite.visible == sprite.visible;

    auto material_loaded_view = loaded.GetWorld().registry().view<MaterialInstanceComponent>();
    size_t material_loaded_count = 0;
    for (auto it = material_loaded_view.begin(); it != material_loaded_view.end(); ++it) {
        ++material_loaded_count;
    }
    if (material_loaded_count != 1) {
        return false;
    }
    auto loaded_material_entity = *material_loaded_view.begin();
    const auto& loaded_material = material_loaded_view.get<MaterialInstanceComponent>(loaded_material_entity);
    bool material_ok = loaded_material.material_id == material.material_id &&
                       loaded_material.name == material.name &&
                       loaded_material.shader_variant == material.shader_variant &&
                       loaded_material.blend_mode == material.blend_mode &&
                       loaded_material.texture_handle == material.texture_handle &&
                       near_equal(loaded_material.tint.r, material.tint.r) &&
                       near_equal(loaded_material.tint.g, material.tint.g) &&
                       near_equal(loaded_material.tint.b, material.tint.b) &&
                       near_equal(loaded_material.tint.a, material.tint.a) &&
                       near_equal(loaded_material.uv_rect.x, material.uv_rect.x) &&
                       near_equal(loaded_material.uv_rect.y, material.uv_rect.y) &&
                       near_equal(loaded_material.uv_rect.z, material.uv_rect.z) &&
                       near_equal(loaded_material.uv_rect.w, material.uv_rect.w);

    return transform_ok && sprite_ok && material_ok;
}

bool RunSceneBackwardCompatibilityRegressionSample(const std::string& filepath) {
    const std::string legacy_json =
        "{\n"
        "  \"name\": \"legacy_scene\",\n"
        "  \"entities\": [\n"
        "    {\n"
        "      \"id\": 1,\n"
        "      \"components\": {\n"
        "        \"TransformComponent\": {\n"
        "          \"position\": [1.0, 2.0, 0.0],\n"
        "          \"rotation\": [0.0, 0.0, 0.0, 1.0],\n"
        "          \"scale\": [1.2, 1.3, 1.0]\n"
        "        },\n"
        "        \"SpriteRendererComponent\": {\n"
        "          \"texture_handle\": 999,\n"
        "          \"material_instance_id\": 12345,\n"
        "          \"shader_variant\": \"SPRITE_TINT\",\n"
        "          \"blend_mode\": 2,\n"
        "          \"color\": [0.4, 0.5, 0.6, 1.0],\n"
        "          \"uv\": [0.0, 0.0, 1.0, 1.0],\n"
        "          \"sorting_layer\": 2,\n"
        "          \"order_in_layer\": 3,\n"
        "          \"visible\": true\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n";

    std::ofstream out(filepath);
    if (!out.is_open()) {
        return false;
    }
    out << legacy_json;
    out.close();

    Scene loaded("legacy_loaded");
    if (!loaded.Deserialize(filepath)) {
        return false;
    }

    auto sprite_view = loaded.GetWorld().registry().view<TransformComponent, SpriteRendererComponent>();
    size_t sprite_count = 0;
    for (auto it = sprite_view.begin(); it != sprite_view.end(); ++it) {
        ++sprite_count;
    }
    if (sprite_count != 1) {
        return false;
    }
    auto entity = *sprite_view.begin();
    const auto& transform = sprite_view.get<TransformComponent>(entity);
    const auto& sprite = sprite_view.get<SpriteRendererComponent>(entity);
    auto material_view = loaded.GetWorld().registry().view<MaterialInstanceComponent>();

    auto near_equal = [](float a, float b) {
        return std::fabs(a - b) < 0.0001f;
    };

    bool transform_ok = near_equal(transform.position.x, 1.0f) &&
                        near_equal(transform.position.y, 2.0f) &&
                        near_equal(transform.scale.x, 1.2f) &&
                        near_equal(transform.scale.y, 1.3f);
    bool sprite_ok = sprite.texture_handle == 999 &&
                     near_equal(sprite.color.r, 0.4f) &&
                     near_equal(sprite.color.g, 0.5f) &&
                     near_equal(sprite.color.b, 0.6f) &&
                     near_equal(sprite.color.a, 1.0f) &&
                     sprite.sorting_layer == 2 &&
                     sprite.order_in_layer == 3 &&
                     sprite.visible &&
                     sprite.material_instance_id == 12345 &&
                     sprite.shader_variant == "SPRITE_TINT" &&
                     sprite.blend_mode == SpriteBlendMode::Multiply;
    size_t material_count = 0;
    for (auto it = material_view.begin(); it != material_view.end(); ++it) {
        ++material_count;
    }
    bool materials_ok = material_count == 1;
    if (materials_ok) {
        auto material_entity = *material_view.begin();
        const auto& material = material_view.get<MaterialInstanceComponent>(material_entity);
        materials_ok = material.material_id == 12345 &&
                       material.shader_variant == "SPRITE_TINT" &&
                       material.blend_mode == SpriteBlendMode::Multiply &&
                       material.texture_handle == 999 &&
                       near_equal(material.tint.r, 0.4f) &&
                       near_equal(material.tint.g, 0.5f) &&
                       near_equal(material.tint.b, 0.6f) &&
                       near_equal(material.tint.a, 1.0f);
    }

    return transform_ok && sprite_ok && materials_ok;
}

bool SaveEntityAsPrefab(World& world, Entity entity, const std::string& filepath) {
    if (!world.IsAlive(entity) || !world.registry().all_of<TransformComponent>(entity)) {
        return false;
    }
    auto parent_view = world.registry().view<ParentComponent>();
    std::vector<Entity> stack;
    std::vector<Entity> ordered_entities;
    std::unordered_set<uint32_t> visited;
    stack.push_back(entity);
    while (!stack.empty()) {
        Entity current = stack.back();
        stack.pop_back();
        uint32_t current_id = static_cast<uint32_t>(current);
        if (!visited.emplace(current_id).second) {
            continue;
        }
        ordered_entities.push_back(current);
        for (auto child : parent_view) {
            const auto& parent = parent_view.get<ParentComponent>(child);
            if (parent.parent == current) {
                stack.push_back(child);
            }
        }
    }

    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    doc.AddMember("type", "prefab", allocator);
    doc.AddMember("prefab_schema_version", kCurrentPrefabSchemaVersion, allocator);
    doc.AddMember("root_id", static_cast<uint32_t>(entity), allocator);
    rapidjson::Value entities_json(rapidjson::kArrayType);
    for (Entity current : ordered_entities) {
        rapidjson::Value entity_json(rapidjson::kObjectType);
        entity_json.AddMember("id", static_cast<uint32_t>(current), allocator);
        if (world.registry().all_of<ParentComponent>(current)) {
            const auto& parent = world.registry().get<ParentComponent>(current);
            uint32_t parent_id = static_cast<uint32_t>(parent.parent);
            if (visited.find(parent_id) != visited.end()) {
                entity_json.AddMember("parent_id", parent_id, allocator);
            }
        }

        rapidjson::Value components(rapidjson::kObjectType);
        const auto& transform = world.registry().get<TransformComponent>(current);
        rapidjson::Value transform_json(rapidjson::kObjectType);
        rapidjson::Value position(rapidjson::kArrayType);
        position.PushBack(transform.position.x, allocator).PushBack(transform.position.y, allocator).PushBack(transform.position.z, allocator);
        transform_json.AddMember("position", position, allocator);
        rapidjson::Value rotation(rapidjson::kArrayType);
        rotation.PushBack(transform.rotation.x, allocator).PushBack(transform.rotation.y, allocator).PushBack(transform.rotation.z, allocator).PushBack(transform.rotation.w, allocator);
        transform_json.AddMember("rotation", rotation, allocator);
        rapidjson::Value scale(rapidjson::kArrayType);
        scale.PushBack(transform.scale.x, allocator).PushBack(transform.scale.y, allocator).PushBack(transform.scale.z, allocator);
        transform_json.AddMember("scale", scale, allocator);
        components.AddMember("TransformComponent", transform_json, allocator);

        if (world.registry().all_of<SpriteRendererComponent>(current)) {
            const auto& sprite = world.registry().get<SpriteRendererComponent>(current);
            rapidjson::Value sprite_json(rapidjson::kObjectType);
            sprite_json.AddMember("texture_handle", sprite.texture_handle, allocator);
            sprite_json.AddMember("material_instance_id", sprite.material_instance_id, allocator);
            sprite_json.AddMember("shader_variant", rapidjson::Value(sprite.shader_variant.c_str(), allocator), allocator);
            sprite_json.AddMember("blend_mode", static_cast<int>(sprite.blend_mode), allocator);
            rapidjson::Value color(rapidjson::kArrayType);
            color.PushBack(sprite.color.r, allocator).PushBack(sprite.color.g, allocator).PushBack(sprite.color.b, allocator).PushBack(sprite.color.a, allocator);
            sprite_json.AddMember("color", color, allocator);
            rapidjson::Value uv(rapidjson::kArrayType);
            uv.PushBack(sprite.uv.x, allocator).PushBack(sprite.uv.y, allocator).PushBack(sprite.uv.z, allocator).PushBack(sprite.uv.w, allocator);
            sprite_json.AddMember("uv", uv, allocator);
            sprite_json.AddMember("sorting_layer", sprite.sorting_layer, allocator);
            sprite_json.AddMember("order_in_layer", sprite.order_in_layer, allocator);
            sprite_json.AddMember("visible", sprite.visible, allocator);
            components.AddMember("SpriteRendererComponent", sprite_json, allocator);
        }

        if (world.registry().all_of<ScriptComponent>(current)) {
            const auto& script = world.registry().get<ScriptComponent>(current);
            rapidjson::Value script_json(rapidjson::kObjectType);
            script_json.AddMember("script_path", rapidjson::Value(script.script_path.c_str(), allocator), allocator);
            script_json.AddMember("enabled", script.enabled, allocator);
            components.AddMember("ScriptComponent", script_json, allocator);
        }

        if (world.registry().all_of<SpineRendererComponent>(current)) {
            const auto& spine = world.registry().get<SpineRendererComponent>(current);
            rapidjson::Value spine_json(rapidjson::kObjectType);
            spine_json.AddMember("skeleton_data_path", rapidjson::Value(spine.skeleton_data_path.c_str(), allocator), allocator);
            spine_json.AddMember("atlas_path", rapidjson::Value(spine.atlas_path.c_str(), allocator), allocator);
            spine_json.AddMember("sorting_layer", spine.sorting_layer, allocator);
            spine_json.AddMember("order_in_layer", spine.order_in_layer, allocator);
            spine_json.AddMember("visible", spine.visible, allocator);
            spine_json.AddMember("time_scale", spine.time_scale, allocator);
            spine_json.AddMember("current_animation", rapidjson::Value(spine.current_animation.c_str(), allocator), allocator);
            spine_json.AddMember("loop", spine.loop, allocator);
            components.AddMember("SpineRendererComponent", spine_json, allocator);
        }

        entity_json.AddMember("components", components, allocator);
        entities_json.PushBack(entity_json, allocator);
    }
    doc.AddMember("entities", entities_json, allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::ofstream out(filepath);
    if (!out.is_open()) {
        return false;
    }
    out << buffer.GetString();
    out.close();
    return true;
}

Entity InstantiatePrefab(World& world, const std::string& filepath, const PrefabInstantiateOptions& options) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        return entt::null;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    rapidjson::Document doc;
    if (doc.Parse(buffer.str().c_str()).HasParseError() || !doc.IsObject()) {
        return entt::null;
    }
    auto parse_transform = [](const rapidjson::Value& components, TransformComponent& transform) {
        if (!components.HasMember("TransformComponent") || !components["TransformComponent"].IsObject()) {
            return;
        }
        const auto& transform_json = components["TransformComponent"];
        if (transform_json.HasMember("position") && transform_json["position"].IsArray() && transform_json["position"].Size() == 3) {
            const auto& p = transform_json["position"].GetArray();
            transform.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
        }
        if (transform_json.HasMember("rotation") && transform_json["rotation"].IsArray() && transform_json["rotation"].Size() == 4) {
            const auto& r = transform_json["rotation"].GetArray();
            transform.rotation = glm::quat(r[3].GetFloat(), r[0].GetFloat(), r[1].GetFloat(), r[2].GetFloat());
        }
        if (transform_json.HasMember("scale") && transform_json["scale"].IsArray() && transform_json["scale"].Size() == 3) {
            const auto& s = transform_json["scale"].GetArray();
            transform.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
        }
    };
    auto parse_sprite = [](const rapidjson::Value& components, SpriteRendererComponent& sprite) {
        if (!components.HasMember("SpriteRendererComponent") || !components["SpriteRendererComponent"].IsObject()) {
            return false;
        }
        const auto& sprite_json = components["SpriteRendererComponent"];
        if (sprite_json.HasMember("texture_handle") && sprite_json["texture_handle"].IsUint()) {
            sprite.texture_handle = sprite_json["texture_handle"].GetUint();
        }
        if (sprite_json.HasMember("material_instance_id") && sprite_json["material_instance_id"].IsUint()) {
            sprite.material_instance_id = sprite_json["material_instance_id"].GetUint();
        }
        if (sprite_json.HasMember("shader_variant") && sprite_json["shader_variant"].IsString()) {
            sprite.shader_variant = sprite_json["shader_variant"].GetString();
        }
        if (sprite_json.HasMember("blend_mode") && sprite_json["blend_mode"].IsInt()) {
            sprite.blend_mode = static_cast<SpriteBlendMode>(sprite_json["blend_mode"].GetInt());
        }
        if (sprite_json.HasMember("color") && sprite_json["color"].IsArray() && sprite_json["color"].Size() == 4) {
            const auto& c = sprite_json["color"].GetArray();
            sprite.color = glm::vec4(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat(), c[3].GetFloat());
        }
        if (sprite_json.HasMember("uv") && sprite_json["uv"].IsArray() && sprite_json["uv"].Size() == 4) {
            const auto& uv = sprite_json["uv"].GetArray();
            sprite.uv = glm::vec4(uv[0].GetFloat(), uv[1].GetFloat(), uv[2].GetFloat(), uv[3].GetFloat());
        }
        if (sprite_json.HasMember("sorting_layer") && sprite_json["sorting_layer"].IsInt()) {
            sprite.sorting_layer = sprite_json["sorting_layer"].GetInt();
        }
        if (sprite_json.HasMember("order_in_layer") && sprite_json["order_in_layer"].IsInt()) {
            sprite.order_in_layer = sprite_json["order_in_layer"].GetInt();
        }
        if (sprite_json.HasMember("visible") && sprite_json["visible"].IsBool()) {
            sprite.visible = sprite_json["visible"].GetBool();
        }
        return true;
    };
    auto parse_script = [](const rapidjson::Value& components, ScriptComponent& script) {
        if (!components.HasMember("ScriptComponent") || !components["ScriptComponent"].IsObject()) {
            return false;
        }
        const auto& script_json = components["ScriptComponent"];
        if (script_json.HasMember("script_path") && script_json["script_path"].IsString()) {
            script.script_path = script_json["script_path"].GetString();
        }
        if (script_json.HasMember("enabled") && script_json["enabled"].IsBool()) {
            script.enabled = script_json["enabled"].GetBool();
        }
        return true;
    };
    auto parse_spine = [](const rapidjson::Value& components, SpineRendererComponent& spine) {
        if (!components.HasMember("SpineRendererComponent") || !components["SpineRendererComponent"].IsObject()) {
            return false;
        }
        const auto& spine_json = components["SpineRendererComponent"];
        if (spine_json.HasMember("skeleton_data_path") && spine_json["skeleton_data_path"].IsString()) {
            spine.skeleton_data_path = spine_json["skeleton_data_path"].GetString();
        }
        if (spine_json.HasMember("atlas_path") && spine_json["atlas_path"].IsString()) {
            spine.atlas_path = spine_json["atlas_path"].GetString();
        }
        if (spine_json.HasMember("sorting_layer") && spine_json["sorting_layer"].IsInt()) {
            spine.sorting_layer = spine_json["sorting_layer"].GetInt();
        }
        if (spine_json.HasMember("order_in_layer") && spine_json["order_in_layer"].IsInt()) {
            spine.order_in_layer = spine_json["order_in_layer"].GetInt();
        }
        if (spine_json.HasMember("visible") && spine_json["visible"].IsBool()) {
            spine.visible = spine_json["visible"].GetBool();
        }
        if (spine_json.HasMember("time_scale") && spine_json["time_scale"].IsNumber()) {
            spine.time_scale = spine_json["time_scale"].GetFloat();
        }
        if (spine_json.HasMember("current_animation") && spine_json["current_animation"].IsString()) {
            spine.current_animation = spine_json["current_animation"].GetString();
            spine.dirty_animation = !spine.current_animation.empty();
        }
        if (spine_json.HasMember("loop") && spine_json["loop"].IsBool()) {
            spine.loop = spine_json["loop"].GetBool();
        }
        return true;
    };

    if (doc.HasMember("entities") && doc["entities"].IsArray() && doc["entities"].Size() > 0) {
        const auto& entities = doc["entities"].GetArray();
        std::unordered_map<uint32_t, Entity> id_map;
        std::vector<std::pair<Entity, uint32_t>> pending_parents;
        for (const auto& entity_json : entities) {
            if (!entity_json.IsObject() || !entity_json.HasMember("id") || !entity_json["id"].IsUint()) {
                continue;
            }
            id_map[entity_json["id"].GetUint()] = world.CreateEntity();
        }
        if (id_map.empty()) {
            return entt::null;
        }

        uint32_t root_id = entities[0]["id"].GetUint();
        if (doc.HasMember("root_id") && doc["root_id"].IsUint()) {
            root_id = doc["root_id"].GetUint();
        }

        for (const auto& entity_json : entities) {
            if (!entity_json.IsObject() || !entity_json.HasMember("id") || !entity_json["id"].IsUint()) {
                continue;
            }
            auto id_it = id_map.find(entity_json["id"].GetUint());
            if (id_it == id_map.end()) {
                continue;
            }
            Entity instance = id_it->second;
            TransformComponent transform;
            if (entity_json.HasMember("components") && entity_json["components"].IsObject()) {
                const auto& components = entity_json["components"];
                parse_transform(components, transform);
                SpriteRendererComponent sprite;
                if (parse_sprite(components, sprite)) {
                    world.registry().emplace<SpriteRendererComponent>(instance, sprite);
                }
                ScriptComponent script;
                if (parse_script(components, script)) {
                    world.registry().emplace<ScriptComponent>(instance, script);
                }
                SpineRendererComponent spine;
                if (parse_spine(components, spine)) {
                    world.registry().emplace<SpineRendererComponent>(instance, spine);
                }
            }
            transform.dirty = true;
            world.registry().emplace<TransformComponent>(instance, transform);
            if (entity_json.HasMember("parent_id") && entity_json["parent_id"].IsUint()) {
                pending_parents.emplace_back(instance, entity_json["parent_id"].GetUint());
            }
        }

        for (const auto& pending : pending_parents) {
            auto it = id_map.find(pending.second);
            if (it != id_map.end()) {
                world.registry().emplace_or_replace<ParentComponent>(pending.first, ParentComponent{it->second});
            }
        }

        auto root_it = id_map.find(root_id);
        Entity root_entity = root_it != id_map.end() ? root_it->second : id_map.begin()->second;
        if (world.registry().all_of<TransformComponent>(root_entity)) {
            auto& root_transform = world.registry().get<TransformComponent>(root_entity);
            if (options.override_position) {
                root_transform.position = options.position;
            }
            if (options.override_rotation) {
                root_transform.rotation = options.rotation;
            }
            if (options.override_scale) {
                root_transform.scale = options.scale;
            }
            root_transform.dirty = true;
        }
        return root_entity;
    }

    if (!doc.HasMember("components") || !doc["components"].IsObject()) {
        return entt::null;
    }
    const auto& components = doc["components"];
    Entity entity = world.CreateEntity();
    TransformComponent transform;
    parse_transform(components, transform);
    transform.dirty = true;
    if (options.override_position) {
        transform.position = options.position;
    }
    if (options.override_rotation) {
        transform.rotation = options.rotation;
    }
    if (options.override_scale) {
        transform.scale = options.scale;
    }
    world.registry().emplace<TransformComponent>(entity, transform);
    SpriteRendererComponent sprite;
    if (parse_sprite(components, sprite)) {
        world.registry().emplace<SpriteRendererComponent>(entity, sprite);
    }
    ScriptComponent script;
    if (parse_script(components, script)) {
        world.registry().emplace<ScriptComponent>(entity, script);
    }
    SpineRendererComponent spine;
    if (parse_spine(components, spine)) {
        world.registry().emplace<SpineRendererComponent>(entity, spine);
    }
    return entity;
}

Entity InstantiatePrefab(World& world, const std::string& filepath) {
    return InstantiatePrefab(world, filepath, PrefabInstantiateOptions{});
}

} // namespace scene

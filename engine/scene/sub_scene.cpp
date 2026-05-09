/**
 * @file sub_scene.cpp
 * @brief 子场景实现 —— Load/Unload 生命周期管理
 */

#include "engine/scene/sub_scene.h"
#include "engine/scene/scene.h"
#include "engine/base/debug.h"
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include "engine/ecs/transform.h"
#include "engine/ecs/sprite.h"
#include "engine/ecs/script.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/uuid_component.h"

namespace scene {

SubScene::SubScene(const std::string& path) : path_(path) {
}

SubScene::~SubScene() {
    // 注意：析构时不自动 Unload，需要由使用者显式调用 Unload(world)
    // 因为 SubScene 不持有 World 指针
}

SubScene::SubScene(SubScene&& other) noexcept
    : path_(std::move(other.path_)),
      entities_(std::move(other.entities_)),
      state_(other.state_) {
    other.state_ = SubSceneState::Unloaded;
}

SubScene& SubScene::operator=(SubScene&& other) noexcept {
    if (this != &other) {
        path_ = std::move(other.path_);
        entities_ = std::move(other.entities_);
        state_ = other.state_;
        other.state_ = SubSceneState::Unloaded;
    }
    return *this;
}

bool SubScene::Load(World& world, AssetManager& asset_manager, const std::string& path) {
    if (state_ == SubSceneState::Loaded) {
        DEBUG_LOG_WARN("SubScene::Load: already loaded, path={}", path_);
        return false;
    }

    path_ = path;
    state_ = SubSceneState::Loading;

    std::ifstream in(path);
    if (!in.is_open()) {
        DEBUG_LOG_ERROR("SubScene::Load: failed to open file {}", path);
        state_ = SubSceneState::Unloaded;
        return false;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string json_str = buffer.str();

    rapidjson::Document doc;
    if (doc.Parse(json_str.c_str()).HasParseError() || !doc.IsObject()) {
        DEBUG_LOG_ERROR("SubScene::Load: JSON parse failed for {}", path);
        state_ = SubSceneState::Unloaded;
        return false;
    }

    if (!doc.HasMember("entities") || !doc["entities"].IsArray()) {
        // 空场景也算加载成功
        state_ = SubSceneState::Loaded;
        return true;
    }

    // 反序列化实体，记录到 entities_ 列表
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
        entities_.push_back(entity);

        if (entity_json.HasMember("id") && entity_json["id"].IsUint()) {
            entity_id_map[entity_json["id"].GetUint()] = entity;
        }
        const auto& components = entity_json["components"];

        // TransformComponent
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

        // SpriteRendererComponent
        if (components.HasMember("SpriteRendererComponent") && components["SpriteRendererComponent"].IsObject()) {
            const auto& sprite_json = components["SpriteRendererComponent"];
            SpriteRendererComponent sprite;
            if (sprite_json.HasMember("texture_handle") && sprite_json["texture_handle"].IsUint()) {
                sprite.texture_handle = sprite_json["texture_handle"].GetUint();
            }
            if (sprite_json.HasMember("color") && sprite_json["color"].IsArray() && sprite_json["color"].Size() == 4) {
                const auto& c = sprite_json["color"].GetArray();
                sprite.color = glm::vec4(c[0].GetFloat(), c[1].GetFloat(), c[2].GetFloat(), c[3].GetFloat());
            }
            if (sprite_json.HasMember("visible") && sprite_json["visible"].IsBool()) {
                sprite.visible = sprite_json["visible"].GetBool();
            }
            world.registry().emplace<SpriteRendererComponent>(entity, sprite);
        }

        // ParentComponent
        if (components.HasMember("ParentComponent") && components["ParentComponent"].IsObject()) {
            const auto& parent_json = components["ParentComponent"];
            if (parent_json.HasMember("parent") && parent_json["parent"].IsUint()) {
                pending_parents.emplace_back(entity, parent_json["parent"].GetUint());
            }
        }

        // ScriptComponent
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

        // UUIDComponent
        if (components.HasMember("UUIDComponent") && components["UUIDComponent"].IsObject()) {
            const auto& uuid_json = components["UUIDComponent"];
            UUIDComponent uuid_comp;
            if (uuid_json.HasMember("uuid") && uuid_json["uuid"].IsString()) {
                uuid_comp.uuid = UUIDComponent::FromString(uuid_json["uuid"].GetString());
            } else if (uuid_json.HasMember("uuid") && uuid_json["uuid"].IsUint64()) {
                uuid_comp.uuid = uuid_json["uuid"].GetUint64();
            }
            world.registry().emplace<UUIDComponent>(entity, uuid_comp);
        }

        // MeshRendererComponent (full property support, matching Scene::Deserialize)
        if (components.HasMember("MeshRendererComponent") && components["MeshRendererComponent"].IsObject()) {
            const auto& mesh_json = components["MeshRendererComponent"];
            dse::MeshRendererComponent mesh;
            if (mesh_json.HasMember("mesh_path") && mesh_json["mesh_path"].IsString()) {
                mesh.mesh_path = mesh_json["mesh_path"].GetString();
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
            if (mesh_json.HasMember("material_alpha_cutoff") && mesh_json["material_alpha_cutoff"].IsNumber()) {
                mesh.material_alpha_cutoff = mesh_json["material_alpha_cutoff"].GetFloat();
            }
            if (mesh_json.HasMember("material_alpha_test") && mesh_json["material_alpha_test"].IsBool()) {
                mesh.material_alpha_test = mesh_json["material_alpha_test"].GetBool();
            }
            if (mesh_json.HasMember("material_double_sided") && mesh_json["material_double_sided"].IsBool()) {
                mesh.material_double_sided = mesh_json["material_double_sided"].GetBool();
            }
            if (mesh_json.HasMember("receive_shadow") && mesh_json["receive_shadow"].IsBool()) {
                mesh.receive_shadow = mesh_json["receive_shadow"].GetBool();
            }
            if (mesh_json.HasMember("visible") && mesh_json["visible"].IsBool()) {
                mesh.visible = mesh_json["visible"].GetBool();
            }
            if (mesh_json.HasMember("sorting_layer") && mesh_json["sorting_layer"].IsInt()) {
                mesh.sorting_layer = mesh_json["sorting_layer"].GetInt();
            }
            if (mesh_json.HasMember("order_in_layer") && mesh_json["order_in_layer"].IsInt()) {
                mesh.order_in_layer = mesh_json["order_in_layer"].GetInt();
            }
            world.registry().emplace<dse::MeshRendererComponent>(entity, mesh);
        }

        // Camera3DComponent
        if (components.HasMember("Camera3DComponent") && components["Camera3DComponent"].IsObject()) {
            const auto& camera_json = components["Camera3DComponent"];
            dse::Camera3DComponent camera;
            if (camera_json.HasMember("enabled") && camera_json["enabled"].IsBool()) {
                camera.enabled = camera_json["enabled"].GetBool();
            }
            if (camera_json.HasMember("fov") && camera_json["fov"].IsNumber()) {
                camera.fov = camera_json["fov"].GetFloat();
            }
            if (camera_json.HasMember("near_clip") && camera_json["near_clip"].IsNumber()) {
                camera.near_clip = camera_json["near_clip"].GetFloat();
            }
            if (camera_json.HasMember("far_clip") && camera_json["far_clip"].IsNumber()) {
                camera.far_clip = camera_json["far_clip"].GetFloat();
            }
            world.registry().emplace<dse::Camera3DComponent>(entity, camera);
        }

        // DirectionalLight3DComponent
        if (components.HasMember("DirectionalLight3DComponent") && components["DirectionalLight3DComponent"].IsObject()) {
            const auto& light_json = components["DirectionalLight3DComponent"];
            dse::DirectionalLight3DComponent light;
            if (light_json.HasMember("enabled") && light_json["enabled"].IsBool()) {
                light.enabled = light_json["enabled"].GetBool();
            }
            if (light_json.HasMember("intensity") && light_json["intensity"].IsNumber()) {
                light.intensity = light_json["intensity"].GetFloat();
            }
            world.registry().emplace<dse::DirectionalLight3DComponent>(entity, light);
        }
    }

    // 解析父子关系
    for (const auto& pair : pending_parents) {
        auto it = entity_id_map.find(pair.second);
        ParentComponent parent_component;
        parent_component.parent = it != entity_id_map.end() ? it->second : entt::null;
        world.registry().emplace_or_replace<ParentComponent>(pair.first, parent_component);
        if (world.registry().all_of<TransformComponent>(pair.first)) {
            world.registry().get<TransformComponent>(pair.first).dirty = true;
        }
    }

    state_ = SubSceneState::Loaded;
    DEBUG_LOG_INFO("SubScene::Load: loaded {} entities from {}", entities_.size(), path);
    return true;
}

void SubScene::Unload(World& world) {
    if (state_ != SubSceneState::Loaded) {
        return;
    }
    for (auto entity : entities_) {
        if (world.IsAlive(entity)) {
            world.DestroyEntity(entity);
        }
    }
    DEBUG_LOG_INFO("SubScene::Unload: unloaded {} entities from {}", entities_.size(), path_);
    entities_.clear();
    state_ = SubSceneState::Unloaded;
}

} // namespace scene

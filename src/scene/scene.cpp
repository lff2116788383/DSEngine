#include "scene/scene.h"
#include "phase1/ecs/components_2d.h"
#include "utils/debug.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <unordered_map>
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

void Scene::BindWorld(Phase1World* world) {
    if (world) {
        world_ = world;
    }
}

void Scene::UnbindWorld() {
    world_ = &owned_world_;
}

Phase1World& Scene::ActiveWorld() {
    return world_ ? *world_ : owned_world_;
}

const Phase1World& Scene::ActiveWorld() const {
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

bool SaveEntityAsPrefab(Phase1World& world, Entity entity, const std::string& filepath) {
    if (!world.IsAlive(entity) || !world.registry().all_of<TransformComponent>(entity)) {
        return false;
    }
    rapidjson::Document doc;
    doc.SetObject();
    auto& allocator = doc.GetAllocator();
    doc.AddMember("type", "prefab", allocator);
    doc.AddMember("prefab_schema_version", kCurrentPrefabSchemaVersion, allocator);
    rapidjson::Value components(rapidjson::kObjectType);
    const auto& transform = world.registry().get<TransformComponent>(entity);
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
    if (world.registry().all_of<SpriteRendererComponent>(entity)) {
        const auto& sprite = world.registry().get<SpriteRendererComponent>(entity);
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
    doc.AddMember("components", components, allocator);
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

Entity InstantiatePrefab(Phase1World& world, const std::string& filepath, const PrefabInstantiateOptions& options) {
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
    if (!doc.HasMember("components") || !doc["components"].IsObject()) {
        return entt::null;
    }
    const auto& components = doc["components"];
    Entity entity = world.CreateEntity();
    TransformComponent transform;
    if (components.HasMember("TransformComponent") && components["TransformComponent"].IsObject()) {
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
    }
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
    return entity;
}

Entity InstantiatePrefab(Phase1World& world, const std::string& filepath) {
    return InstantiatePrefab(world, filepath, PrefabInstantiateOptions{});
}

} // namespace scene

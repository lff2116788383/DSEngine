/**
 * @file scene_component_serialization.cpp
 * @brief 扩展 ECS 组件的场景 JSON 序列化/反序列化（大世界、NavMesh、后处理等）
 */

#include "engine/scene/scene_component_serialization.h"

#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_foliage.h"
#include "engine/ecs/components_3d_navmesh.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/reflect/component_reflection.h"
#include "engine/reflect/reflect.h"
#include "engine/reflect/reflect_json.h"

#include <glm/glm.hpp>

namespace scene::component_io {
namespace {

rapidjson::Value MakeVec3Array(const glm::vec3& v, rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(v.x, allocator).PushBack(v.y, allocator).PushBack(v.z, allocator);
    return arr;
}

rapidjson::Value MakeVec4Array(const glm::vec4& v, rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(v.x, allocator).PushBack(v.y, allocator).PushBack(v.z, allocator).PushBack(v.w, allocator);
    return arr;
}

bool ReadVec3(const rapidjson::Value& json, glm::vec3& out) {
    if (!json.IsArray() || json.Size() != 3) {
        return false;
    }
    out = glm::vec3(json[0].GetFloat(), json[1].GetFloat(), json[2].GetFloat());
    return true;
}

bool ReadVec4(const rapidjson::Value& json, glm::vec4& out) {
    if (!json.IsArray() || json.Size() != 4) {
        return false;
    }
    out = glm::vec4(json[0].GetFloat(), json[1].GetFloat(), json[2].GetFloat(), json[3].GetFloat());
    return true;
}

void AddStringArray(rapidjson::Value& parent, const char* key,
                    const std::string* values, int count,
                    rapidjson::Document::AllocatorType& allocator) {
    rapidjson::Value arr(rapidjson::kArrayType);
    for (int i = 0; i < count; ++i) {
        arr.PushBack(rapidjson::Value(values[i].c_str(), allocator), allocator);
    }
    parent.AddMember(rapidjson::StringRef(key), arr, allocator);
}

void ReadStringArray(const rapidjson::Value& parent, const char* key,
                     std::string* values, int count) {
    if (!parent.HasMember(key) || !parent[key].IsArray()) {
        return;
    }
    const auto& arr = parent[key].GetArray();
    for (rapidjson::SizeType i = 0; i < arr.Size() && static_cast<int>(i) < count; ++i) {
        if (arr[i].IsString()) {
            values[i] = arr[i].GetString();
        }
    }
}

void SerializePostProcess(const dse::PostProcessComponent& pp,
                          rapidjson::Value& components,
                          rapidjson::Document::AllocatorType& allocator) {
    // Generic reflection driver: field set defined by PostProcessComponent reflection registration (single source of truth).
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::PostProcessComponent>();
    if (!ti) return;
    rapidjson::Value json(rapidjson::kObjectType);
    dse::reflect::SerializeReflected(*ti, &pp, json, allocator);
    components.AddMember("PostProcessComponent", json, allocator);
}

dse::PostProcessComponent DeserializePostProcess(const rapidjson::Value& json) {
    // Generic reflection driver: missing/mismatched fields keep defaults; equivalent to per-field reads.
    dse::PostProcessComponent pp;
    const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::PostProcessComponent>();
    if (ti) dse::reflect::DeserializeReflected(*ti, &pp, json);
    return pp;
}

} // namespace

void SerializeExtendedComponents(entt::registry& registry, Entity entity,
                                 rapidjson::Value& components,
                                 rapidjson::Document::AllocatorType& allocator) {
    dse::reflect::EnsureCoreReflectionRegistered();
    if (registry.all_of<dse::PostProcessComponent>(entity)) {
        SerializePostProcess(registry.get<dse::PostProcessComponent>(entity), components, allocator);
    }

    if (registry.all_of<dse::SubSceneComponent>(entity)) {
        const auto& sub = registry.get<dse::SubSceneComponent>(entity);
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::SubSceneComponent>();
        if (ti) {
            rapidjson::Value json(rapidjson::kObjectType);
            dse::reflect::SerializeReflected(*ti, &sub, json, allocator);
            components.AddMember("SubSceneComponent", json, allocator);
        }
    }

    if (registry.all_of<dse::FoliageComponent>(entity)) {
        const auto& foliage = registry.get<dse::FoliageComponent>(entity);
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::FoliageComponent>();
        if (ti) {
            rapidjson::Value json(rapidjson::kObjectType);
            dse::reflect::SerializeReflected(*ti, &foliage, json, allocator);
            components.AddMember("FoliageComponent", json, allocator);
        }
    }

    if (registry.all_of<dse::TreeComponent>(entity)) {
        const auto& tree = registry.get<dse::TreeComponent>(entity);
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::TreeComponent>();
        if (ti) {
            rapidjson::Value json(rapidjson::kObjectType);
            dse::reflect::SerializeReflected(*ti, &tree, json, allocator);
            components.AddMember("TreeComponent", json, allocator);
        }
    }

    if (registry.all_of<dse::TerrainTileManagerComponent>(entity)) {
        const auto& ttm = registry.get<dse::TerrainTileManagerComponent>(entity);
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::TerrainTileManagerComponent>();
        if (ti) {
            rapidjson::Value json(rapidjson::kObjectType);
            dse::reflect::SerializeReflected(*ti, &ttm, json, allocator);
            components.AddMember("TerrainTileManagerComponent", json, allocator);
        }
    }

    if (registry.all_of<dse::DynamicObstacleComponent>(entity)) {
        const auto& obstacle = registry.get<dse::DynamicObstacleComponent>(entity);
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::DynamicObstacleComponent>();
        if (ti) {
            rapidjson::Value json(rapidjson::kObjectType);
            dse::reflect::SerializeReflected(*ti, &obstacle, json, allocator);
            components.AddMember("DynamicObstacleComponent", json, allocator);
        }
    }

    if (registry.all_of<dse::NavMeshAutoRebakeComponent>(entity)) {
        const auto& nav = registry.get<dse::NavMeshAutoRebakeComponent>(entity);
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::NavMeshAutoRebakeComponent>();
        if (ti) {
            rapidjson::Value json(rapidjson::kObjectType);
            dse::reflect::SerializeReflected(*ti, &nav, json, allocator);
            components.AddMember("NavMeshAutoRebakeComponent", json, allocator);
        }
    }
}

void DeserializeExtendedComponents(entt::registry& registry, Entity entity,
                                   const rapidjson::Value& components) {
    dse::reflect::EnsureCoreReflectionRegistered();
    if (components.HasMember("PostProcessComponent") && components["PostProcessComponent"].IsObject()) {
        registry.emplace<dse::PostProcessComponent>(
            entity, DeserializePostProcess(components["PostProcessComponent"]));
    }

    if (components.HasMember("SubSceneComponent") && components["SubSceneComponent"].IsObject()) {
        dse::SubSceneComponent sub;
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::SubSceneComponent>();
        if (ti) dse::reflect::DeserializeReflected(*ti, &sub, components["SubSceneComponent"]);
        registry.emplace<dse::SubSceneComponent>(entity, std::move(sub));
    }

    if (components.HasMember("FoliageComponent") && components["FoliageComponent"].IsObject()) {
        dse::FoliageComponent foliage;
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::FoliageComponent>();
        if (ti) dse::reflect::DeserializeReflected(*ti, &foliage, components["FoliageComponent"]);
        registry.emplace<dse::FoliageComponent>(entity, foliage);
    }

    if (components.HasMember("TreeComponent") && components["TreeComponent"].IsObject()) {
        dse::TreeComponent tree;
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::TreeComponent>();
        if (ti) dse::reflect::DeserializeReflected(*ti, &tree, components["TreeComponent"]);
        registry.emplace<dse::TreeComponent>(entity, std::move(tree));
    }

    if (components.HasMember("TerrainTileManagerComponent") &&
        components["TerrainTileManagerComponent"].IsObject()) {
        dse::TerrainTileManagerComponent ttm;
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::TerrainTileManagerComponent>();
        if (ti) dse::reflect::DeserializeReflected(*ti, &ttm, components["TerrainTileManagerComponent"]);
        registry.emplace<dse::TerrainTileManagerComponent>(entity, std::move(ttm));
    }

    if (components.HasMember("DynamicObstacleComponent") &&
        components["DynamicObstacleComponent"].IsObject()) {
        dse::DynamicObstacleComponent obstacle;
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::DynamicObstacleComponent>();
        if (ti) dse::reflect::DeserializeReflected(*ti, &obstacle, components["DynamicObstacleComponent"]);
        obstacle.obstacle_ref_ = 0;
        obstacle.dirty_ = true;
        registry.emplace<dse::DynamicObstacleComponent>(entity, std::move(obstacle));
    }

    if (components.HasMember("NavMeshAutoRebakeComponent") &&
        components["NavMeshAutoRebakeComponent"].IsObject()) {
        dse::NavMeshAutoRebakeComponent nav;
        const dse::reflect::TypeInfo* ti = dse::reflect::Reflection::Find<dse::NavMeshAutoRebakeComponent>();
        if (ti) dse::reflect::DeserializeReflected(*ti, &nav, components["NavMeshAutoRebakeComponent"]);
        nav.cooldown_timer_ = 0.0f;
        nav.needs_full_rebake_ = true;
        nav.baked_tile_count_ = 0;
        registry.emplace<dse::NavMeshAutoRebakeComponent>(entity, std::move(nav));
    }
}

} // namespace scene::component_io

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
        rapidjson::Value json(rapidjson::kObjectType);
        json.AddMember("enabled", sub.enabled, allocator);
        json.AddMember("scene_path", rapidjson::Value(sub.scene_path.c_str(), allocator), allocator);
        components.AddMember("SubSceneComponent", json, allocator);
    }

    if (registry.all_of<dse::FoliageComponent>(entity)) {
        const auto& foliage = registry.get<dse::FoliageComponent>(entity);
        rapidjson::Value json(rapidjson::kObjectType);
        json.AddMember("enabled", foliage.enabled, allocator);
        json.AddMember("wind_strength", foliage.wind_strength, allocator);
        json.AddMember("stiffness", foliage.stiffness, allocator);
        json.AddMember("phase_offset", foliage.phase_offset, allocator);
        json.AddMember("push_response", foliage.push_response, allocator);
        components.AddMember("FoliageComponent", json, allocator);
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
        rapidjson::Value json(rapidjson::kObjectType);
        json.AddMember("enabled", ttm.enabled, allocator);
        json.AddMember("tile_world_size", ttm.tile_world_size, allocator);
        json.AddMember("tile_resolution", ttm.tile_resolution, allocator);
        json.AddMember("max_height", ttm.max_height, allocator);
        json.AddMember("max_lod_levels", ttm.max_lod_levels, allocator);
        json.AddMember("lod_distance_factor", ttm.lod_distance_factor, allocator);
        json.AddMember("load_radius", ttm.load_radius, allocator);
        json.AddMember("unload_radius", ttm.unload_radius, allocator);
        json.AddMember("heightmap_pattern", rapidjson::Value(ttm.heightmap_pattern.c_str(), allocator), allocator);
        AddStringArray(json, "splat_texture_paths", ttm.splat_texture_paths, 4, allocator);
        json.AddMember("splat_tiling", MakeVec4Array(ttm.splat_tiling, allocator), allocator);
        json.AddMember("base_texture_path", rapidjson::Value(ttm.base_texture_path.c_str(), allocator), allocator);
        json.AddMember("use_procedural", ttm.use_procedural, allocator);
        json.AddMember("procedural_base_height", ttm.procedural_base_height, allocator);
        components.AddMember("TerrainTileManagerComponent", json, allocator);
    }

    if (registry.all_of<dse::DynamicObstacleComponent>(entity)) {
        const auto& obstacle = registry.get<dse::DynamicObstacleComponent>(entity);
        rapidjson::Value json(rapidjson::kObjectType);
        json.AddMember("enabled", obstacle.enabled, allocator);
        json.AddMember("shape", static_cast<int>(obstacle.shape), allocator);
        json.AddMember("box_extents", MakeVec3Array(obstacle.box_extents, allocator), allocator);
        json.AddMember("cylinder_radius", obstacle.cylinder_radius, allocator);
        json.AddMember("cylinder_height", obstacle.cylinder_height, allocator);
        components.AddMember("DynamicObstacleComponent", json, allocator);
    }

    if (registry.all_of<dse::NavMeshAutoRebakeComponent>(entity)) {
        const auto& nav = registry.get<dse::NavMeshAutoRebakeComponent>(entity);
        rapidjson::Value json(rapidjson::kObjectType);
        json.AddMember("enabled", nav.enabled, allocator);
        json.AddMember("tile_size", nav.tile_size, allocator);
        json.AddMember("rebake_cooldown", nav.rebake_cooldown, allocator);
        json.AddMember("collect_terrain", nav.collect_terrain, allocator);
        json.AddMember("collect_mesh_renderers", nav.collect_mesh_renderers, allocator);
        json.AddMember("agent_height", nav.agent_height, allocator);
        json.AddMember("agent_radius", nav.agent_radius, allocator);
        json.AddMember("agent_max_climb", nav.agent_max_climb, allocator);
        json.AddMember("agent_max_slope", nav.agent_max_slope, allocator);
        json.AddMember("cell_size", nav.cell_size, allocator);
        json.AddMember("cell_height", nav.cell_height, allocator);
        components.AddMember("NavMeshAutoRebakeComponent", json, allocator);
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
        const auto& json = components["SubSceneComponent"];
        dse::SubSceneComponent sub;
        if (json.HasMember("enabled") && json["enabled"].IsBool()) {
            sub.enabled = json["enabled"].GetBool();
        }
        if (json.HasMember("scene_path") && json["scene_path"].IsString()) {
            sub.scene_path = json["scene_path"].GetString();
        }
        registry.emplace<dse::SubSceneComponent>(entity, std::move(sub));
    }

    if (components.HasMember("FoliageComponent") && components["FoliageComponent"].IsObject()) {
        const auto& json = components["FoliageComponent"];
        dse::FoliageComponent foliage;
        if (json.HasMember("enabled") && json["enabled"].IsBool()) {
            foliage.enabled = json["enabled"].GetBool();
        }
        if (json.HasMember("wind_strength") && json["wind_strength"].IsNumber()) {
            foliage.wind_strength = json["wind_strength"].GetFloat();
        }
        if (json.HasMember("stiffness") && json["stiffness"].IsNumber()) {
            foliage.stiffness = json["stiffness"].GetFloat();
        }
        if (json.HasMember("phase_offset") && json["phase_offset"].IsNumber()) {
            foliage.phase_offset = json["phase_offset"].GetFloat();
        }
        if (json.HasMember("push_response") && json["push_response"].IsNumber()) {
            foliage.push_response = json["push_response"].GetFloat();
        }
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
        const auto& json = components["TerrainTileManagerComponent"];
        dse::TerrainTileManagerComponent ttm;
        if (json.HasMember("enabled") && json["enabled"].IsBool()) {
            ttm.enabled = json["enabled"].GetBool();
        }
        if (json.HasMember("tile_world_size") && json["tile_world_size"].IsNumber()) {
            ttm.tile_world_size = json["tile_world_size"].GetFloat();
        }
        if (json.HasMember("tile_resolution") && json["tile_resolution"].IsInt()) {
            ttm.tile_resolution = json["tile_resolution"].GetInt();
        }
        if (json.HasMember("max_height") && json["max_height"].IsNumber()) {
            ttm.max_height = json["max_height"].GetFloat();
        }
        if (json.HasMember("max_lod_levels") && json["max_lod_levels"].IsInt()) {
            ttm.max_lod_levels = json["max_lod_levels"].GetInt();
        }
        if (json.HasMember("lod_distance_factor") && json["lod_distance_factor"].IsNumber()) {
            ttm.lod_distance_factor = json["lod_distance_factor"].GetFloat();
        }
        if (json.HasMember("load_radius") && json["load_radius"].IsNumber()) {
            ttm.load_radius = json["load_radius"].GetFloat();
        }
        if (json.HasMember("unload_radius") && json["unload_radius"].IsNumber()) {
            ttm.unload_radius = json["unload_radius"].GetFloat();
        }
        if (json.HasMember("heightmap_pattern") && json["heightmap_pattern"].IsString()) {
            ttm.heightmap_pattern = json["heightmap_pattern"].GetString();
        }
        ReadStringArray(json, "splat_texture_paths", ttm.splat_texture_paths, 4);
        if (json.HasMember("splat_tiling")) {
            ReadVec4(json["splat_tiling"], ttm.splat_tiling);
        }
        if (json.HasMember("base_texture_path") && json["base_texture_path"].IsString()) {
            ttm.base_texture_path = json["base_texture_path"].GetString();
        }
        if (json.HasMember("use_procedural") && json["use_procedural"].IsBool()) {
            ttm.use_procedural = json["use_procedural"].GetBool();
        }
        if (json.HasMember("procedural_base_height") && json["procedural_base_height"].IsNumber()) {
            ttm.procedural_base_height = json["procedural_base_height"].GetFloat();
        }
        registry.emplace<dse::TerrainTileManagerComponent>(entity, std::move(ttm));
    }

    if (components.HasMember("DynamicObstacleComponent") &&
        components["DynamicObstacleComponent"].IsObject()) {
        const auto& json = components["DynamicObstacleComponent"];
        dse::DynamicObstacleComponent obstacle;
        if (json.HasMember("enabled") && json["enabled"].IsBool()) {
            obstacle.enabled = json["enabled"].GetBool();
        }
        if (json.HasMember("shape") && json["shape"].IsInt()) {
            obstacle.shape = static_cast<dse::DynamicObstacleComponent::Shape>(json["shape"].GetInt());
        }
        if (json.HasMember("box_extents")) {
            ReadVec3(json["box_extents"], obstacle.box_extents);
        }
        if (json.HasMember("cylinder_radius") && json["cylinder_radius"].IsNumber()) {
            obstacle.cylinder_radius = json["cylinder_radius"].GetFloat();
        }
        if (json.HasMember("cylinder_height") && json["cylinder_height"].IsNumber()) {
            obstacle.cylinder_height = json["cylinder_height"].GetFloat();
        }
        obstacle.obstacle_ref_ = 0;
        obstacle.dirty_ = true;
        registry.emplace<dse::DynamicObstacleComponent>(entity, std::move(obstacle));
    }

    if (components.HasMember("NavMeshAutoRebakeComponent") &&
        components["NavMeshAutoRebakeComponent"].IsObject()) {
        const auto& json = components["NavMeshAutoRebakeComponent"];
        dse::NavMeshAutoRebakeComponent nav;
        if (json.HasMember("enabled") && json["enabled"].IsBool()) {
            nav.enabled = json["enabled"].GetBool();
        }
        if (json.HasMember("tile_size") && json["tile_size"].IsNumber()) {
            nav.tile_size = json["tile_size"].GetFloat();
        }
        if (json.HasMember("rebake_cooldown") && json["rebake_cooldown"].IsNumber()) {
            nav.rebake_cooldown = json["rebake_cooldown"].GetFloat();
        }
        if (json.HasMember("collect_terrain") && json["collect_terrain"].IsBool()) {
            nav.collect_terrain = json["collect_terrain"].GetBool();
        }
        if (json.HasMember("collect_mesh_renderers") && json["collect_mesh_renderers"].IsBool()) {
            nav.collect_mesh_renderers = json["collect_mesh_renderers"].GetBool();
        }
        if (json.HasMember("agent_height") && json["agent_height"].IsNumber()) {
            nav.agent_height = json["agent_height"].GetFloat();
        }
        if (json.HasMember("agent_radius") && json["agent_radius"].IsNumber()) {
            nav.agent_radius = json["agent_radius"].GetFloat();
        }
        if (json.HasMember("agent_max_climb") && json["agent_max_climb"].IsNumber()) {
            nav.agent_max_climb = json["agent_max_climb"].GetFloat();
        }
        if (json.HasMember("agent_max_slope") && json["agent_max_slope"].IsNumber()) {
            nav.agent_max_slope = json["agent_max_slope"].GetFloat();
        }
        if (json.HasMember("cell_size") && json["cell_size"].IsNumber()) {
            nav.cell_size = json["cell_size"].GetFloat();
        }
        if (json.HasMember("cell_height") && json["cell_height"].IsNumber()) {
            nav.cell_height = json["cell_height"].GetFloat();
        }
        nav.cooldown_timer_ = 0.0f;
        nav.needs_full_rebake_ = true;
        nav.baked_tile_count_ = 0;
        registry.emplace<dse::NavMeshAutoRebakeComponent>(entity, std::move(nav));
    }
}

} // namespace scene::component_io

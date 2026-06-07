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
    rapidjson::Value json(rapidjson::kObjectType);
    json.AddMember("enabled", pp.enabled, allocator);

    json.AddMember("bloom_enabled", pp.bloom_enabled, allocator);
    json.AddMember("bloom_threshold", pp.bloom_threshold, allocator);
    json.AddMember("bloom_intensity", pp.bloom_intensity, allocator);
    json.AddMember("bloom_knee", pp.bloom_knee, allocator);
    json.AddMember("bloom_mip_weight", pp.bloom_mip_weight, allocator);

    json.AddMember("color_grading_enabled", pp.color_grading_enabled, allocator);
    json.AddMember("exposure", pp.exposure, allocator);
    json.AddMember("gamma", pp.gamma, allocator);

    json.AddMember("ssao_enabled", pp.ssao_enabled, allocator);
    json.AddMember("ssao_radius", pp.ssao_radius, allocator);
    json.AddMember("ssao_bias", pp.ssao_bias, allocator);
    json.AddMember("ssao_sample_count", pp.ssao_sample_count, allocator);
    json.AddMember("ssao_power", pp.ssao_power, allocator);
    json.AddMember("ssao_intensity", pp.ssao_intensity, allocator);

    json.AddMember("auto_exposure_enabled", pp.auto_exposure_enabled, allocator);
    json.AddMember("exposure_min", pp.exposure_min, allocator);
    json.AddMember("exposure_max", pp.exposure_max, allocator);
    json.AddMember("adaptation_speed_up", pp.adaptation_speed_up, allocator);
    json.AddMember("adaptation_speed_down", pp.adaptation_speed_down, allocator);
    json.AddMember("exposure_compensation", pp.exposure_compensation, allocator);

    json.AddMember("color_lut_intensity", pp.color_lut_intensity, allocator);

    json.AddMember("vignette_enabled", pp.vignette_enabled, allocator);
    json.AddMember("vignette_intensity", pp.vignette_intensity, allocator);
    json.AddMember("vignette_radius", pp.vignette_radius, allocator);
    json.AddMember("vignette_softness", pp.vignette_softness, allocator);

    json.AddMember("film_grain_enabled", pp.film_grain_enabled, allocator);
    json.AddMember("film_grain_intensity", pp.film_grain_intensity, allocator);
    json.AddMember("film_grain_time_scale", pp.film_grain_time_scale, allocator);

    json.AddMember("fxaa_enabled", pp.fxaa_enabled, allocator);
    json.AddMember("taa_enabled", pp.taa_enabled, allocator);
    json.AddMember("taa_blend_factor", pp.taa_blend_factor, allocator);

    json.AddMember("contact_shadow_enabled", pp.contact_shadow_enabled, allocator);
    json.AddMember("contact_shadow_strength", pp.contact_shadow_strength, allocator);
    json.AddMember("contact_shadow_steps", pp.contact_shadow_steps, allocator);
    json.AddMember("contact_shadow_step_size", pp.contact_shadow_step_size, allocator);

    json.AddMember("dof_enabled", pp.dof_enabled, allocator);
    json.AddMember("dof_focus_distance", pp.dof_focus_distance, allocator);
    json.AddMember("dof_focus_range", pp.dof_focus_range, allocator);
    json.AddMember("dof_bokeh_radius", pp.dof_bokeh_radius, allocator);

    json.AddMember("motion_blur_enabled", pp.motion_blur_enabled, allocator);
    json.AddMember("motion_blur_intensity", pp.motion_blur_intensity, allocator);
    json.AddMember("motion_blur_samples", pp.motion_blur_samples, allocator);

    json.AddMember("ssr_enabled", pp.ssr_enabled, allocator);
    json.AddMember("ssr_max_distance", pp.ssr_max_distance, allocator);
    json.AddMember("ssr_thickness", pp.ssr_thickness, allocator);
    json.AddMember("ssr_step_size", pp.ssr_step_size, allocator);
    json.AddMember("ssr_max_steps", pp.ssr_max_steps, allocator);
    json.AddMember("ssr_fade_distance", pp.ssr_fade_distance, allocator);
    json.AddMember("ssr_max_roughness", pp.ssr_max_roughness, allocator);

    json.AddMember("outline_enabled", pp.outline_enabled, allocator);
    json.AddMember("outline_color", MakeVec3Array(pp.outline_color, allocator), allocator);
    json.AddMember("outline_thickness", pp.outline_thickness, allocator);
    json.AddMember("outline_depth_threshold", pp.outline_depth_threshold, allocator);
    json.AddMember("outline_normal_threshold", pp.outline_normal_threshold, allocator);

    json.AddMember("light_shaft_enabled", pp.light_shaft_enabled, allocator);
    json.AddMember("light_shaft_color", MakeVec3Array(pp.light_shaft_color, allocator), allocator);
    json.AddMember("light_shaft_density", pp.light_shaft_density, allocator);
    json.AddMember("light_shaft_weight", pp.light_shaft_weight, allocator);
    json.AddMember("light_shaft_decay", pp.light_shaft_decay, allocator);
    json.AddMember("light_shaft_exposure", pp.light_shaft_exposure, allocator);
    json.AddMember("light_shaft_intensity", pp.light_shaft_intensity, allocator);
    json.AddMember("light_shaft_samples", pp.light_shaft_samples, allocator);

    json.AddMember("fog_enabled", pp.fog_enabled, allocator);
    json.AddMember("fog_color", MakeVec3Array(pp.fog_color, allocator), allocator);
    json.AddMember("fog_density", pp.fog_density, allocator);
    json.AddMember("fog_height_falloff", pp.fog_height_falloff, allocator);
    json.AddMember("fog_height_offset", pp.fog_height_offset, allocator);
    json.AddMember("fog_start", pp.fog_start, allocator);
    json.AddMember("fog_end", pp.fog_end, allocator);
    json.AddMember("fog_steps", pp.fog_steps, allocator);
    json.AddMember("fog_sun_scatter", pp.fog_sun_scatter, allocator);

    components.AddMember("PostProcessComponent", json, allocator);
}

dse::PostProcessComponent DeserializePostProcess(const rapidjson::Value& json) {
    dse::PostProcessComponent pp;
    if (json.HasMember("enabled") && json["enabled"].IsBool()) {
        pp.enabled = json["enabled"].GetBool();
    }
    if (json.HasMember("bloom_enabled") && json["bloom_enabled"].IsBool()) {
        pp.bloom_enabled = json["bloom_enabled"].GetBool();
    }
    if (json.HasMember("bloom_threshold") && json["bloom_threshold"].IsNumber()) {
        pp.bloom_threshold = json["bloom_threshold"].GetFloat();
    }
    if (json.HasMember("bloom_intensity") && json["bloom_intensity"].IsNumber()) {
        pp.bloom_intensity = json["bloom_intensity"].GetFloat();
    }
    if (json.HasMember("bloom_knee") && json["bloom_knee"].IsNumber()) {
        pp.bloom_knee = json["bloom_knee"].GetFloat();
    }
    if (json.HasMember("bloom_mip_weight") && json["bloom_mip_weight"].IsNumber()) {
        pp.bloom_mip_weight = json["bloom_mip_weight"].GetFloat();
    }
    if (json.HasMember("color_grading_enabled") && json["color_grading_enabled"].IsBool()) {
        pp.color_grading_enabled = json["color_grading_enabled"].GetBool();
    }
    if (json.HasMember("exposure") && json["exposure"].IsNumber()) {
        pp.exposure = json["exposure"].GetFloat();
    }
    if (json.HasMember("gamma") && json["gamma"].IsNumber()) {
        pp.gamma = json["gamma"].GetFloat();
    }
    if (json.HasMember("ssao_enabled") && json["ssao_enabled"].IsBool()) {
        pp.ssao_enabled = json["ssao_enabled"].GetBool();
    }
    if (json.HasMember("ssao_radius") && json["ssao_radius"].IsNumber()) {
        pp.ssao_radius = json["ssao_radius"].GetFloat();
    }
    if (json.HasMember("ssao_bias") && json["ssao_bias"].IsNumber()) {
        pp.ssao_bias = json["ssao_bias"].GetFloat();
    }
    if (json.HasMember("ssao_sample_count") && json["ssao_sample_count"].IsInt()) {
        pp.ssao_sample_count = json["ssao_sample_count"].GetInt();
    }
    if (json.HasMember("ssao_power") && json["ssao_power"].IsNumber()) {
        pp.ssao_power = json["ssao_power"].GetFloat();
    }
    if (json.HasMember("ssao_intensity") && json["ssao_intensity"].IsNumber()) {
        pp.ssao_intensity = json["ssao_intensity"].GetFloat();
    }
    if (json.HasMember("auto_exposure_enabled") && json["auto_exposure_enabled"].IsBool()) {
        pp.auto_exposure_enabled = json["auto_exposure_enabled"].GetBool();
    }
    if (json.HasMember("exposure_min") && json["exposure_min"].IsNumber()) {
        pp.exposure_min = json["exposure_min"].GetFloat();
    }
    if (json.HasMember("exposure_max") && json["exposure_max"].IsNumber()) {
        pp.exposure_max = json["exposure_max"].GetFloat();
    }
    if (json.HasMember("adaptation_speed_up") && json["adaptation_speed_up"].IsNumber()) {
        pp.adaptation_speed_up = json["adaptation_speed_up"].GetFloat();
    }
    if (json.HasMember("adaptation_speed_down") && json["adaptation_speed_down"].IsNumber()) {
        pp.adaptation_speed_down = json["adaptation_speed_down"].GetFloat();
    }
    if (json.HasMember("exposure_compensation") && json["exposure_compensation"].IsNumber()) {
        pp.exposure_compensation = json["exposure_compensation"].GetFloat();
    }
    if (json.HasMember("color_lut_intensity") && json["color_lut_intensity"].IsNumber()) {
        pp.color_lut_intensity = json["color_lut_intensity"].GetFloat();
    }
    if (json.HasMember("vignette_enabled") && json["vignette_enabled"].IsBool()) {
        pp.vignette_enabled = json["vignette_enabled"].GetBool();
    }
    if (json.HasMember("vignette_intensity") && json["vignette_intensity"].IsNumber()) {
        pp.vignette_intensity = json["vignette_intensity"].GetFloat();
    }
    if (json.HasMember("vignette_radius") && json["vignette_radius"].IsNumber()) {
        pp.vignette_radius = json["vignette_radius"].GetFloat();
    }
    if (json.HasMember("vignette_softness") && json["vignette_softness"].IsNumber()) {
        pp.vignette_softness = json["vignette_softness"].GetFloat();
    }
    if (json.HasMember("film_grain_enabled") && json["film_grain_enabled"].IsBool()) {
        pp.film_grain_enabled = json["film_grain_enabled"].GetBool();
    }
    if (json.HasMember("film_grain_intensity") && json["film_grain_intensity"].IsNumber()) {
        pp.film_grain_intensity = json["film_grain_intensity"].GetFloat();
    }
    if (json.HasMember("film_grain_time_scale") && json["film_grain_time_scale"].IsNumber()) {
        pp.film_grain_time_scale = json["film_grain_time_scale"].GetFloat();
    }
    if (json.HasMember("fxaa_enabled") && json["fxaa_enabled"].IsBool()) {
        pp.fxaa_enabled = json["fxaa_enabled"].GetBool();
    }
    if (json.HasMember("taa_enabled") && json["taa_enabled"].IsBool()) {
        pp.taa_enabled = json["taa_enabled"].GetBool();
    }
    if (json.HasMember("taa_blend_factor") && json["taa_blend_factor"].IsNumber()) {
        pp.taa_blend_factor = json["taa_blend_factor"].GetFloat();
    }
    if (json.HasMember("contact_shadow_enabled") && json["contact_shadow_enabled"].IsBool()) {
        pp.contact_shadow_enabled = json["contact_shadow_enabled"].GetBool();
    }
    if (json.HasMember("contact_shadow_strength") && json["contact_shadow_strength"].IsNumber()) {
        pp.contact_shadow_strength = json["contact_shadow_strength"].GetFloat();
    }
    if (json.HasMember("contact_shadow_steps") && json["contact_shadow_steps"].IsInt()) {
        pp.contact_shadow_steps = json["contact_shadow_steps"].GetInt();
    }
    if (json.HasMember("contact_shadow_step_size") && json["contact_shadow_step_size"].IsNumber()) {
        pp.contact_shadow_step_size = json["contact_shadow_step_size"].GetFloat();
    }
    if (json.HasMember("dof_enabled") && json["dof_enabled"].IsBool()) {
        pp.dof_enabled = json["dof_enabled"].GetBool();
    }
    if (json.HasMember("dof_focus_distance") && json["dof_focus_distance"].IsNumber()) {
        pp.dof_focus_distance = json["dof_focus_distance"].GetFloat();
    }
    if (json.HasMember("dof_focus_range") && json["dof_focus_range"].IsNumber()) {
        pp.dof_focus_range = json["dof_focus_range"].GetFloat();
    }
    if (json.HasMember("dof_bokeh_radius") && json["dof_bokeh_radius"].IsNumber()) {
        pp.dof_bokeh_radius = json["dof_bokeh_radius"].GetFloat();
    }
    if (json.HasMember("motion_blur_enabled") && json["motion_blur_enabled"].IsBool()) {
        pp.motion_blur_enabled = json["motion_blur_enabled"].GetBool();
    }
    if (json.HasMember("motion_blur_intensity") && json["motion_blur_intensity"].IsNumber()) {
        pp.motion_blur_intensity = json["motion_blur_intensity"].GetFloat();
    }
    if (json.HasMember("motion_blur_samples") && json["motion_blur_samples"].IsInt()) {
        pp.motion_blur_samples = json["motion_blur_samples"].GetInt();
    }
    if (json.HasMember("ssr_enabled") && json["ssr_enabled"].IsBool()) {
        pp.ssr_enabled = json["ssr_enabled"].GetBool();
    }
    if (json.HasMember("ssr_max_distance") && json["ssr_max_distance"].IsNumber()) {
        pp.ssr_max_distance = json["ssr_max_distance"].GetFloat();
    }
    if (json.HasMember("ssr_thickness") && json["ssr_thickness"].IsNumber()) {
        pp.ssr_thickness = json["ssr_thickness"].GetFloat();
    }
    if (json.HasMember("ssr_step_size") && json["ssr_step_size"].IsNumber()) {
        pp.ssr_step_size = json["ssr_step_size"].GetFloat();
    }
    if (json.HasMember("ssr_max_steps") && json["ssr_max_steps"].IsInt()) {
        pp.ssr_max_steps = json["ssr_max_steps"].GetInt();
    }
    if (json.HasMember("ssr_fade_distance") && json["ssr_fade_distance"].IsNumber()) {
        pp.ssr_fade_distance = json["ssr_fade_distance"].GetFloat();
    }
    if (json.HasMember("ssr_max_roughness") && json["ssr_max_roughness"].IsNumber()) {
        pp.ssr_max_roughness = json["ssr_max_roughness"].GetFloat();
    }
    if (json.HasMember("outline_enabled") && json["outline_enabled"].IsBool()) {
        pp.outline_enabled = json["outline_enabled"].GetBool();
    }
    if (json.HasMember("outline_color")) {
        ReadVec3(json["outline_color"], pp.outline_color);
    }
    if (json.HasMember("outline_thickness") && json["outline_thickness"].IsNumber()) {
        pp.outline_thickness = json["outline_thickness"].GetFloat();
    }
    if (json.HasMember("outline_depth_threshold") && json["outline_depth_threshold"].IsNumber()) {
        pp.outline_depth_threshold = json["outline_depth_threshold"].GetFloat();
    }
    if (json.HasMember("outline_normal_threshold") && json["outline_normal_threshold"].IsNumber()) {
        pp.outline_normal_threshold = json["outline_normal_threshold"].GetFloat();
    }
    if (json.HasMember("light_shaft_enabled") && json["light_shaft_enabled"].IsBool()) {
        pp.light_shaft_enabled = json["light_shaft_enabled"].GetBool();
    }
    if (json.HasMember("light_shaft_color")) {
        ReadVec3(json["light_shaft_color"], pp.light_shaft_color);
    }
    if (json.HasMember("light_shaft_density") && json["light_shaft_density"].IsNumber()) {
        pp.light_shaft_density = json["light_shaft_density"].GetFloat();
    }
    if (json.HasMember("light_shaft_weight") && json["light_shaft_weight"].IsNumber()) {
        pp.light_shaft_weight = json["light_shaft_weight"].GetFloat();
    }
    if (json.HasMember("light_shaft_decay") && json["light_shaft_decay"].IsNumber()) {
        pp.light_shaft_decay = json["light_shaft_decay"].GetFloat();
    }
    if (json.HasMember("light_shaft_exposure") && json["light_shaft_exposure"].IsNumber()) {
        pp.light_shaft_exposure = json["light_shaft_exposure"].GetFloat();
    }
    if (json.HasMember("light_shaft_intensity") && json["light_shaft_intensity"].IsNumber()) {
        pp.light_shaft_intensity = json["light_shaft_intensity"].GetFloat();
    }
    if (json.HasMember("light_shaft_samples") && json["light_shaft_samples"].IsInt()) {
        pp.light_shaft_samples = json["light_shaft_samples"].GetInt();
    }
    if (json.HasMember("fog_enabled") && json["fog_enabled"].IsBool()) {
        pp.fog_enabled = json["fog_enabled"].GetBool();
    }
    if (json.HasMember("fog_color")) {
        ReadVec3(json["fog_color"], pp.fog_color);
    }
    if (json.HasMember("fog_density") && json["fog_density"].IsNumber()) {
        pp.fog_density = json["fog_density"].GetFloat();
    }
    if (json.HasMember("fog_height_falloff") && json["fog_height_falloff"].IsNumber()) {
        pp.fog_height_falloff = json["fog_height_falloff"].GetFloat();
    }
    if (json.HasMember("fog_height_offset") && json["fog_height_offset"].IsNumber()) {
        pp.fog_height_offset = json["fog_height_offset"].GetFloat();
    }
    if (json.HasMember("fog_start") && json["fog_start"].IsNumber()) {
        pp.fog_start = json["fog_start"].GetFloat();
    }
    if (json.HasMember("fog_end") && json["fog_end"].IsNumber()) {
        pp.fog_end = json["fog_end"].GetFloat();
    }
    if (json.HasMember("fog_steps") && json["fog_steps"].IsInt()) {
        pp.fog_steps = json["fog_steps"].GetInt();
    }
    if (json.HasMember("fog_sun_scatter") && json["fog_sun_scatter"].IsNumber()) {
        pp.fog_sun_scatter = json["fog_sun_scatter"].GetFloat();
    }
    return pp;
}

} // namespace

void SerializeExtendedComponents(entt::registry& registry, Entity entity,
                                 rapidjson::Value& components,
                                 rapidjson::Document::AllocatorType& allocator) {
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
        rapidjson::Value json(rapidjson::kObjectType);
        json.AddMember("enabled", tree.enabled, allocator);
        json.AddMember("mesh_path", rapidjson::Value(tree.mesh_path.c_str(), allocator), allocator);
        json.AddMember("lod1_mesh_path", rapidjson::Value(tree.lod1_mesh_path.c_str(), allocator), allocator);
        json.AddMember("billboard_texture_path", rapidjson::Value(tree.billboard_texture_path.c_str(), allocator), allocator);
        json.AddMember("density", tree.density, allocator);
        json.AddMember("spawn_radius", tree.spawn_radius, allocator);
        json.AddMember("seed", tree.seed, allocator);
        json.AddMember("chunk_size", tree.chunk_size, allocator);
        json.AddMember("min_scale", tree.min_scale, allocator);
        json.AddMember("max_scale", tree.max_scale, allocator);
        json.AddMember("height_variation", tree.height_variation, allocator);
        json.AddMember("random_rotation", tree.random_rotation, allocator);
        json.AddMember("lod1_distance", tree.lod1_distance, allocator);
        json.AddMember("billboard_distance", tree.billboard_distance, allocator);
        json.AddMember("cull_distance", tree.cull_distance, allocator);
        json.AddMember("wind_strength", tree.wind_strength, allocator);
        json.AddMember("wind_speed", tree.wind_speed, allocator);
        json.AddMember("cast_shadow", tree.cast_shadow, allocator);
        json.AddMember("shadow_distance", tree.shadow_distance, allocator);
        components.AddMember("TreeComponent", json, allocator);
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
        const auto& json = components["TreeComponent"];
        dse::TreeComponent tree;
        if (json.HasMember("enabled") && json["enabled"].IsBool()) {
            tree.enabled = json["enabled"].GetBool();
        }
        if (json.HasMember("mesh_path") && json["mesh_path"].IsString()) {
            tree.mesh_path = json["mesh_path"].GetString();
        }
        if (json.HasMember("lod1_mesh_path") && json["lod1_mesh_path"].IsString()) {
            tree.lod1_mesh_path = json["lod1_mesh_path"].GetString();
        }
        if (json.HasMember("billboard_texture_path") && json["billboard_texture_path"].IsString()) {
            tree.billboard_texture_path = json["billboard_texture_path"].GetString();
        }
        if (json.HasMember("density") && json["density"].IsNumber()) {
            tree.density = json["density"].GetFloat();
        }
        if (json.HasMember("spawn_radius") && json["spawn_radius"].IsNumber()) {
            tree.spawn_radius = json["spawn_radius"].GetFloat();
        }
        if (json.HasMember("seed") && json["seed"].IsUint()) {
            tree.seed = json["seed"].GetUint();
        }
        if (json.HasMember("chunk_size") && json["chunk_size"].IsNumber()) {
            tree.chunk_size = json["chunk_size"].GetFloat();
        }
        if (json.HasMember("min_scale") && json["min_scale"].IsNumber()) {
            tree.min_scale = json["min_scale"].GetFloat();
        }
        if (json.HasMember("max_scale") && json["max_scale"].IsNumber()) {
            tree.max_scale = json["max_scale"].GetFloat();
        }
        if (json.HasMember("height_variation") && json["height_variation"].IsNumber()) {
            tree.height_variation = json["height_variation"].GetFloat();
        }
        if (json.HasMember("random_rotation") && json["random_rotation"].IsBool()) {
            tree.random_rotation = json["random_rotation"].GetBool();
        }
        if (json.HasMember("lod1_distance") && json["lod1_distance"].IsNumber()) {
            tree.lod1_distance = json["lod1_distance"].GetFloat();
        }
        if (json.HasMember("billboard_distance") && json["billboard_distance"].IsNumber()) {
            tree.billboard_distance = json["billboard_distance"].GetFloat();
        }
        if (json.HasMember("cull_distance") && json["cull_distance"].IsNumber()) {
            tree.cull_distance = json["cull_distance"].GetFloat();
        }
        if (json.HasMember("wind_strength") && json["wind_strength"].IsNumber()) {
            tree.wind_strength = json["wind_strength"].GetFloat();
        }
        if (json.HasMember("wind_speed") && json["wind_speed"].IsNumber()) {
            tree.wind_speed = json["wind_speed"].GetFloat();
        }
        if (json.HasMember("cast_shadow") && json["cast_shadow"].IsBool()) {
            tree.cast_shadow = json["cast_shadow"].GetBool();
        }
        if (json.HasMember("shadow_distance") && json["shadow_distance"].IsNumber()) {
            tree.shadow_distance = json["shadow_distance"].GetFloat();
        }
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

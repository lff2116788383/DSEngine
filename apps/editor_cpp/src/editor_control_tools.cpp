#include "editor_control_server.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "engine/runtime/engine_app.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/audio.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/trail_renderer_2d.h"
#include "engine/ecs/line_renderer_2d.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/audio_spatial_2d.h"
#include "engine/ecs/blueprint_component.h"
#include "engine/ecs/components_3d_impostor.h"
#include "engine/base/time.h"
#include "engine/scripting/lua/lua_runtime.h"
#include "engine/assets/asset_manager.h"
#include "editor_toolbar.h"
#include "editor_scene_io.h"
#include "editor_shared_components.h"
#include "editor_shortcuts.h"
#include "editor_undo.h"
#include "editor_entity_snapshot.h"
#include "editor_shell.h"
#include "editor_scene_tabs.h"
#include "editor_prefab.h"
#include "editor_selection.h"
#include "editor_project.h"
#include "editor_asset_db.h"

#include <vector>
#include <string>

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stb/stb_image_write.h>

namespace dse::editor {

// ─── Helper ─────────────────────────────────────────────────────────────────

static JsonRpcResponse MakeOk(rapidjson::Document result = {}) {
    JsonRpcResponse resp;
    resp.is_error = false;
    resp.result = std::move(result);
    return resp;
}

static JsonRpcResponse MakeToolError(int code, const std::string& msg) {
    JsonRpcResponse resp;
    resp.is_error = true;
    resp.error_code = code;
    resp.error_message = msg;
    return resp;
}

static void CollectEntityComponents(entt::registry& registry, entt::entity entity,
                                     rapidjson::Value& arr, rapidjson::Document::AllocatorType& alloc,
                                     bool include_properties);
static bool RemoveComponentByType(entt::registry& registry, entt::entity entity,
                                   const std::string& type);

// 实体计数：必须遍历 valid() 而非 storage<entt::entity>().size()——后者含已释放
// 但未回收的槽位，删除/scene_new 后不回落。所有对外回显 entity_count 的工具统一走此处。
static int CountValidEntities(entt::registry& registry) {
    int count = 0;
    for (auto entity : registry.storage<entt::entity>()) {
        if (registry.valid(entity)) ++count;
    }
    return count;
}

// ─── Tool: dsengine_lua_execute ─────────────────────────────────────────────

static JsonRpcResponse HandleLuaExecute(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("code") || !params["code"].IsString()) {
        return MakeToolError(-32602, "Missing required param: code");
    }

    const char* code = params["code"].GetString();
    std::string out;
    bool ok = dse::runtime::ExecuteLuaString(code, &out);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", ok, alloc);
    result.AddMember("output",
        rapidjson::Value(out.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_get_state ─────────────────────────────────────────

static JsonRpcResponse HandleSceneGetState(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    bool include_components = true;
    if (params.HasMember("include_components") && params["include_components"].IsBool()) {
        include_components = params["include_components"].GetBool();
    }

    World& world = engine.pipeline()->world();
    auto& registry = world.registry();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    result.AddMember("editor_state",
        rapidjson::Value(
            dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
            dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit",
            alloc),
        alloc);

    rapidjson::Value entities_arr(rapidjson::kArrayType);

    int valid_entity_count = 0;
    auto& entity_view = registry.storage<entt::entity>();
    for (auto entity : entity_view) {
        if (!registry.valid(entity)) continue;
        ++valid_entity_count;

        rapidjson::Value entity_obj(rapidjson::kObjectType);
        entity_obj.AddMember("id", static_cast<uint32_t>(entity), alloc);

        // Name
        if (registry.all_of<EditorNameComponent>(entity)) {
            const auto& name = registry.get<EditorNameComponent>(entity);
            entity_obj.AddMember("name",
                rapidjson::Value(name.name.c_str(), alloc), alloc);
        }

        // Hierarchy: parent_id (null at root) + sibling_index, so read-side
        // consumers can reconstruct the tree without touching the registry.
        entt::entity parent = entt::null;
        if (registry.all_of<ParentComponent>(entity))
            parent = registry.get<ParentComponent>(entity).parent;
        if (parent != entt::null && registry.valid(parent)) {
            entity_obj.AddMember("parent_id",
                rapidjson::Value(static_cast<uint32_t>(parent)), alloc);
        } else {
            entity_obj.AddMember("parent_id", rapidjson::Value(rapidjson::kNullType), alloc);
        }
        int sibling_index = registry.all_of<SiblingIndexComponent>(entity)
            ? registry.get<SiblingIndexComponent>(entity).index : 0;
        entity_obj.AddMember("sibling_index", rapidjson::Value(sibling_index), alloc);

        if (include_components) {
            rapidjson::Value components(rapidjson::kArrayType);
            CollectEntityComponents(registry, entity, components, alloc, true);
            entity_obj.AddMember("components", components, alloc);
        }

        entities_arr.PushBack(entity_obj, alloc);
    }

    result.AddMember("entities", entities_arr, alloc);
    result.AddMember("entity_count", valid_entity_count, alloc);
    return MakeOk(std::move(result));
}

// ─── Helper: 解析 vec3 数组 ──────────────────────────────────────────────────

static glm::vec3 ParseVec3(const rapidjson::Value& arr, glm::vec3 def = glm::vec3(0.0f)) {
    if (!arr.IsArray() || arr.Size() < 3) return def;
    return glm::vec3(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat());
}

static glm::vec4 ParseVec4(const rapidjson::Value& arr, glm::vec4 def = glm::vec4(1.0f)) {
    if (!arr.IsArray() || arr.Size() < 4) return def;
    return glm::vec4(arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat(), arr[3].GetFloat());
}

static glm::vec2 ParseVec2(const rapidjson::Value& arr, glm::vec2 def = glm::vec2(0.0f)) {
    if (!arr.IsArray() || arr.Size() < 2) return def;
    return glm::vec2(arr[0].GetFloat(), arr[1].GetFloat());
}

// ─── Helper: 按类型名添加组件 ────────────────────────────────────────────────

static void AddComponentByType(entt::registry& registry, entt::entity entity,
                               const std::string& type,
                               const rapidjson::Value* props) {
    auto getF = [&](const char* key, float def) -> float {
        if (props && props->HasMember(key) && (*props)[key].IsNumber())
            return (*props)[key].GetFloat();
        return def;
    };
    auto getB = [&](const char* key, bool def) -> bool {
        if (props && props->HasMember(key) && (*props)[key].IsBool())
            return (*props)[key].GetBool();
        return def;
    };
    auto getS = [&](const char* key, const char* def) -> std::string {
        if (props && props->HasMember(key) && (*props)[key].IsString())
            return (*props)[key].GetString();
        return def;
    };

    if (type == "MeshRenderer" || type == "MeshRendererComponent") {
        if (!registry.all_of<MeshRendererComponent>(entity)) {
            auto& mr = registry.emplace<MeshRendererComponent>(entity);
            mr.mesh_path = getS("mesh_path", "");
            mr.shader_variant = getS("shader_variant", "MESH_PBR");
            mr.metallic = getF("metallic", 0.0f);
            mr.roughness = getF("roughness", 0.5f);
            if (props && props->HasMember("color"))
                mr.color = ParseVec4((*props)["color"]);
        }
    } else if (type == "UIRenderer" || type == "UIRendererComponent") {
        if (!registry.all_of<UIRendererComponent>(entity)) {
            registry.emplace<UIRendererComponent>(entity);
        }
    } else if (type == "Camera3D" || type == "Camera3DComponent") {
        if (!registry.all_of<Camera3DComponent>(entity)) {
            auto& cam = registry.emplace<Camera3DComponent>(entity);
            cam.fov = getF("fov", 60.0f);
            cam.near_clip = getF("near", 0.1f);
            cam.far_clip = getF("far", 1000.0f);
        }
    } else if (type == "DirectionalLight" || type == "DirectionalLight3DComponent") {
        if (!registry.all_of<DirectionalLight3DComponent>(entity)) {
            auto& dl = registry.emplace<DirectionalLight3DComponent>(entity);
            dl.intensity = getF("intensity", 1.0f);
            if (props && props->HasMember("color"))
                dl.color = ParseVec3((*props)["color"], glm::vec3(1.0f));
            if (props && props->HasMember("direction"))
                dl.direction = ParseVec3((*props)["direction"], dl.direction);
        }
    } else if (type == "PointLight" || type == "PointLightComponent") {
        if (!registry.all_of<PointLightComponent>(entity)) {
            auto& pl = registry.emplace<PointLightComponent>(entity);
            pl.intensity = getF("intensity", 1.0f);
            pl.radius = getF("range", 10.0f);
            if (props && props->HasMember("color"))
                pl.color = ParseVec3((*props)["color"], glm::vec3(1.0f));
        }
    } else if (type == "SpotLight" || type == "SpotLightComponent") {
        if (!registry.all_of<SpotLightComponent>(entity)) {
            auto& sl = registry.emplace<SpotLightComponent>(entity);
            sl.intensity = getF("intensity", 1.0f);
            sl.radius = getF("range", 10.0f);
            sl.inner_cone_angle = getF("inner_cone", 12.5f);
            sl.outer_cone_angle = getF("outer_cone", 17.5f);
        }
    } else if (type == "RigidBody3D" || type == "RigidBody3DComponent") {
        if (!registry.all_of<RigidBody3DComponent>(entity)) {
            auto& rb = registry.emplace<RigidBody3DComponent>(entity);
            rb.mass = getF("mass", 1.0f);
            std::string body_type = getS("body_type", "dynamic");
            if (body_type == "static") rb.type = RigidBody3DType::Static;
            else if (body_type == "kinematic") rb.type = RigidBody3DType::Kinematic;
            else rb.type = RigidBody3DType::Dynamic;
        }
    } else if (type == "BoxCollider3D" || type == "BoxCollider3DComponent") {
        if (!registry.all_of<BoxCollider3DComponent>(entity)) {
            auto& bc = registry.emplace<BoxCollider3DComponent>(entity);
            if (props && props->HasMember("size"))
                bc.size = ParseVec3((*props)["size"], glm::vec3(1.0f));
            bc.is_trigger = getB("is_trigger", false);
        }
    } else if (type == "SphereCollider3D" || type == "SphereCollider3DComponent") {
        if (!registry.all_of<SphereCollider3DComponent>(entity)) {
            auto& sc = registry.emplace<SphereCollider3DComponent>(entity);
            sc.radius = getF("radius", 0.5f);
            sc.is_trigger = getB("is_trigger", false);
        }
    } else if (type == "AudioSource" || type == "AudioSourceComponent") {
        if (!registry.all_of<AudioSourceComponent>(entity)) {
            auto& as = registry.emplace<AudioSourceComponent>(entity);
            as.loop = getB("loop", false);
            as.play_on_awake = getB("play_on_awake", true);
            as.volume = getF("volume", 1.0f);
        }
    } else if (type == "AudioListener" || type == "AudioListenerComponent") {
        if (!registry.all_of<AudioListenerComponent>(entity))
            registry.emplace<AudioListenerComponent>(entity);
    } else if (type == "SkyLight" || type == "SkyLightComponent") {
        if (!registry.all_of<SkyLightComponent>(entity))
            registry.emplace<SkyLightComponent>(entity);
    } else if (type == "Skybox" || type == "SkyboxComponent") {
        if (!registry.all_of<SkyboxComponent>(entity)) {
            auto& sb = registry.emplace<SkyboxComponent>(entity);
            sb.cubemap_path = getS("cubemap_path", "");
        }
    } else if (type == "PostProcess" || type == "PostProcessComponent") {
        if (!registry.all_of<PostProcessComponent>(entity))
            registry.emplace<PostProcessComponent>(entity);
    }
    // ─── 3D Render (extended) ────────────────────────────────────────────────
    else if (type == "Decal" || type == "DecalComponent") {
        if (!registry.all_of<DecalComponent>(entity)) {
            auto& c = registry.emplace<DecalComponent>(entity);
            c.angle_fade = getF("angle_fade", 0.5f);
            if (props && props->HasMember("color")) c.color = ParseVec4((*props)["color"]);
        }
    } else if (type == "Water" || type == "WaterComponent") {
        if (!registry.all_of<WaterComponent>(entity)) {
            auto& c = registry.emplace<WaterComponent>(entity);
            c.water_level = getF("water_level", 0.0f);
            c.wave_amplitude = getF("wave_amplitude", 0.15f);
            c.wave_speed = getF("wave_speed", 1.0f);
            c.transparency = getF("transparency", 0.6f);
            if (props && props->HasMember("deep_color")) c.deep_color = ParseVec3((*props)["deep_color"], c.deep_color);
            if (props && props->HasMember("shallow_color")) c.shallow_color = ParseVec3((*props)["shallow_color"], c.shallow_color);
        }
    } else if (type == "Terrain" || type == "TerrainComponent") {
        if (!registry.all_of<TerrainComponent>(entity)) {
            auto& c = registry.emplace<TerrainComponent>(entity);
            c.width = getF("width", 100.0f);
            c.depth = getF("depth", 100.0f);
            c.max_height = getF("max_height", 20.0f);
            c.heightmap_path = getS("heightmap_path", "");
            c.texture_path = getS("texture_path", "");
        }
    } else if (type == "Grass" || type == "GrassComponent") {
        if (!registry.all_of<GrassComponent>(entity)) {
            auto& c = registry.emplace<GrassComponent>(entity);
            c.density = getF("density", 1.0f);
            c.spawn_radius = getF("spawn_radius", 50.0f);
            c.blade_width = getF("blade_width", 0.1f);
            c.blade_height = getF("blade_height", 1.0f);
            if (props && props->HasMember("base_color")) c.base_color = ParseVec3((*props)["base_color"], c.base_color);
            if (props && props->HasMember("tip_color")) c.tip_color = ParseVec3((*props)["tip_color"], c.tip_color);
        }
    } else if (type == "LODGroup" || type == "LODGroupComponent") {
        if (!registry.all_of<LODGroupComponent>(entity)) {
            auto& c = registry.emplace<LODGroupComponent>(entity);
            c.global_scale = getF("global_scale", 1.0f);
            c.hysteresis = getF("hysteresis", 0.05f);
        }
    } else if (type == "LightProbe" || type == "LightProbeComponent") {
        if (!registry.all_of<LightProbeComponent>(entity)) {
            auto& c = registry.emplace<LightProbeComponent>(entity);
            c.influence_radius = getF("influence_radius", 10.0f);
        }
    } else if (type == "ReflectionProbe" || type == "ReflectionProbeComponent") {
        if (!registry.all_of<ReflectionProbeComponent>(entity)) {
            auto& c = registry.emplace<ReflectionProbeComponent>(entity);
            c.influence_radius = getF("influence_radius", 15.0f);
            c.use_box_projection = getB("use_box_projection", false);
        }
    } else if (type == "GIProbeVolume" || type == "GIProbeVolumeComponent") {
        if (!registry.all_of<GIProbeVolumeComponent>(entity)) {
            auto& c = registry.emplace<GIProbeVolumeComponent>(entity);
            c.gi_intensity = getF("gi_intensity", 1.0f);
        }
    } else if (type == "Impostor" || type == "ImpostorComponent") {
        if (!registry.all_of<ImpostorComponent>(entity)) {
            auto& c = registry.emplace<ImpostorComponent>(entity);
            c.atlas_path = getS("atlas_path", "");
            c.transition_distance = getF("transition_distance", 100.0f);
        }
    } else if (type == "SubScene" || type == "SubSceneComponent") {
        if (!registry.all_of<SubSceneComponent>(entity)) {
            auto& c = registry.emplace<SubSceneComponent>(entity);
            c.scene_path = getS("scene_path", "");
        }
    } else if (type == "FreeCameraController" || type == "FreeCameraControllerComponent") {
        if (!registry.all_of<FreeCameraControllerComponent>(entity)) {
            auto& c = registry.emplace<FreeCameraControllerComponent>(entity);
            c.move_speed = getF("move_speed", 5.0f);
            c.mouse_sensitivity = getF("mouse_sensitivity", 0.1f);
        }
    }
    // ─── 3D Physics (extended) ───────────────────────────────────────────────
    else if (type == "CapsuleCollider3D" || type == "CapsuleCollider3DComponent") {
        if (!registry.all_of<CapsuleCollider3DComponent>(entity)) {
            auto& c = registry.emplace<CapsuleCollider3DComponent>(entity);
            c.radius = getF("radius", 0.5f);
            c.height = getF("height", 1.0f);
            c.is_trigger = getB("is_trigger", false);
        }
    } else if (type == "MeshCollider3D" || type == "MeshCollider3DComponent") {
        if (!registry.all_of<MeshCollider3DComponent>(entity)) {
            auto& c = registry.emplace<MeshCollider3DComponent>(entity);
            c.convex = getB("convex", false);
            c.is_trigger = getB("is_trigger", false);
        }
    } else if (type == "CharacterController3D" || type == "CharacterController3DComponent") {
        if (!registry.all_of<CharacterController3DComponent>(entity)) {
            auto& c = registry.emplace<CharacterController3DComponent>(entity);
            c.radius = getF("radius", 0.3f);
            c.height = getF("height", 1.0f);
            c.slope_limit = getF("slope_limit", 45.0f);
            c.step_offset = getF("step_offset", 0.3f);
        }
    } else if (type == "Joint3D" || type == "Joint3DComponent") {
        if (!registry.all_of<Joint3DComponent>(entity)) {
            auto& c = registry.emplace<Joint3DComponent>(entity);
            c.spring_stiffness = getF("spring_stiffness", 100.0f);
            c.spring_damping = getF("spring_damping", 10.0f);
            if (props && props->HasMember("anchor")) c.anchor = ParseVec3((*props)["anchor"]);
        }
    } else if (type == "SoftBody" || type == "SoftBodyComponent") {
        if (!registry.all_of<SoftBodyComponent>(entity)) {
            auto& c = registry.emplace<SoftBodyComponent>(entity);
            c.stiffness = getF("stiffness", 0.5f);
            c.damping = getF("damping", 0.99f);
        }
    } else if (type == "Rope" || type == "RopeComponent") {
        if (!registry.all_of<RopeComponent>(entity)) {
            auto& c = registry.emplace<RopeComponent>(entity);
            c.segment_count = props && props->HasMember("segment_count") && (*props)["segment_count"].IsInt() ? (*props)["segment_count"].GetInt() : 10;
            c.segment_length = getF("segment_length", 0.2f);
        }
    }
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    else if (type == "Ragdoll" || type == "RagdollComponent") {
        if (!registry.all_of<RagdollComponent>(entity)) {
            auto& c = registry.emplace<RagdollComponent>(entity);
            c.total_mass = getF("total_mass", 10.0f);
            c.auto_setup = getB("auto_setup", true);
        }
    } else if (type == "Vehicle" || type == "VehicleComponent") {
        if (!registry.all_of<VehicleComponent>(entity)) {
            auto& c = registry.emplace<VehicleComponent>(entity);
            c.max_engine_force = getF("max_engine_force", 5000.0f);
            c.max_steer_angle = getF("max_steer_angle", 35.0f);
        }
    } else if (type == "Buoyancy" || type == "BuoyancyComponent") {
        if (!registry.all_of<BuoyancyComponent>(entity)) {
            auto& c = registry.emplace<BuoyancyComponent>(entity);
            c.water_level = getF("water_level", 0.0f);
            c.buoyancy_force = getF("buoyancy_force", 10.0f);
        }
    }
#endif
    // ─── 3D Animation ───────────────────────────────────────────────────────
    else if (type == "Animator3D" || type == "Animator3DComponent") {
        if (!registry.all_of<Animator3DComponent>(entity)) {
            auto& c = registry.emplace<Animator3DComponent>(entity);
            c.dskel_path = getS("dskel_path", "");
            c.danim_path = getS("danim_path", "");
            c.speed = getF("speed", 1.0f);
            c.loop = getB("loop", true);
        }
    } else if (type == "BoneAttachment" || type == "BoneAttachmentComponent") {
        if (!registry.all_of<BoneAttachmentComponent>(entity)) {
            auto& c = registry.emplace<BoneAttachmentComponent>(entity);
            c.bone_name = getS("bone_name", "");
            if (props && props->HasMember("offset_position")) c.offset_position = ParseVec3((*props)["offset_position"]);
        }
    }
    // ─── 3D Particle ─────────────────────────────────────────────────────────
    else if (type == "ParticleSystem3D" || type == "ParticleSystem3DComponent") {
        if (!registry.all_of<ParticleSystem3DComponent>(entity)) {
            auto& c = registry.emplace<ParticleSystem3DComponent>(entity);
            c.max_particles = props && props->HasMember("max_particles") && (*props)["max_particles"].IsInt() ? (*props)["max_particles"].GetInt() : 1000;
            c.emission_rate = getF("emission_rate", 100.0f);
            c.start_speed_min = getF("start_speed_min", 1.0f);
            c.start_speed_max = getF("start_speed_max", 5.0f);
            if (props && props->HasMember("start_color")) c.start_color = ParseVec4((*props)["start_color"]);
            c.texture_path = getS("texture_path", "");
        }
    }
    // ─── AI ──────────────────────────────────────────────────────────────────
    else if (type == "BehaviorTree" || type == "BehaviorTreeComponent") {
        if (!registry.all_of<BehaviorTreeComponent>(entity)) {
            auto& c = registry.emplace<BehaviorTreeComponent>(entity);
            c.tree_name = getS("tree_name", "");
            c.auto_restart = getB("auto_restart", true);
        }
    } else if (type == "Cutscene" || type == "CutsceneComponent") {
        if (!registry.all_of<CutsceneComponent>(entity)) {
            auto& c = registry.emplace<CutsceneComponent>(entity);
            c.sequence_name = getS("sequence_name", "");
            c.auto_play = getB("auto_play", false);
        }
    } else if (type == "Steering" || type == "SteeringComponent") {
        if (!registry.all_of<SteeringComponent>(entity)) {
            auto& c = registry.emplace<SteeringComponent>(entity);
            c.max_velocity = getF("max_velocity", 5.0f);
            c.max_force = getF("max_force", 10.0f);
        }
    }
#ifdef DSE_ENABLE_NAVMESH
    else if (type == "NavMeshAgent" || type == "NavMeshAgentComponent") {
        if (!registry.all_of<NavMeshAgentComponent>(entity)) {
            auto& c = registry.emplace<NavMeshAgentComponent>(entity);
            c.speed = getF("speed", 3.5f);
            c.agent_radius = getF("agent_radius", 0.6f);
            c.agent_height = getF("agent_height", 2.0f);
            if (props && props->HasMember("destination")) c.destination = ParseVec3((*props)["destination"]);
        }
    }
#endif
    // ─── Script ──────────────────────────────────────────────────────────────
    else if (type == "Script" || type == "ScriptComponent") {
        if (!registry.all_of<ScriptComponent>(entity)) {
            auto& c = registry.emplace<ScriptComponent>(entity);
            c.script_path = getS("script_path", "");
            c.enabled = getB("enabled", true);
        }
    } else if (type == "LuaScript" || type == "LuaScriptComponent") {
        if (!registry.all_of<LuaScriptComponent>(entity)) {
            auto& c = registry.emplace<LuaScriptComponent>(entity);
            c.script_path = getS("script_path", "");
        }
    } else if (type == "CSharpScript" || type == "CSharpScriptComponent") {
        if (!registry.all_of<CSharpScriptComponent>(entity)) {
            auto& c = registry.emplace<CSharpScriptComponent>(entity);
            c.class_name = getS("class_name", "");
            c.enabled = getB("enabled", true);
        }
    } else if (type == "Blueprint" || type == "BlueprintComponent") {
        if (!registry.all_of<BlueprintComponent>(entity)) {
            auto& c = registry.emplace<BlueprintComponent>(entity);
            c.blueprint_asset_path = getS("blueprint_asset_path", "");
            c.enabled = getB("enabled", true);
        }
    }
    // ─── UI ─────────────────────────────────────────────────────────────────
    else if (type == "UIPanel" || type == "UIPanelComponent") {
        if (!registry.all_of<UIPanelComponent>(entity))
            registry.emplace<UIPanelComponent>(entity).blocks_input = getB("blocks_input", false);
    } else if (type == "UIButton" || type == "UIButtonComponent") {
        if (!registry.all_of<UIButtonComponent>(entity)) {
            auto& c = registry.emplace<UIButtonComponent>(entity);
            if (props && props->HasMember("normal_color")) c.normal_color = ParseVec4((*props)["normal_color"]);
            if (props && props->HasMember("hover_color")) c.hover_color = ParseVec4((*props)["hover_color"]);
            if (props && props->HasMember("pressed_color")) c.pressed_color = ParseVec4((*props)["pressed_color"]);
        }
    } else if (type == "UILabel" || type == "UILabelComponent") {
        if (!registry.all_of<UILabelComponent>(entity)) {
            auto& c = registry.emplace<UILabelComponent>(entity);
            c.text = getS("text", "");
            c.font_size = getF("font_size", 32.0f);
            if (props && props->HasMember("color")) c.color = ParseVec4((*props)["color"]);
        }
    } else if (type == "UIMask" || type == "UIMaskComponent") {
        if (!registry.all_of<UIMaskComponent>(entity))
            registry.emplace<UIMaskComponent>(entity);
    } else if (type == "UIRichText" || type == "UIRichTextComponent") {
        if (!registry.all_of<UIRichTextComponent>(entity)) {
            auto& c = registry.emplace<UIRichTextComponent>(entity);
            c.text = getS("text", "");
        }
    } else if (type == "UIJoystick" || type == "UIJoystickComponent") {
        if (!registry.all_of<UIJoystickComponent>(entity)) {
            auto& c = registry.emplace<UIJoystickComponent>(entity);
            c.max_radius = getF("max_radius", 64.0f);
        }
    } else if (type == "UIAnchor" || type == "UIAnchorComponent") {
        if (!registry.all_of<UIAnchorComponent>(entity)) {
            auto& c = registry.emplace<UIAnchorComponent>(entity);
            c.anchor = props && props->HasMember("anchor") && (*props)["anchor"].IsInt() ? (*props)["anchor"].GetInt() : 5;
        }
    } else if (type == "UIGridLayout" || type == "UIGridLayoutComponent") {
        if (!registry.all_of<UIGridLayoutComponent>(entity)) {
            auto& c = registry.emplace<UIGridLayoutComponent>(entity);
            c.columns = props && props->HasMember("columns") && (*props)["columns"].IsInt() ? (*props)["columns"].GetInt() : 1;
            if (props && props->HasMember("cell_size")) c.cell_size = ParseVec2((*props)["cell_size"], c.cell_size);
            if (props && props->HasMember("spacing")) c.spacing = ParseVec2((*props)["spacing"], c.spacing);
        }
    } else if (type == "UICanvasScaler" || type == "UICanvasScalerComponent") {
        if (!registry.all_of<UICanvasScalerComponent>(entity)) {
            auto& c = registry.emplace<UICanvasScalerComponent>(entity);
            if (props && props->HasMember("reference_resolution")) c.reference_resolution = ParseVec2((*props)["reference_resolution"], c.reference_resolution);
        }
    } else if (type == "UIBoxLayout" || type == "UIBoxLayoutComponent") {
        if (!registry.all_of<UIBoxLayoutComponent>(entity)) {
            auto& c = registry.emplace<UIBoxLayoutComponent>(entity);
            c.vertical = getB("vertical", false);
            c.spacing = getF("spacing", 0.0f);
        }
    } else if (type == "UIContentSizeFitter" || type == "UIContentSizeFitterComponent") {
        if (!registry.all_of<UIContentSizeFitterComponent>(entity))
            registry.emplace<UIContentSizeFitterComponent>(entity);
    } else if (type == "UIAnimation" || type == "UIAnimationComponent") {
        if (!registry.all_of<UIAnimationComponent>(entity)) {
            auto& c = registry.emplace<UIAnimationComponent>(entity);
            c.duration = getF("duration", 0.3f);
            c.loop = getB("loop", false);
        }
    } else if (type == "UITextInput" || type == "UITextInputComponent") {
        if (!registry.all_of<UITextInputComponent>(entity)) {
            auto& c = registry.emplace<UITextInputComponent>(entity);
            c.placeholder = getS("placeholder", "");
            c.is_password = getB("is_password", false);
            c.multiline = getB("multiline", false);
        }
    } else if (type == "UIScrollView" || type == "UIScrollViewComponent") {
        if (!registry.all_of<UIScrollViewComponent>(entity)) {
            auto& c = registry.emplace<UIScrollViewComponent>(entity);
            c.horizontal = getB("horizontal", false);
            c.vertical = getB("vertical", true);
        }
    } else if (type == "UISlider" || type == "UISliderComponent") {
        if (!registry.all_of<UISliderComponent>(entity)) {
            auto& c = registry.emplace<UISliderComponent>(entity);
            c.min_value = getF("min_value", 0.0f);
            c.max_value = getF("max_value", 1.0f);
            c.value = getF("value", 0.0f);
        }
    } else if (type == "UIToggle" || type == "UIToggleComponent") {
        if (!registry.all_of<UIToggleComponent>(entity)) {
            auto& c = registry.emplace<UIToggleComponent>(entity);
            c.is_on = getB("is_on", false);
        }
    } else if (type == "UIProgressBar" || type == "UIProgressBarComponent") {
        if (!registry.all_of<UIProgressBarComponent>(entity)) {
            auto& c = registry.emplace<UIProgressBarComponent>(entity);
            c.value = getF("value", 0.0f);
            c.max_value = getF("max_value", 1.0f);
        }
    } else if (type == "UIDropdown" || type == "UIDropdownComponent") {
        if (!registry.all_of<UIDropdownComponent>(entity))
            registry.emplace<UIDropdownComponent>(entity);
    }
    // ─── 2D ─────────────────────────────────────────────────────────────────
    else if (type == "SpriteRenderer" || type == "SpriteRendererComponent") {
        if (!registry.all_of<SpriteRendererComponent>(entity)) {
            auto& c = registry.emplace<SpriteRendererComponent>(entity);
            c.shader_variant = getS("shader_variant", "SPRITE_UNLIT");
            if (props && props->HasMember("color")) c.color = ParseVec4((*props)["color"]);
            c.visible = getB("visible", true);
        }
    } else if (type == "Light2D" || type == "Light2DComponent") {
        if (!registry.all_of<Light2DComponent>(entity)) {
            auto& c = registry.emplace<Light2DComponent>(entity);
            c.intensity = getF("intensity", 1.0f);
            c.range = getF("range", 5.0f);
            if (props && props->HasMember("color")) c.color = ParseVec3((*props)["color"], c.color);
        }
    } else if (type == "TrailRenderer2D" || type == "TrailRenderer2DComponent") {
        if (!registry.all_of<TrailRenderer2DComponent>(entity)) {
            auto& c = registry.emplace<TrailRenderer2DComponent>(entity);
            c.lifetime = getF("lifetime", 0.5f);
            c.start_width = getF("start_width", 0.5f);
            c.end_width = getF("end_width", 0.0f);
        }
    } else if (type == "LineRenderer2D" || type == "LineRenderer2DComponent") {
        if (!registry.all_of<LineRenderer2DComponent>(entity)) {
            auto& c = registry.emplace<LineRenderer2DComponent>(entity);
            c.width = getF("width", 0.1f);
        }
    } else if (type == "Parallax" || type == "ParallaxComponent") {
        if (!registry.all_of<ParallaxComponent>(entity))
            registry.emplace<ParallaxComponent>(entity);
    } else if (type == "CameraController2D" || type == "CameraController2DComponent") {
        if (!registry.all_of<CameraController2DComponent>(entity)) {
            auto& c = registry.emplace<CameraController2DComponent>(entity);
            c.target_zoom = getF("target_zoom", 1.0f);
        }
    } else if (type == "AudioSpatial2D" || type == "AudioSpatial2DComponent") {
        if (!registry.all_of<AudioSpatial2DComponent>(entity)) {
            auto& c = registry.emplace<AudioSpatial2DComponent>(entity);
            c.min_distance = getF("min_distance", 1.0f);
            c.max_distance = getF("max_distance", 20.0f);
        }
    } else if (type == "AudioListener2D" || type == "AudioListener2DComponent") {
        if (!registry.all_of<AudioListener2DComponent>(entity))
            registry.emplace<AudioListener2DComponent>(entity);
    }
    // 未知类型静默忽略
}

static bool ModifyComponentProperties(entt::registry& registry, entt::entity entity,
                                       const std::string& type, const rapidjson::Value& props) {
    auto getF = [&](const char* key, float def) -> float {
        if (props.HasMember(key) && props[key].IsNumber()) return props[key].GetFloat();
        return def;
    };

    if (type == "MeshRenderer" || type == "MeshRendererComponent") {
        if (!registry.all_of<MeshRendererComponent>(entity)) return false;
        auto& mr = registry.get<MeshRendererComponent>(entity);
        if (props.HasMember("mesh_path") && props["mesh_path"].IsString())
            mr.mesh_path = props["mesh_path"].GetString();
        if (props.HasMember("shader_variant") && props["shader_variant"].IsString())
            mr.shader_variant = props["shader_variant"].GetString();
        if (props.HasMember("color") && props["color"].IsArray())
            mr.color = ParseVec4(props["color"]);
        if (props.HasMember("metallic")) mr.metallic = getF("metallic", mr.metallic);
        if (props.HasMember("roughness")) mr.roughness = getF("roughness", mr.roughness);
        return true;
    } else if (type == "Camera3D" || type == "Camera3DComponent") {
        if (!registry.all_of<Camera3DComponent>(entity)) return false;
        auto& c = registry.get<Camera3DComponent>(entity);
        if (props.HasMember("fov")) c.fov = getF("fov", c.fov);
        if (props.HasMember("near_clip")) c.near_clip = getF("near_clip", c.near_clip);
        if (props.HasMember("far_clip")) c.far_clip = getF("far_clip", c.far_clip);
        return true;
    } else if (type == "DirectionalLight" || type == "DirectionalLight3DComponent") {
        if (!registry.all_of<DirectionalLight3DComponent>(entity)) return false;
        auto& dl = registry.get<DirectionalLight3DComponent>(entity);
        if (props.HasMember("intensity")) dl.intensity = getF("intensity", dl.intensity);
        if (props.HasMember("color") && props["color"].IsArray())
            dl.color = ParseVec3(props["color"], dl.color);
        if (props.HasMember("direction") && props["direction"].IsArray())
            dl.direction = ParseVec3(props["direction"], dl.direction);
        return true;
    } else if (type == "PointLight" || type == "PointLightComponent") {
        if (!registry.all_of<PointLightComponent>(entity)) return false;
        auto& pl = registry.get<PointLightComponent>(entity);
        if (props.HasMember("intensity")) pl.intensity = getF("intensity", pl.intensity);
        if (props.HasMember("range")) pl.radius = getF("range", pl.radius);
        if (props.HasMember("color") && props["color"].IsArray())
            pl.color = ParseVec3(props["color"], pl.color);
        return true;
    } else if (type == "SpotLight" || type == "SpotLightComponent") {
        if (!registry.all_of<SpotLightComponent>(entity)) return false;
        auto& sl = registry.get<SpotLightComponent>(entity);
        if (props.HasMember("intensity")) sl.intensity = getF("intensity", sl.intensity);
        if (props.HasMember("range")) sl.radius = getF("range", sl.radius);
        if (props.HasMember("inner_cone")) sl.inner_cone_angle = getF("inner_cone", sl.inner_cone_angle);
        if (props.HasMember("outer_cone")) sl.outer_cone_angle = getF("outer_cone", sl.outer_cone_angle);
        return true;
    } else if (type == "RigidBody3D" || type == "RigidBody3DComponent") {
        if (!registry.all_of<RigidBody3DComponent>(entity)) return false;
        auto& rb = registry.get<RigidBody3DComponent>(entity);
        if (props.HasMember("mass")) rb.mass = getF("mass", rb.mass);
        if (props.HasMember("body_type") && props["body_type"].IsString()) {
            std::string bt = props["body_type"].GetString();
            if (bt == "static") rb.type = RigidBody3DType::Static;
            else if (bt == "kinematic") rb.type = RigidBody3DType::Kinematic;
            else rb.type = RigidBody3DType::Dynamic;
        }
        return true;
    } else if (type == "AudioSource" || type == "AudioSourceComponent") {
        if (!registry.all_of<AudioSourceComponent>(entity)) return false;
        auto& as = registry.get<AudioSourceComponent>(entity);
        if (props.HasMember("volume")) as.volume = getF("volume", as.volume);
        if (props.HasMember("pitch")) as.pitch = getF("pitch", as.pitch);
        if (props.HasMember("loop") && props["loop"].IsBool()) as.loop = props["loop"].GetBool();
        return true;
    } else if (type == "Skybox" || type == "SkyboxComponent") {
        if (!registry.all_of<SkyboxComponent>(entity)) return false;
        auto& c = registry.get<SkyboxComponent>(entity);
        if (props.HasMember("cubemap_path") && props["cubemap_path"].IsString())
            c.cubemap_path = props["cubemap_path"].GetString();
        return true;
    } else if (type == "BoxCollider3D" || type == "BoxCollider3DComponent") {
        if (!registry.all_of<BoxCollider3DComponent>(entity)) return false;
        auto& c = registry.get<BoxCollider3DComponent>(entity);
        if (props.HasMember("size") && props["size"].IsArray()) c.size = ParseVec3(props["size"], c.size);
        if (props.HasMember("is_trigger") && props["is_trigger"].IsBool()) c.is_trigger = props["is_trigger"].GetBool();
        if (props.HasMember("center") && props["center"].IsArray()) c.center = ParseVec3(props["center"], c.center);
        return true;
    } else if (type == "SphereCollider3D" || type == "SphereCollider3DComponent") {
        if (!registry.all_of<SphereCollider3DComponent>(entity)) return false;
        auto& c = registry.get<SphereCollider3DComponent>(entity);
        if (props.HasMember("radius")) c.radius = getF("radius", c.radius);
        if (props.HasMember("is_trigger") && props["is_trigger"].IsBool()) c.is_trigger = props["is_trigger"].GetBool();
        return true;
    }
    // ─── 3D Render (extended) modify ─────────────────────────────────────────
    else if (type == "Decal" || type == "DecalComponent") {
        if (!registry.all_of<DecalComponent>(entity)) return false;
        auto& c = registry.get<DecalComponent>(entity);
        if (props.HasMember("enabled") && props["enabled"].IsBool()) c.enabled = props["enabled"].GetBool();
        if (props.HasMember("angle_fade")) c.angle_fade = getF("angle_fade", c.angle_fade);
        if (props.HasMember("color") && props["color"].IsArray()) c.color = ParseVec4(props["color"]);
        return true;
    } else if (type == "Water" || type == "WaterComponent") {
        if (!registry.all_of<WaterComponent>(entity)) return false;
        auto& c = registry.get<WaterComponent>(entity);
        if (props.HasMember("water_level")) c.water_level = getF("water_level", c.water_level);
        if (props.HasMember("wave_amplitude")) c.wave_amplitude = getF("wave_amplitude", c.wave_amplitude);
        if (props.HasMember("wave_speed")) c.wave_speed = getF("wave_speed", c.wave_speed);
        if (props.HasMember("transparency")) c.transparency = getF("transparency", c.transparency);
        if (props.HasMember("deep_color") && props["deep_color"].IsArray()) c.deep_color = ParseVec3(props["deep_color"], c.deep_color);
        if (props.HasMember("shallow_color") && props["shallow_color"].IsArray()) c.shallow_color = ParseVec3(props["shallow_color"], c.shallow_color);
        return true;
    } else if (type == "Terrain" || type == "TerrainComponent") {
        if (!registry.all_of<TerrainComponent>(entity)) return false;
        auto& c = registry.get<TerrainComponent>(entity);
        if (props.HasMember("width")) c.width = getF("width", c.width);
        if (props.HasMember("depth")) c.depth = getF("depth", c.depth);
        if (props.HasMember("max_height")) c.max_height = getF("max_height", c.max_height);
        if (props.HasMember("heightmap_path") && props["heightmap_path"].IsString()) c.heightmap_path = props["heightmap_path"].GetString();
        if (props.HasMember("texture_path") && props["texture_path"].IsString()) c.texture_path = props["texture_path"].GetString();
        return true;
    } else if (type == "Grass" || type == "GrassComponent") {
        if (!registry.all_of<GrassComponent>(entity)) return false;
        auto& c = registry.get<GrassComponent>(entity);
        if (props.HasMember("density")) c.density = getF("density", c.density);
        if (props.HasMember("spawn_radius")) c.spawn_radius = getF("spawn_radius", c.spawn_radius);
        if (props.HasMember("blade_width")) c.blade_width = getF("blade_width", c.blade_width);
        if (props.HasMember("blade_height")) c.blade_height = getF("blade_height", c.blade_height);
        if (props.HasMember("base_color") && props["base_color"].IsArray()) c.base_color = ParseVec3(props["base_color"], c.base_color);
        if (props.HasMember("tip_color") && props["tip_color"].IsArray()) c.tip_color = ParseVec3(props["tip_color"], c.tip_color);
        return true;
    } else if (type == "LODGroup" || type == "LODGroupComponent") {
        if (!registry.all_of<LODGroupComponent>(entity)) return false;
        auto& c = registry.get<LODGroupComponent>(entity);
        if (props.HasMember("global_scale")) c.global_scale = getF("global_scale", c.global_scale);
        if (props.HasMember("hysteresis")) c.hysteresis = getF("hysteresis", c.hysteresis);
        return true;
    } else if (type == "LightProbe" || type == "LightProbeComponent") {
        if (!registry.all_of<LightProbeComponent>(entity)) return false;
        auto& c = registry.get<LightProbeComponent>(entity);
        if (props.HasMember("influence_radius")) c.influence_radius = getF("influence_radius", c.influence_radius);
        return true;
    } else if (type == "ReflectionProbe" || type == "ReflectionProbeComponent") {
        if (!registry.all_of<ReflectionProbeComponent>(entity)) return false;
        auto& c = registry.get<ReflectionProbeComponent>(entity);
        if (props.HasMember("influence_radius")) c.influence_radius = getF("influence_radius", c.influence_radius);
        if (props.HasMember("use_box_projection") && props["use_box_projection"].IsBool()) c.use_box_projection = props["use_box_projection"].GetBool();
        return true;
    } else if (type == "GIProbeVolume" || type == "GIProbeVolumeComponent") {
        if (!registry.all_of<GIProbeVolumeComponent>(entity)) return false;
        auto& c = registry.get<GIProbeVolumeComponent>(entity);
        if (props.HasMember("gi_intensity")) c.gi_intensity = getF("gi_intensity", c.gi_intensity);
        return true;
    } else if (type == "Impostor" || type == "ImpostorComponent") {
        if (!registry.all_of<ImpostorComponent>(entity)) return false;
        auto& c = registry.get<ImpostorComponent>(entity);
        if (props.HasMember("atlas_path") && props["atlas_path"].IsString()) c.atlas_path = props["atlas_path"].GetString();
        if (props.HasMember("transition_distance")) c.transition_distance = getF("transition_distance", c.transition_distance);
        if (props.HasMember("fade_range")) c.fade_range = getF("fade_range", c.fade_range);
        if (props.HasMember("cull_distance")) c.cull_distance = getF("cull_distance", c.cull_distance);
        if (props.HasMember("cast_shadow") && props["cast_shadow"].IsBool()) c.cast_shadow = props["cast_shadow"].GetBool();
        return true;
    } else if (type == "SubScene" || type == "SubSceneComponent") {
        if (!registry.all_of<SubSceneComponent>(entity)) return false;
        auto& c = registry.get<SubSceneComponent>(entity);
        if (props.HasMember("scene_path") && props["scene_path"].IsString()) c.scene_path = props["scene_path"].GetString();
        return true;
    } else if (type == "FreeCameraController" || type == "FreeCameraControllerComponent") {
        if (!registry.all_of<FreeCameraControllerComponent>(entity)) return false;
        auto& c = registry.get<FreeCameraControllerComponent>(entity);
        if (props.HasMember("move_speed")) c.move_speed = getF("move_speed", c.move_speed);
        if (props.HasMember("mouse_sensitivity")) c.mouse_sensitivity = getF("mouse_sensitivity", c.mouse_sensitivity);
        return true;
    }
    // ─── 3D Physics (extended) modify ────────────────────────────────────────
    else if (type == "CapsuleCollider3D" || type == "CapsuleCollider3DComponent") {
        if (!registry.all_of<CapsuleCollider3DComponent>(entity)) return false;
        auto& c = registry.get<CapsuleCollider3DComponent>(entity);
        if (props.HasMember("radius")) c.radius = getF("radius", c.radius);
        if (props.HasMember("height")) c.height = getF("height", c.height);
        if (props.HasMember("is_trigger") && props["is_trigger"].IsBool()) c.is_trigger = props["is_trigger"].GetBool();
        if (props.HasMember("center") && props["center"].IsArray()) c.center = ParseVec3(props["center"], c.center);
        return true;
    } else if (type == "MeshCollider3D" || type == "MeshCollider3DComponent") {
        if (!registry.all_of<MeshCollider3DComponent>(entity)) return false;
        auto& c = registry.get<MeshCollider3DComponent>(entity);
        if (props.HasMember("convex") && props["convex"].IsBool()) c.convex = props["convex"].GetBool();
        if (props.HasMember("is_trigger") && props["is_trigger"].IsBool()) c.is_trigger = props["is_trigger"].GetBool();
        return true;
    } else if (type == "CharacterController3D" || type == "CharacterController3DComponent") {
        if (!registry.all_of<CharacterController3DComponent>(entity)) return false;
        auto& c = registry.get<CharacterController3DComponent>(entity);
        if (props.HasMember("radius")) c.radius = getF("radius", c.radius);
        if (props.HasMember("height")) c.height = getF("height", c.height);
        if (props.HasMember("slope_limit")) c.slope_limit = getF("slope_limit", c.slope_limit);
        if (props.HasMember("step_offset")) c.step_offset = getF("step_offset", c.step_offset);
        return true;
    } else if (type == "Joint3D" || type == "Joint3DComponent") {
        if (!registry.all_of<Joint3DComponent>(entity)) return false;
        auto& c = registry.get<Joint3DComponent>(entity);
        if (props.HasMember("spring_stiffness")) c.spring_stiffness = getF("spring_stiffness", c.spring_stiffness);
        if (props.HasMember("spring_damping")) c.spring_damping = getF("spring_damping", c.spring_damping);
        if (props.HasMember("anchor") && props["anchor"].IsArray()) c.anchor = ParseVec3(props["anchor"], c.anchor);
        return true;
    } else if (type == "SoftBody" || type == "SoftBodyComponent") {
        if (!registry.all_of<SoftBodyComponent>(entity)) return false;
        auto& c = registry.get<SoftBodyComponent>(entity);
        if (props.HasMember("stiffness")) c.stiffness = getF("stiffness", c.stiffness);
        if (props.HasMember("damping")) c.damping = getF("damping", c.damping);
        return true;
    } else if (type == "Rope" || type == "RopeComponent") {
        if (!registry.all_of<RopeComponent>(entity)) return false;
        auto& c = registry.get<RopeComponent>(entity);
        if (props.HasMember("segment_length")) c.segment_length = getF("segment_length", c.segment_length);
        return true;
    }
    // ─── 3D Animation modify ─────────────────────────────────────────────────
    else if (type == "Animator3D" || type == "Animator3DComponent") {
        if (!registry.all_of<Animator3DComponent>(entity)) return false;
        auto& c = registry.get<Animator3DComponent>(entity);
        if (props.HasMember("dskel_path") && props["dskel_path"].IsString()) c.dskel_path = props["dskel_path"].GetString();
        if (props.HasMember("danim_path") && props["danim_path"].IsString()) c.danim_path = props["danim_path"].GetString();
        if (props.HasMember("speed")) c.speed = getF("speed", c.speed);
        if (props.HasMember("loop") && props["loop"].IsBool()) c.loop = props["loop"].GetBool();
        if (props.HasMember("enabled") && props["enabled"].IsBool()) c.enabled = props["enabled"].GetBool();
        if (props.HasMember("lock_root_motion") && props["lock_root_motion"].IsBool()) c.lock_root_motion = props["lock_root_motion"].GetBool();
        if (props.HasMember("extract_root_motion") && props["extract_root_motion"].IsBool()) c.extract_root_motion = props["extract_root_motion"].GetBool();
        return true;
    } else if (type == "BoneAttachment" || type == "BoneAttachmentComponent") {
        if (!registry.all_of<BoneAttachmentComponent>(entity)) return false;
        auto& c = registry.get<BoneAttachmentComponent>(entity);
        if (props.HasMember("bone_name") && props["bone_name"].IsString()) { c.bone_name = props["bone_name"].GetString(); c.index_dirty = true; }
        if (props.HasMember("offset_position") && props["offset_position"].IsArray()) c.offset_position = ParseVec3(props["offset_position"], c.offset_position);
        return true;
    }
    // ─── 3D Particle modify ──────────────────────────────────────────────────
    else if (type == "ParticleSystem3D" || type == "ParticleSystem3DComponent") {
        if (!registry.all_of<ParticleSystem3DComponent>(entity)) return false;
        auto& c = registry.get<ParticleSystem3DComponent>(entity);
        if (props.HasMember("emission_rate")) c.emission_rate = getF("emission_rate", c.emission_rate);
        if (props.HasMember("start_speed_min")) c.start_speed_min = getF("start_speed_min", c.start_speed_min);
        if (props.HasMember("start_speed_max")) c.start_speed_max = getF("start_speed_max", c.start_speed_max);
        if (props.HasMember("start_color") && props["start_color"].IsArray()) c.start_color = ParseVec4(props["start_color"]);
        if (props.HasMember("texture_path") && props["texture_path"].IsString()) c.texture_path = props["texture_path"].GetString();
        if (props.HasMember("enabled") && props["enabled"].IsBool()) c.enabled = props["enabled"].GetBool();
        return true;
    }
    // ─── AI modify ───────────────────────────────────────────────────────────
    else if (type == "BehaviorTree" || type == "BehaviorTreeComponent") {
        if (!registry.all_of<BehaviorTreeComponent>(entity)) return false;
        auto& c = registry.get<BehaviorTreeComponent>(entity);
        if (props.HasMember("tree_name") && props["tree_name"].IsString()) c.tree_name = props["tree_name"].GetString();
        if (props.HasMember("auto_restart") && props["auto_restart"].IsBool()) c.auto_restart = props["auto_restart"].GetBool();
        return true;
    } else if (type == "Cutscene" || type == "CutsceneComponent") {
        if (!registry.all_of<CutsceneComponent>(entity)) return false;
        auto& c = registry.get<CutsceneComponent>(entity);
        if (props.HasMember("sequence_name") && props["sequence_name"].IsString()) c.sequence_name = props["sequence_name"].GetString();
        if (props.HasMember("auto_play") && props["auto_play"].IsBool()) c.auto_play = props["auto_play"].GetBool();
        return true;
    } else if (type == "Steering" || type == "SteeringComponent") {
        if (!registry.all_of<SteeringComponent>(entity)) return false;
        auto& c = registry.get<SteeringComponent>(entity);
        if (props.HasMember("max_velocity")) c.max_velocity = getF("max_velocity", c.max_velocity);
        if (props.HasMember("max_force")) c.max_force = getF("max_force", c.max_force);
        return true;
    }
#ifdef DSE_ENABLE_NAVMESH
    else if (type == "NavMeshAgent" || type == "NavMeshAgentComponent") {
        if (!registry.all_of<NavMeshAgentComponent>(entity)) return false;
        auto& c = registry.get<NavMeshAgentComponent>(entity);
        if (props.HasMember("speed")) c.speed = getF("speed", c.speed);
        if (props.HasMember("agent_radius")) c.agent_radius = getF("agent_radius", c.agent_radius);
        if (props.HasMember("agent_height")) c.agent_height = getF("agent_height", c.agent_height);
        if (props.HasMember("destination") && props["destination"].IsArray()) c.destination = ParseVec3(props["destination"], c.destination);
        return true;
    }
#endif
    // ─── Script modify ───────────────────────────────────────────────────────
    else if (type == "Script" || type == "ScriptComponent") {
        if (!registry.all_of<ScriptComponent>(entity)) return false;
        auto& c = registry.get<ScriptComponent>(entity);
        if (props.HasMember("script_path") && props["script_path"].IsString()) c.script_path = props["script_path"].GetString();
        if (props.HasMember("enabled") && props["enabled"].IsBool()) c.enabled = props["enabled"].GetBool();
        return true;
    } else if (type == "LuaScript" || type == "LuaScriptComponent") {
        if (!registry.all_of<LuaScriptComponent>(entity)) return false;
        auto& c = registry.get<LuaScriptComponent>(entity);
        if (props.HasMember("script_path") && props["script_path"].IsString()) c.script_path = props["script_path"].GetString();
        return true;
    } else if (type == "CSharpScript" || type == "CSharpScriptComponent") {
        if (!registry.all_of<CSharpScriptComponent>(entity)) return false;
        auto& c = registry.get<CSharpScriptComponent>(entity);
        if (props.HasMember("class_name") && props["class_name"].IsString()) c.class_name = props["class_name"].GetString();
        if (props.HasMember("enabled") && props["enabled"].IsBool()) c.enabled = props["enabled"].GetBool();
        return true;
    } else if (type == "Blueprint" || type == "BlueprintComponent") {
        if (!registry.all_of<BlueprintComponent>(entity)) return false;
        auto& c = registry.get<BlueprintComponent>(entity);
        if (props.HasMember("blueprint_asset_path") && props["blueprint_asset_path"].IsString()) c.blueprint_asset_path = props["blueprint_asset_path"].GetString();
        if (props.HasMember("enabled") && props["enabled"].IsBool()) c.enabled = props["enabled"].GetBool();
        return true;
    }
    // ─── UI modify ───────────────────────────────────────────────────────────
    else if (type == "UIPanel" || type == "UIPanelComponent") {
        if (!registry.all_of<UIPanelComponent>(entity)) return false;
        auto& c = registry.get<UIPanelComponent>(entity);
        if (props.HasMember("blocks_input") && props["blocks_input"].IsBool()) c.blocks_input = props["blocks_input"].GetBool();
        return true;
    } else if (type == "UIButton" || type == "UIButtonComponent") {
        if (!registry.all_of<UIButtonComponent>(entity)) return false;
        auto& c = registry.get<UIButtonComponent>(entity);
        if (props.HasMember("normal_color") && props["normal_color"].IsArray()) c.normal_color = ParseVec4(props["normal_color"]);
        if (props.HasMember("hover_color") && props["hover_color"].IsArray()) c.hover_color = ParseVec4(props["hover_color"]);
        if (props.HasMember("pressed_color") && props["pressed_color"].IsArray()) c.pressed_color = ParseVec4(props["pressed_color"]);
        return true;
    } else if (type == "UILabel" || type == "UILabelComponent") {
        if (!registry.all_of<UILabelComponent>(entity)) return false;
        auto& c = registry.get<UILabelComponent>(entity);
        if (props.HasMember("text") && props["text"].IsString()) c.text = props["text"].GetString();
        if (props.HasMember("font_size")) c.font_size = getF("font_size", c.font_size);
        if (props.HasMember("color") && props["color"].IsArray()) c.color = ParseVec4(props["color"]);
        return true;
    } else if (type == "UIRichText" || type == "UIRichTextComponent") {
        if (!registry.all_of<UIRichTextComponent>(entity)) return false;
        auto& c = registry.get<UIRichTextComponent>(entity);
        if (props.HasMember("text") && props["text"].IsString()) c.text = props["text"].GetString();
        return true;
    } else if (type == "UIJoystick" || type == "UIJoystickComponent") {
        if (!registry.all_of<UIJoystickComponent>(entity)) return false;
        auto& c = registry.get<UIJoystickComponent>(entity);
        if (props.HasMember("max_radius")) c.max_radius = getF("max_radius", c.max_radius);
        return true;
    } else if (type == "UIAnchor" || type == "UIAnchorComponent") {
        if (!registry.all_of<UIAnchorComponent>(entity)) return false;
        auto& c = registry.get<UIAnchorComponent>(entity);
        if (props.HasMember("anchor") && props["anchor"].IsInt()) c.anchor = props["anchor"].GetInt();
        return true;
    } else if (type == "UIGridLayout" || type == "UIGridLayoutComponent") {
        if (!registry.all_of<UIGridLayoutComponent>(entity)) return false;
        auto& c = registry.get<UIGridLayoutComponent>(entity);
        if (props.HasMember("columns") && props["columns"].IsInt()) c.columns = props["columns"].GetInt();
        if (props.HasMember("cell_size") && props["cell_size"].IsArray()) c.cell_size = ParseVec2(props["cell_size"], c.cell_size);
        if (props.HasMember("spacing") && props["spacing"].IsArray()) c.spacing = ParseVec2(props["spacing"], c.spacing);
        return true;
    } else if (type == "UICanvasScaler" || type == "UICanvasScalerComponent") {
        if (!registry.all_of<UICanvasScalerComponent>(entity)) return false;
        auto& c = registry.get<UICanvasScalerComponent>(entity);
        if (props.HasMember("reference_resolution") && props["reference_resolution"].IsArray()) c.reference_resolution = ParseVec2(props["reference_resolution"], c.reference_resolution);
        return true;
    } else if (type == "UIBoxLayout" || type == "UIBoxLayoutComponent") {
        if (!registry.all_of<UIBoxLayoutComponent>(entity)) return false;
        auto& c = registry.get<UIBoxLayoutComponent>(entity);
        if (props.HasMember("vertical") && props["vertical"].IsBool()) c.vertical = props["vertical"].GetBool();
        if (props.HasMember("spacing")) c.spacing = getF("spacing", c.spacing);
        return true;
    } else if (type == "UIAnimation" || type == "UIAnimationComponent") {
        if (!registry.all_of<UIAnimationComponent>(entity)) return false;
        auto& c = registry.get<UIAnimationComponent>(entity);
        if (props.HasMember("duration")) c.duration = getF("duration", c.duration);
        if (props.HasMember("loop") && props["loop"].IsBool()) c.loop = props["loop"].GetBool();
        return true;
    } else if (type == "UITextInput" || type == "UITextInputComponent") {
        if (!registry.all_of<UITextInputComponent>(entity)) return false;
        auto& c = registry.get<UITextInputComponent>(entity);
        if (props.HasMember("placeholder") && props["placeholder"].IsString()) c.placeholder = props["placeholder"].GetString();
        if (props.HasMember("is_password") && props["is_password"].IsBool()) c.is_password = props["is_password"].GetBool();
        return true;
    } else if (type == "UIScrollView" || type == "UIScrollViewComponent") {
        if (!registry.all_of<UIScrollViewComponent>(entity)) return false;
        auto& c = registry.get<UIScrollViewComponent>(entity);
        if (props.HasMember("horizontal") && props["horizontal"].IsBool()) c.horizontal = props["horizontal"].GetBool();
        if (props.HasMember("vertical") && props["vertical"].IsBool()) c.vertical = props["vertical"].GetBool();
        return true;
    } else if (type == "UISlider" || type == "UISliderComponent") {
        if (!registry.all_of<UISliderComponent>(entity)) return false;
        auto& c = registry.get<UISliderComponent>(entity);
        if (props.HasMember("value")) c.value = getF("value", c.value);
        if (props.HasMember("min_value")) c.min_value = getF("min_value", c.min_value);
        if (props.HasMember("max_value")) c.max_value = getF("max_value", c.max_value);
        return true;
    } else if (type == "UIToggle" || type == "UIToggleComponent") {
        if (!registry.all_of<UIToggleComponent>(entity)) return false;
        auto& c = registry.get<UIToggleComponent>(entity);
        if (props.HasMember("is_on") && props["is_on"].IsBool()) c.is_on = props["is_on"].GetBool();
        return true;
    } else if (type == "UIProgressBar" || type == "UIProgressBarComponent") {
        if (!registry.all_of<UIProgressBarComponent>(entity)) return false;
        auto& c = registry.get<UIProgressBarComponent>(entity);
        if (props.HasMember("value")) c.value = getF("value", c.value);
        if (props.HasMember("max_value")) c.max_value = getF("max_value", c.max_value);
        return true;
    }
    // ─── 2D modify ───────────────────────────────────────────────────────────
    else if (type == "SpriteRenderer" || type == "SpriteRendererComponent") {
        if (!registry.all_of<SpriteRendererComponent>(entity)) return false;
        auto& c = registry.get<SpriteRendererComponent>(entity);
        if (props.HasMember("shader_variant") && props["shader_variant"].IsString()) c.shader_variant = props["shader_variant"].GetString();
        if (props.HasMember("color") && props["color"].IsArray()) c.color = ParseVec4(props["color"]);
        if (props.HasMember("visible") && props["visible"].IsBool()) c.visible = props["visible"].GetBool();
        return true;
    } else if (type == "Light2D" || type == "Light2DComponent") {
        if (!registry.all_of<Light2DComponent>(entity)) return false;
        auto& c = registry.get<Light2DComponent>(entity);
        if (props.HasMember("intensity")) c.intensity = getF("intensity", c.intensity);
        if (props.HasMember("range")) c.range = getF("range", c.range);
        if (props.HasMember("color") && props["color"].IsArray()) c.color = ParseVec3(props["color"], c.color);
        return true;
    } else if (type == "TrailRenderer2D" || type == "TrailRenderer2DComponent") {
        if (!registry.all_of<TrailRenderer2DComponent>(entity)) return false;
        auto& c = registry.get<TrailRenderer2DComponent>(entity);
        if (props.HasMember("lifetime")) c.lifetime = getF("lifetime", c.lifetime);
        if (props.HasMember("start_width")) c.start_width = getF("start_width", c.start_width);
        if (props.HasMember("end_width")) c.end_width = getF("end_width", c.end_width);
        return true;
    } else if (type == "LineRenderer2D" || type == "LineRenderer2DComponent") {
        if (!registry.all_of<LineRenderer2DComponent>(entity)) return false;
        auto& c = registry.get<LineRenderer2DComponent>(entity);
        if (props.HasMember("width")) c.width = getF("width", c.width);
        return true;
    } else if (type == "CameraController2D" || type == "CameraController2DComponent") {
        if (!registry.all_of<CameraController2DComponent>(entity)) return false;
        auto& c = registry.get<CameraController2DComponent>(entity);
        if (props.HasMember("target_zoom")) c.target_zoom = getF("target_zoom", c.target_zoom);
        return true;
    } else if (type == "AudioSpatial2D" || type == "AudioSpatial2DComponent") {
        if (!registry.all_of<AudioSpatial2DComponent>(entity)) return false;
        auto& c = registry.get<AudioSpatial2DComponent>(entity);
        if (props.HasMember("min_distance")) c.min_distance = getF("min_distance", c.min_distance);
        if (props.HasMember("max_distance")) c.max_distance = getF("max_distance", c.max_distance);
        return true;
    }
    return false;
}

// ─── Tool: dsengine_entity_create ───────────────────────────────────────────

static JsonRpcResponse HandleEntityCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("name") || !params["name"].IsString()) {
        return MakeToolError(-32602, "Missing required param: name");
    }

    World& world = engine.pipeline()->world();
    auto& registry = world.registry();
    auto entity = registry.create();

    // Name
    auto& name_comp = registry.emplace<EditorNameComponent>(entity);
    name_comp.name = params["name"].GetString();

    // Transform
    TransformComponent transform;
    if (params.HasMember("position") && params["position"].IsArray() && params["position"].Size() >= 3) {
        transform.position = ParseVec3(params["position"]);
    }
    if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
        transform.scale = ParseVec3(params["scale"], glm::vec3(1.0f));
    }
    if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
        const auto& r = params["rotation"];
        transform.rotation = glm::quat(glm::vec3(
            glm::radians(r[0].GetFloat()),
            glm::radians(r[1].GetFloat()),
            glm::radians(r[2].GetFloat())));
    }
    registry.emplace<TransformComponent>(entity, transform);

    // mesh 快捷参数 → 自动添加 MeshRendererComponent
    if (params.HasMember("mesh") && params["mesh"].IsString()) {
        if (!registry.all_of<MeshRendererComponent>(entity)) {
            auto& mr = registry.emplace<MeshRendererComponent>(entity);
            mr.mesh_path = params["mesh"].GetString();
            mr.shader_variant = "MESH_PBR";
            if (params.HasMember("color") && params["color"].IsArray())
                mr.color = ParseVec4(params["color"]);
        }
    }

    // components 数组 → 批量添加
    if (params.HasMember("components") && params["components"].IsArray()) {
        for (auto& comp : params["components"].GetArray()) {
            std::string comp_type;
            const rapidjson::Value* comp_props = nullptr;

            if (comp.IsString()) {
                comp_type = comp.GetString();
            } else if (comp.IsObject()) {
                if (comp.HasMember("type") && comp["type"].IsString()) {
                    comp_type = comp["type"].GetString();
                }
                if (comp.HasMember("properties") && comp["properties"].IsObject()) {
                    comp_props = &comp["properties"];
                }
            }

            if (!comp_type.empty()) {
                AddComponentByType(registry, entity, comp_type, comp_props);
            }
        }
    }

    // 构建返回值：列出实际添加的组件
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("name", rapidjson::Value(name_comp.name.c_str(), alloc), alloc);

    rapidjson::Value added_comps(rapidjson::kArrayType);
    added_comps.PushBack("Transform", alloc);
    if (registry.all_of<MeshRendererComponent>(entity))     added_comps.PushBack("MeshRenderer", alloc);
    if (registry.all_of<Camera3DComponent>(entity))          added_comps.PushBack("Camera3D", alloc);
    if (registry.all_of<DirectionalLight3DComponent>(entity)) added_comps.PushBack("DirectionalLight", alloc);
    if (registry.all_of<PointLightComponent>(entity))        added_comps.PushBack("PointLight", alloc);
    if (registry.all_of<SpotLightComponent>(entity))         added_comps.PushBack("SpotLight", alloc);
    if (registry.all_of<RigidBody3DComponent>(entity))       added_comps.PushBack("RigidBody3D", alloc);
    if (registry.all_of<BoxCollider3DComponent>(entity))     added_comps.PushBack("BoxCollider3D", alloc);
    if (registry.all_of<SphereCollider3DComponent>(entity))  added_comps.PushBack("SphereCollider3D", alloc);
    if (registry.all_of<AudioSourceComponent>(entity))       added_comps.PushBack("AudioSource", alloc);
    if (registry.all_of<AudioListenerComponent>(entity))     added_comps.PushBack("AudioListener", alloc);
    if (registry.all_of<SkyLightComponent>(entity))          added_comps.PushBack("SkyLight", alloc);
    if (registry.all_of<SkyboxComponent>(entity))            added_comps.PushBack("Skybox", alloc);
    if (registry.all_of<PostProcessComponent>(entity))       added_comps.PushBack("PostProcess", alloc);
    result.AddMember("components", added_comps, alloc);

    // Undo/Redo（全组件保真，与层级面板 Create 收敛后一致）：
    // do/redo —— 首次入栈实体已存在则跳过，撤销后重做用快照重建（新 handle）；
    // undo —— 销毁当前实体。用 shared 状态跨重建跟踪 handle。
    {
        auto& w = world;
        auto& reg = registry;
        auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg, entity));
        auto tracked = std::make_shared<entt::entity>(entity);
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Create Entity (RPC)",
            [tracked, snapshot, &w, &reg]() {
                if (*tracked != entt::null && reg.valid(*tracked)) return;
                *tracked = snapshot->Restore(w, reg);
            },
            [tracked, &w, &reg]() {
                if (*tracked != entt::null && reg.valid(*tracked)) {
                    w.DestroyEntity(*tracked);
                    *tracked = entt::null;
                }
            }
        ));
    }

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_delete ───────────────────────────────────────────

static JsonRpcResponse HandleEntityDelete(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    // 全组件快照（撤销可完整还原，含 parent/sibling）；do/redo 销毁、undo 还原。
    // 不在此处直接 destroy：交由命令 do-lambda 在入栈时执行（统一一条销毁路径）。
    {
        auto& w = engine.pipeline()->world();
        auto& reg = registry;
        auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg, entity));
        auto tracked = std::make_shared<entt::entity>(entity);
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Delete Entity (RPC)",
            [tracked, &w, &reg]() {
                if (*tracked != entt::null && reg.valid(*tracked)) {
                    w.DestroyEntity(*tracked);
                    *tracked = entt::null;
                }
            },
            [tracked, snapshot, &w, &reg]() {
                *tracked = snapshot->Restore(w, reg);
            }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("deleted", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_batch_delete ─────────────────────────────────────

static JsonRpcResponse HandleEntityBatchDelete(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_ids") || !params["entity_ids"].IsArray()) {
        return MakeToolError(-32602, "Missing required param: entity_ids (array of uint)");
    }

    auto& registry = engine.pipeline()->world().registry();
    auto& w = engine.pipeline()->world();

    auto compound = std::make_unique<CompoundCommand>("Batch Delete");
    std::vector<uint32_t> deleted_ids;

    for (const auto& v : params["entity_ids"].GetArray()) {
        if (!v.IsUint()) continue;
        auto entity = static_cast<entt::entity>(v.GetUint());
        if (!registry.valid(entity)) continue;

        deleted_ids.push_back(static_cast<uint32_t>(entity));

        // 全组件快照；do/redo 销毁、undo 还原。销毁交由 compound 入栈时的 do 执行。
        auto& reg = registry;
        auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg, entity));
        auto tracked = std::make_shared<entt::entity>(entity);
        compound->AddCommand(std::make_unique<LambdaCommand>(
            "Delete Entity",
            [tracked, &w, &reg]() {
                if (*tracked != entt::null && reg.valid(*tracked)) {
                    w.DestroyEntity(*tracked);
                    *tracked = entt::null;
                }
            },
            [tracked, snapshot, &w, &reg]() {
                *tracked = snapshot->Restore(w, reg);
            }
        ));
    }

    if (!compound->IsEmpty())
        GetUndoRedoManager().Execute(std::move(compound), false);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("deleted_count", rapidjson::Value(static_cast<int>(deleted_ids.size())), alloc);
    rapidjson::Value ids_arr(rapidjson::kArrayType);
    for (auto id : deleted_ids) ids_arr.PushBack(id, alloc);
    result.AddMember("deleted_ids", ids_arr, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_script_create ───────────────────────────────────────────

static JsonRpcResponse HandleScriptCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("path") || !params["path"].IsString() ||
        !params.HasMember("content") || !params["content"].IsString()) {
        return MakeToolError(-32602, "Missing required params: path, content");
    }

    std::filesystem::path file_path = params["path"].GetString();
    // 安全检查：不允许 .. 路径逃逸
    if (file_path.string().find("..") != std::string::npos) {
        return MakeToolError(-32602, "Path must not contain '..'");
    }

    std::filesystem::create_directories(file_path.parent_path());
    std::ofstream ofs(file_path, std::ios::trunc);
    if (!ofs.is_open()) {
        return MakeToolError(-32603, "Failed to write file: " + file_path.string());
    }
    ofs << params["content"].GetString();
    ofs.close();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(file_path.string().c_str(), alloc), alloc);
    result.AddMember("written", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_get_state ────────────────────────────────────────

static JsonRpcResponse HandleEditorGetState(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    const char* state_str =
        dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
        dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);

    result.AddMember("entity_count", CountValidEntities(registry), alloc);

    auto* am = engine.asset_manager();
    if (am) {
        std::string dr = am->GetDataRoot();
        result.AddMember("data_root", rapidjson::Value(dr.c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_ping ────────────────────────────────────────────────────

static JsonRpcResponse HandlePing(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("pong", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_play ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorPlay(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    if (dse::editor::GetEditorState() != dse::editor::EditorState::Edit) {
        return MakeToolError(-32603, "Already in play/pause mode");
    }

    auto& registry = engine.pipeline()->world().registry();
    dse::editor::EnterPlayMode(registry);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("editor_state", "play", alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_stop ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorStop(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    if (dse::editor::GetEditorState() == dse::editor::EditorState::Edit) {
        return MakeToolError(-32603, "Not in play mode");
    }

    auto& registry = engine.pipeline()->world().registry();
    entt::entity dummy = entt::null;
    dse::editor::ExitPlayMode(registry, dummy);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("editor_state", "edit", alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_undo ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorUndo(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& mgr = GetUndoRedoManager();
    bool ok = mgr.CanUndo() ? mgr.Undo() : false;

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", ok, alloc);
    if (mgr.CanUndo()) {
        result.AddMember("next_undo",
            rapidjson::Value(mgr.GetUndoDescription().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_redo ─────────────────────────────────────────────

static JsonRpcResponse HandleEditorRedo(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& mgr = GetUndoRedoManager();
    bool ok = mgr.CanRedo() ? mgr.Redo() : false;

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", ok, alloc);
    if (mgr.CanRedo()) {
        result.AddMember("next_redo",
            rapidjson::Value(mgr.GetRedoDescription().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_modify ───────────────────────────────────────────

static JsonRpcResponse HandleEntityModify(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    // name
    if (params.HasMember("name") && params["name"].IsString()) {
        std::string old_name;
        if (registry.all_of<EditorNameComponent>(entity))
            old_name = registry.get<EditorNameComponent>(entity).name;
        std::string new_name = params["name"].GetString();
        if (registry.all_of<EditorNameComponent>(entity)) {
            registry.get<EditorNameComponent>(entity).name = new_name;
        } else {
            registry.emplace<EditorNameComponent>(entity).name = new_name;
        }
        auto& reg = registry;
        GetUndoRedoManager().Execute(
            std::make_unique<PropertyChangeCommand<std::string>>(
                "Rename Entity",
                old_name, new_name,
                [&reg, entity](const std::string& v) {
                    if (reg.valid(entity) && reg.all_of<EditorNameComponent>(entity))
                        reg.get<EditorNameComponent>(entity).name = v;
                }),
            false);
    }

    // position / rotation / scale
    if (registry.all_of<TransformComponent>(entity)) {
        auto& t = registry.get<TransformComponent>(entity);
        if (params.HasMember("position") && params["position"].IsArray() && params["position"].Size() >= 3) {
            const auto& p = params["position"];
            t.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
            t.dirty = true;
        }
        if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
            const auto& r = params["rotation"];
            t.rotation = glm::quat(glm::vec3(
                glm::radians(r[0].GetFloat()),
                glm::radians(r[1].GetFloat()),
                glm::radians(r[2].GetFloat())));
            t.dirty = true;
        }
        if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
            const auto& s = params["scale"];
            t.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
            t.dirty = true;
        }
    } else if (params.HasMember("position") || params.HasMember("rotation") || params.HasMember("scale")) {
        TransformComponent t;
        if (params.HasMember("position") && params["position"].IsArray() && params["position"].Size() >= 3) {
            const auto& p = params["position"];
            t.position = glm::vec3(p[0].GetFloat(), p[1].GetFloat(), p[2].GetFloat());
        }
        if (params.HasMember("rotation") && params["rotation"].IsArray() && params["rotation"].Size() >= 3) {
            const auto& r = params["rotation"];
            t.rotation = glm::quat(glm::vec3(
                glm::radians(r[0].GetFloat()),
                glm::radians(r[1].GetFloat()),
                glm::radians(r[2].GetFloat())));
        }
        if (params.HasMember("scale") && params["scale"].IsArray() && params["scale"].Size() >= 3) {
            const auto& s = params["scale"];
            t.scale = glm::vec3(s[0].GetFloat(), s[1].GetFloat(), s[2].GetFloat());
        }
        registry.emplace<TransformComponent>(entity, t);
    }

    // modify_component: 修改已存在组件的属性
    rapidjson::Value modified_comps(rapidjson::kArrayType);
    rapidjson::Document::AllocatorType temp_alloc;  // 临时分配器

    if (params.HasMember("modify_component") && params["modify_component"].IsObject()) {
        const auto& mc = params["modify_component"];
        if (mc.HasMember("type") && mc["type"].IsString() &&
            mc.HasMember("properties") && mc["properties"].IsObject()) {
            std::string ctype = mc["type"].GetString();
            if (ModifyComponentProperties(registry, entity, ctype, mc["properties"])) {
                modified_comps.PushBack(rapidjson::Value(ctype.c_str(), temp_alloc), temp_alloc);
            }
        }
    }
    if (params.HasMember("modify_components") && params["modify_components"].IsArray()) {
        for (auto& item : params["modify_components"].GetArray()) {
            if (item.IsObject() && item.HasMember("type") && item["type"].IsString() &&
                item.HasMember("properties") && item["properties"].IsObject()) {
                std::string ctype = item["type"].GetString();
                if (ModifyComponentProperties(registry, entity, ctype, item["properties"])) {
                    modified_comps.PushBack(rapidjson::Value(ctype.c_str(), temp_alloc), temp_alloc);
                }
            }
        }
    }

    // add_components: 批量添加组件（复用 AddComponentByType）
    std::vector<std::string> added_list;
    if (params.HasMember("add_components") && params["add_components"].IsArray()) {
        for (auto& item : params["add_components"].GetArray()) {
            std::string ctype;
            const rapidjson::Value* props = nullptr;
            if (item.IsString()) {
                ctype = item.GetString();
            } else if (item.IsObject() && item.HasMember("type") && item["type"].IsString()) {
                ctype = item["type"].GetString();
                if (item.HasMember("properties") && item["properties"].IsObject())
                    props = &item["properties"];
            }
            if (!ctype.empty()) {
                AddComponentByType(registry, entity, ctype, props);
                added_list.push_back(ctype);
            }
        }
    }

    // remove_components: 批量移除组件（复用 RemoveComponentByType）
    std::vector<std::string> removed_list;
    if (params.HasMember("remove_components") && params["remove_components"].IsArray()) {
        for (auto& item : params["remove_components"].GetArray()) {
            if (item.IsString()) {
                std::string ctype = item.GetString();
                if (RemoveComponentByType(registry, entity, ctype)) {
                    removed_list.push_back(ctype);
                }
            }
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("modified", rapidjson::Value(true), alloc);
    if (modified_comps.Size() > 0) {
        rapidjson::Value comps_copy(rapidjson::kArrayType);
        for (auto& v : modified_comps.GetArray()) {
            comps_copy.PushBack(rapidjson::Value(v.GetString(), alloc), alloc);
        }
        result.AddMember("modified_components", comps_copy, alloc);
    }
    if (!added_list.empty()) {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (auto& s : added_list) arr.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
        result.AddMember("added_components", arr, alloc);
    }
    if (!removed_list.empty()) {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (auto& s : removed_list) arr.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
        result.AddMember("removed_components", arr, alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_add_component ────────────────────────────────────

static JsonRpcResponse HandleEntityAddComponent(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("type") || !params["type"].IsString()) {
        return MakeToolError(-32602, "Missing required param: type (string)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string comp_type = params["type"].GetString();
    const rapidjson::Value* props = nullptr;
    if (params.HasMember("properties") && params["properties"].IsObject()) {
        props = &params["properties"];
    }

    AddComponentByType(registry, entity, comp_type, props);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("component", rapidjson::Value(comp_type.c_str(), alloc), alloc);
    result.AddMember("added", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_remove_component ─────────────────────────────────

static bool RemoveComponentByType(entt::registry& registry, entt::entity entity,
                                   const std::string& type) {
    if (type == "MeshRenderer" || type == "MeshRendererComponent") {
        if (registry.all_of<MeshRendererComponent>(entity)) { registry.remove<MeshRendererComponent>(entity); return true; }
    } else if (type == "Camera3D" || type == "Camera3DComponent") {
        if (registry.all_of<Camera3DComponent>(entity)) { registry.remove<Camera3DComponent>(entity); return true; }
    } else if (type == "DirectionalLight" || type == "DirectionalLight3DComponent") {
        if (registry.all_of<DirectionalLight3DComponent>(entity)) { registry.remove<DirectionalLight3DComponent>(entity); return true; }
    } else if (type == "PointLight" || type == "PointLightComponent") {
        if (registry.all_of<PointLightComponent>(entity)) { registry.remove<PointLightComponent>(entity); return true; }
    } else if (type == "SpotLight" || type == "SpotLightComponent") {
        if (registry.all_of<SpotLightComponent>(entity)) { registry.remove<SpotLightComponent>(entity); return true; }
    } else if (type == "RigidBody3D" || type == "RigidBody3DComponent") {
        if (registry.all_of<RigidBody3DComponent>(entity)) { registry.remove<RigidBody3DComponent>(entity); return true; }
    } else if (type == "BoxCollider3D" || type == "BoxCollider3DComponent") {
        if (registry.all_of<BoxCollider3DComponent>(entity)) { registry.remove<BoxCollider3DComponent>(entity); return true; }
    } else if (type == "SphereCollider3D" || type == "SphereCollider3DComponent") {
        if (registry.all_of<SphereCollider3DComponent>(entity)) { registry.remove<SphereCollider3DComponent>(entity); return true; }
    } else if (type == "AudioSource" || type == "AudioSourceComponent") {
        if (registry.all_of<AudioSourceComponent>(entity)) { registry.remove<AudioSourceComponent>(entity); return true; }
    } else if (type == "AudioListener" || type == "AudioListenerComponent") {
        if (registry.all_of<AudioListenerComponent>(entity)) { registry.remove<AudioListenerComponent>(entity); return true; }
    } else if (type == "SkyLight" || type == "SkyLightComponent") {
        if (registry.all_of<SkyLightComponent>(entity)) { registry.remove<SkyLightComponent>(entity); return true; }
    } else if (type == "Skybox" || type == "SkyboxComponent") {
        if (registry.all_of<SkyboxComponent>(entity)) { registry.remove<SkyboxComponent>(entity); return true; }
    } else if (type == "PostProcess" || type == "PostProcessComponent") {
        if (registry.all_of<PostProcessComponent>(entity)) { registry.remove<PostProcessComponent>(entity); return true; }
    }
    // ─── 3D Render (extended) remove ─────────────────────────────────────────
    else if (type == "Decal" || type == "DecalComponent") {
        if (registry.all_of<DecalComponent>(entity)) { registry.remove<DecalComponent>(entity); return true; }
    } else if (type == "Water" || type == "WaterComponent") {
        if (registry.all_of<WaterComponent>(entity)) { registry.remove<WaterComponent>(entity); return true; }
    } else if (type == "Terrain" || type == "TerrainComponent") {
        if (registry.all_of<TerrainComponent>(entity)) { registry.remove<TerrainComponent>(entity); return true; }
    } else if (type == "Grass" || type == "GrassComponent") {
        if (registry.all_of<GrassComponent>(entity)) { registry.remove<GrassComponent>(entity); return true; }
    } else if (type == "LODGroup" || type == "LODGroupComponent") {
        if (registry.all_of<LODGroupComponent>(entity)) { registry.remove<LODGroupComponent>(entity); return true; }
    } else if (type == "LightProbe" || type == "LightProbeComponent") {
        if (registry.all_of<LightProbeComponent>(entity)) { registry.remove<LightProbeComponent>(entity); return true; }
    } else if (type == "ReflectionProbe" || type == "ReflectionProbeComponent") {
        if (registry.all_of<ReflectionProbeComponent>(entity)) { registry.remove<ReflectionProbeComponent>(entity); return true; }
    } else if (type == "GIProbeVolume" || type == "GIProbeVolumeComponent") {
        if (registry.all_of<GIProbeVolumeComponent>(entity)) { registry.remove<GIProbeVolumeComponent>(entity); return true; }
    } else if (type == "Impostor" || type == "ImpostorComponent") {
        if (registry.all_of<ImpostorComponent>(entity)) { registry.remove<ImpostorComponent>(entity); return true; }
    } else if (type == "SubScene" || type == "SubSceneComponent") {
        if (registry.all_of<SubSceneComponent>(entity)) { registry.remove<SubSceneComponent>(entity); return true; }
    } else if (type == "FreeCameraController" || type == "FreeCameraControllerComponent") {
        if (registry.all_of<FreeCameraControllerComponent>(entity)) { registry.remove<FreeCameraControllerComponent>(entity); return true; }
    }
    // ─── 3D Physics (extended) remove ────────────────────────────────────────
    else if (type == "CapsuleCollider3D" || type == "CapsuleCollider3DComponent") {
        if (registry.all_of<CapsuleCollider3DComponent>(entity)) { registry.remove<CapsuleCollider3DComponent>(entity); return true; }
    } else if (type == "MeshCollider3D" || type == "MeshCollider3DComponent") {
        if (registry.all_of<MeshCollider3DComponent>(entity)) { registry.remove<MeshCollider3DComponent>(entity); return true; }
    } else if (type == "CharacterController3D" || type == "CharacterController3DComponent") {
        if (registry.all_of<CharacterController3DComponent>(entity)) { registry.remove<CharacterController3DComponent>(entity); return true; }
    } else if (type == "Joint3D" || type == "Joint3DComponent") {
        if (registry.all_of<Joint3DComponent>(entity)) { registry.remove<Joint3DComponent>(entity); return true; }
    } else if (type == "SoftBody" || type == "SoftBodyComponent") {
        if (registry.all_of<SoftBodyComponent>(entity)) { registry.remove<SoftBodyComponent>(entity); return true; }
    } else if (type == "Rope" || type == "RopeComponent") {
        if (registry.all_of<RopeComponent>(entity)) { registry.remove<RopeComponent>(entity); return true; }
    }
#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
    else if (type == "Ragdoll" || type == "RagdollComponent") {
        if (registry.all_of<RagdollComponent>(entity)) { registry.remove<RagdollComponent>(entity); return true; }
    } else if (type == "Vehicle" || type == "VehicleComponent") {
        if (registry.all_of<VehicleComponent>(entity)) { registry.remove<VehicleComponent>(entity); return true; }
    } else if (type == "Buoyancy" || type == "BuoyancyComponent") {
        if (registry.all_of<BuoyancyComponent>(entity)) { registry.remove<BuoyancyComponent>(entity); return true; }
    }
#endif
    // ─── 3D Animation remove ─────────────────────────────────────────────────
    else if (type == "Animator3D" || type == "Animator3DComponent") {
        if (registry.all_of<Animator3DComponent>(entity)) { registry.remove<Animator3DComponent>(entity); return true; }
    } else if (type == "BoneAttachment" || type == "BoneAttachmentComponent") {
        if (registry.all_of<BoneAttachmentComponent>(entity)) { registry.remove<BoneAttachmentComponent>(entity); return true; }
    }
    // ─── 3D Particle remove ──────────────────────────────────────────────────
    else if (type == "ParticleSystem3D" || type == "ParticleSystem3DComponent") {
        if (registry.all_of<ParticleSystem3DComponent>(entity)) { registry.remove<ParticleSystem3DComponent>(entity); return true; }
    }
    // ─── AI remove ───────────────────────────────────────────────────────────
    else if (type == "BehaviorTree" || type == "BehaviorTreeComponent") {
        if (registry.all_of<BehaviorTreeComponent>(entity)) { registry.remove<BehaviorTreeComponent>(entity); return true; }
    } else if (type == "Cutscene" || type == "CutsceneComponent") {
        if (registry.all_of<CutsceneComponent>(entity)) { registry.remove<CutsceneComponent>(entity); return true; }
    } else if (type == "Steering" || type == "SteeringComponent") {
        if (registry.all_of<SteeringComponent>(entity)) { registry.remove<SteeringComponent>(entity); return true; }
    }
#ifdef DSE_ENABLE_NAVMESH
    else if (type == "NavMeshAgent" || type == "NavMeshAgentComponent") {
        if (registry.all_of<NavMeshAgentComponent>(entity)) { registry.remove<NavMeshAgentComponent>(entity); return true; }
    }
#endif
    // ─── Script remove ───────────────────────────────────────────────────────
    else if (type == "Script" || type == "ScriptComponent") {
        if (registry.all_of<ScriptComponent>(entity)) { registry.remove<ScriptComponent>(entity); return true; }
    } else if (type == "LuaScript" || type == "LuaScriptComponent") {
        if (registry.all_of<LuaScriptComponent>(entity)) { registry.remove<LuaScriptComponent>(entity); return true; }
    } else if (type == "CSharpScript" || type == "CSharpScriptComponent") {
        if (registry.all_of<CSharpScriptComponent>(entity)) { registry.remove<CSharpScriptComponent>(entity); return true; }
    } else if (type == "Blueprint" || type == "BlueprintComponent") {
        if (registry.all_of<BlueprintComponent>(entity)) { registry.remove<BlueprintComponent>(entity); return true; }
    }
    // ─── UI remove ───────────────────────────────────────────────────────────
    else if (type == "UIPanel" || type == "UIPanelComponent") {
        if (registry.all_of<UIPanelComponent>(entity)) { registry.remove<UIPanelComponent>(entity); return true; }
    } else if (type == "UIButton" || type == "UIButtonComponent") {
        if (registry.all_of<UIButtonComponent>(entity)) { registry.remove<UIButtonComponent>(entity); return true; }
    } else if (type == "UILabel" || type == "UILabelComponent") {
        if (registry.all_of<UILabelComponent>(entity)) { registry.remove<UILabelComponent>(entity); return true; }
    } else if (type == "UIMask" || type == "UIMaskComponent") {
        if (registry.all_of<UIMaskComponent>(entity)) { registry.remove<UIMaskComponent>(entity); return true; }
    } else if (type == "UIRichText" || type == "UIRichTextComponent") {
        if (registry.all_of<UIRichTextComponent>(entity)) { registry.remove<UIRichTextComponent>(entity); return true; }
    } else if (type == "UIJoystick" || type == "UIJoystickComponent") {
        if (registry.all_of<UIJoystickComponent>(entity)) { registry.remove<UIJoystickComponent>(entity); return true; }
    } else if (type == "UIAnchor" || type == "UIAnchorComponent") {
        if (registry.all_of<UIAnchorComponent>(entity)) { registry.remove<UIAnchorComponent>(entity); return true; }
    } else if (type == "UIGridLayout" || type == "UIGridLayoutComponent") {
        if (registry.all_of<UIGridLayoutComponent>(entity)) { registry.remove<UIGridLayoutComponent>(entity); return true; }
    } else if (type == "UICanvasScaler" || type == "UICanvasScalerComponent") {
        if (registry.all_of<UICanvasScalerComponent>(entity)) { registry.remove<UICanvasScalerComponent>(entity); return true; }
    } else if (type == "UIBoxLayout" || type == "UIBoxLayoutComponent") {
        if (registry.all_of<UIBoxLayoutComponent>(entity)) { registry.remove<UIBoxLayoutComponent>(entity); return true; }
    } else if (type == "UIContentSizeFitter" || type == "UIContentSizeFitterComponent") {
        if (registry.all_of<UIContentSizeFitterComponent>(entity)) { registry.remove<UIContentSizeFitterComponent>(entity); return true; }
    } else if (type == "UIAnimation" || type == "UIAnimationComponent") {
        if (registry.all_of<UIAnimationComponent>(entity)) { registry.remove<UIAnimationComponent>(entity); return true; }
    } else if (type == "UITextInput" || type == "UITextInputComponent") {
        if (registry.all_of<UITextInputComponent>(entity)) { registry.remove<UITextInputComponent>(entity); return true; }
    } else if (type == "UIScrollView" || type == "UIScrollViewComponent") {
        if (registry.all_of<UIScrollViewComponent>(entity)) { registry.remove<UIScrollViewComponent>(entity); return true; }
    } else if (type == "UISlider" || type == "UISliderComponent") {
        if (registry.all_of<UISliderComponent>(entity)) { registry.remove<UISliderComponent>(entity); return true; }
    } else if (type == "UIToggle" || type == "UIToggleComponent") {
        if (registry.all_of<UIToggleComponent>(entity)) { registry.remove<UIToggleComponent>(entity); return true; }
    } else if (type == "UIProgressBar" || type == "UIProgressBarComponent") {
        if (registry.all_of<UIProgressBarComponent>(entity)) { registry.remove<UIProgressBarComponent>(entity); return true; }
    } else if (type == "UIDropdown" || type == "UIDropdownComponent") {
        if (registry.all_of<UIDropdownComponent>(entity)) { registry.remove<UIDropdownComponent>(entity); return true; }
    } else if (type == "UIRenderer" || type == "UIRendererComponent") {
        if (registry.all_of<UIRendererComponent>(entity)) { registry.remove<UIRendererComponent>(entity); return true; }
    }
    // ─── 2D remove ───────────────────────────────────────────────────────────
    else if (type == "SpriteRenderer" || type == "SpriteRendererComponent") {
        if (registry.all_of<SpriteRendererComponent>(entity)) { registry.remove<SpriteRendererComponent>(entity); return true; }
    } else if (type == "Light2D" || type == "Light2DComponent") {
        if (registry.all_of<Light2DComponent>(entity)) { registry.remove<Light2DComponent>(entity); return true; }
    } else if (type == "TrailRenderer2D" || type == "TrailRenderer2DComponent") {
        if (registry.all_of<TrailRenderer2DComponent>(entity)) { registry.remove<TrailRenderer2DComponent>(entity); return true; }
    } else if (type == "LineRenderer2D" || type == "LineRenderer2DComponent") {
        if (registry.all_of<LineRenderer2DComponent>(entity)) { registry.remove<LineRenderer2DComponent>(entity); return true; }
    } else if (type == "Parallax" || type == "ParallaxComponent") {
        if (registry.all_of<ParallaxComponent>(entity)) { registry.remove<ParallaxComponent>(entity); return true; }
    } else if (type == "CameraController2D" || type == "CameraController2DComponent") {
        if (registry.all_of<CameraController2DComponent>(entity)) { registry.remove<CameraController2DComponent>(entity); return true; }
    } else if (type == "AudioSpatial2D" || type == "AudioSpatial2DComponent") {
        if (registry.all_of<AudioSpatial2DComponent>(entity)) { registry.remove<AudioSpatial2DComponent>(entity); return true; }
    } else if (type == "AudioListener2D" || type == "AudioListener2DComponent") {
        if (registry.all_of<AudioListener2DComponent>(entity)) { registry.remove<AudioListener2DComponent>(entity); return true; }
    }
    return false;
}

static JsonRpcResponse HandleEntityRemoveComponent(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("type") || !params["type"].IsString()) {
        return MakeToolError(-32602, "Missing required param: type (string)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string comp_type = params["type"].GetString();
    bool removed = RemoveComponentByType(registry, entity, comp_type);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("component", rapidjson::Value(comp_type.c_str(), alloc), alloc);
    result.AddMember("removed", rapidjson::Value(removed), alloc);
    if (!removed) {
        result.AddMember("reason", "Component not found on entity", alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Helper: 收集实体上的组件列表 ───────────────────────────────────────────

static void CollectEntityComponents(entt::registry& registry, entt::entity entity,
                                     rapidjson::Value& arr, rapidjson::Document::AllocatorType& alloc,
                                     bool include_properties) {
    auto addComp = [&](const char* type_name) {
        if (include_properties) {
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", rapidjson::Value(type_name, alloc), alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value(type_name, alloc), alloc);
        }
    };

    if (registry.all_of<TransformComponent>(entity)) {
        if (include_properties) {
            const auto& t = registry.get<TransformComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Transform", alloc);
            rapidjson::Value pos(rapidjson::kArrayType);
            pos.PushBack(t.position.x, alloc).PushBack(t.position.y, alloc).PushBack(t.position.z, alloc);
            comp.AddMember("position", pos, alloc);
            rapidjson::Value rot(rapidjson::kArrayType);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(t.rotation));
            rot.PushBack(euler.x, alloc).PushBack(euler.y, alloc).PushBack(euler.z, alloc);
            comp.AddMember("rotation", rot, alloc);
            rapidjson::Value scl(rapidjson::kArrayType);
            scl.PushBack(t.scale.x, alloc).PushBack(t.scale.y, alloc).PushBack(t.scale.z, alloc);
            comp.AddMember("scale", scl, alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value("Transform", alloc), alloc);
        }
    }
    if (registry.all_of<MeshRendererComponent>(entity)) {
        if (include_properties) {
            const auto& mr = registry.get<MeshRendererComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "MeshRenderer", alloc);
            comp.AddMember("mesh_path", rapidjson::Value(mr.mesh_path.c_str(), alloc), alloc);
            comp.AddMember("shader_variant", rapidjson::Value(mr.shader_variant.c_str(), alloc), alloc);
            comp.AddMember("metallic", mr.metallic, alloc);
            comp.AddMember("roughness", mr.roughness, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(mr.color.r, alloc).PushBack(mr.color.g, alloc).PushBack(mr.color.b, alloc).PushBack(mr.color.a, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value("MeshRenderer", alloc), alloc);
        }
    }
    if (registry.all_of<Camera3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<Camera3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Camera3D", alloc);
            comp.AddMember("fov", c.fov, alloc);
            comp.AddMember("near_clip", c.near_clip, alloc);
            comp.AddMember("far_clip", c.far_clip, alloc);
            arr.PushBack(comp, alloc);
        } else {
            arr.PushBack(rapidjson::Value("Camera3D", alloc), alloc);
        }
    }
    // ─── Lights (full property readback) ────────────────────────────────────
    if (registry.all_of<DirectionalLight3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<DirectionalLight3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "DirectionalLight", alloc);
            comp.AddMember("intensity", c.intensity, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc);
            comp.AddMember("color", col, alloc);
            rapidjson::Value dir(rapidjson::kArrayType);
            dir.PushBack(c.direction.x, alloc).PushBack(c.direction.y, alloc).PushBack(c.direction.z, alloc);
            comp.AddMember("direction", dir, alloc);
            comp.AddMember("cast_shadow", c.cast_shadow, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("DirectionalLight", alloc), alloc); }
    }
    if (registry.all_of<PointLightComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<PointLightComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "PointLight", alloc);
            comp.AddMember("intensity", c.intensity, alloc);
            comp.AddMember("range", c.radius, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("PointLight", alloc), alloc); }
    }
    if (registry.all_of<SpotLightComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SpotLightComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "SpotLight", alloc);
            comp.AddMember("intensity", c.intensity, alloc);
            comp.AddMember("range", c.radius, alloc);
            comp.AddMember("inner_cone", c.inner_cone_angle, alloc);
            comp.AddMember("outer_cone", c.outer_cone_angle, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("SpotLight", alloc), alloc); }
    }
    if (registry.all_of<SkyLightComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SkyLightComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "SkyLight", alloc);
            comp.AddMember("intensity", c.intensity, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("SkyLight", alloc), alloc); }
    }
    // ─── Physics (full property readback) ────────────────────────────────────
    if (registry.all_of<RigidBody3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<RigidBody3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "RigidBody3D", alloc);
            comp.AddMember("mass", c.mass, alloc);
            const char* bt = c.type == RigidBody3DType::Static ? "static" : c.type == RigidBody3DType::Kinematic ? "kinematic" : "dynamic";
            comp.AddMember("body_type", rapidjson::Value(bt, alloc), alloc);
            comp.AddMember("drag", c.drag, alloc);
            comp.AddMember("angular_drag", c.angular_drag, alloc);
            comp.AddMember("use_gravity", c.use_gravity, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("RigidBody3D", alloc), alloc); }
    }
    if (registry.all_of<BoxCollider3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<BoxCollider3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "BoxCollider3D", alloc);
            rapidjson::Value sz(rapidjson::kArrayType);
            sz.PushBack(c.size.x, alloc).PushBack(c.size.y, alloc).PushBack(c.size.z, alloc);
            comp.AddMember("size", sz, alloc);
            rapidjson::Value ct(rapidjson::kArrayType);
            ct.PushBack(c.center.x, alloc).PushBack(c.center.y, alloc).PushBack(c.center.z, alloc);
            comp.AddMember("center", ct, alloc);
            comp.AddMember("is_trigger", c.is_trigger, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("BoxCollider3D", alloc), alloc); }
    }
    if (registry.all_of<SphereCollider3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SphereCollider3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "SphereCollider3D", alloc);
            comp.AddMember("radius", c.radius, alloc);
            comp.AddMember("is_trigger", c.is_trigger, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("SphereCollider3D", alloc), alloc); }
    }
    if (registry.all_of<CapsuleCollider3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<CapsuleCollider3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "CapsuleCollider3D", alloc);
            comp.AddMember("radius", c.radius, alloc);
            comp.AddMember("height", c.height, alloc);
            comp.AddMember("is_trigger", c.is_trigger, alloc);
            comp.AddMember("direction", c.direction, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("CapsuleCollider3D", alloc), alloc); }
    }
    if (registry.all_of<MeshCollider3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<MeshCollider3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "MeshCollider3D", alloc);
            comp.AddMember("convex", c.convex, alloc);
            comp.AddMember("is_trigger", c.is_trigger, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("MeshCollider3D", alloc), alloc); }
    }
    if (registry.all_of<CharacterController3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<CharacterController3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "CharacterController3D", alloc);
            comp.AddMember("radius", c.radius, alloc);
            comp.AddMember("height", c.height, alloc);
            comp.AddMember("slope_limit", c.slope_limit, alloc);
            comp.AddMember("step_offset", c.step_offset, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("CharacterController3D", alloc), alloc); }
    }
    if (registry.all_of<Joint3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<Joint3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Joint3D", alloc);
            comp.AddMember("spring_stiffness", c.spring_stiffness, alloc);
            comp.AddMember("spring_damping", c.spring_damping, alloc);
            rapidjson::Value a(rapidjson::kArrayType);
            a.PushBack(c.anchor.x, alloc).PushBack(c.anchor.y, alloc).PushBack(c.anchor.z, alloc);
            comp.AddMember("anchor", a, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Joint3D", alloc), alloc); }
    }
    if (registry.all_of<SoftBodyComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SoftBodyComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "SoftBody", alloc);
            comp.AddMember("stiffness", c.stiffness, alloc);
            comp.AddMember("damping", c.damping, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("SoftBody", alloc), alloc); }
    }
    if (registry.all_of<RopeComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<RopeComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Rope", alloc);
            comp.AddMember("segment_count", c.segment_count, alloc);
            comp.AddMember("segment_length", c.segment_length, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Rope", alloc), alloc); }
    }
    // ─── Audio (full property readback) ──────────────────────────────────────
    if (registry.all_of<AudioSourceComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<AudioSourceComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "AudioSource", alloc);
            comp.AddMember("volume", c.volume, alloc);
            comp.AddMember("pitch", c.pitch, alloc);
            comp.AddMember("loop", c.loop, alloc);
            comp.AddMember("play_on_awake", c.play_on_awake, alloc);
            comp.AddMember("spatial_enabled", c.spatial_enabled, alloc);
            comp.AddMember("min_distance", c.min_distance, alloc);
            comp.AddMember("max_distance", c.max_distance, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("AudioSource", alloc), alloc); }
    }
    if (registry.all_of<AudioListenerComponent>(entity)) addComp("AudioListener");
    // ─── Sky / Post (full property readback) ─────────────────────────────────
    if (registry.all_of<SkyboxComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SkyboxComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Skybox", alloc);
            comp.AddMember("cubemap_path", rapidjson::Value(c.cubemap_path.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Skybox", alloc), alloc); }
    }
    if (registry.all_of<PostProcessComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<PostProcessComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "PostProcess", alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            comp.AddMember("bloom_intensity", c.bloom_intensity, alloc);
            comp.AddMember("exposure", c.exposure, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("PostProcess", alloc), alloc); }
    }
    // ─── 3D Render extended (full property readback) ─────────────────────────
    if (registry.all_of<DecalComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<DecalComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Decal", alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            comp.AddMember("angle_fade", c.angle_fade, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc).PushBack(c.color.a, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Decal", alloc), alloc); }
    }
    if (registry.all_of<WaterComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<WaterComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Water", alloc);
            comp.AddMember("water_level", c.water_level, alloc);
            comp.AddMember("wave_amplitude", c.wave_amplitude, alloc);
            comp.AddMember("wave_speed", c.wave_speed, alloc);
            comp.AddMember("transparency", c.transparency, alloc);
            rapidjson::Value dc(rapidjson::kArrayType);
            dc.PushBack(c.deep_color.r, alloc).PushBack(c.deep_color.g, alloc).PushBack(c.deep_color.b, alloc);
            comp.AddMember("deep_color", dc, alloc);
            rapidjson::Value sc(rapidjson::kArrayType);
            sc.PushBack(c.shallow_color.r, alloc).PushBack(c.shallow_color.g, alloc).PushBack(c.shallow_color.b, alloc);
            comp.AddMember("shallow_color", sc, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Water", alloc), alloc); }
    }
    if (registry.all_of<TerrainComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<TerrainComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Terrain", alloc);
            comp.AddMember("width", c.width, alloc);
            comp.AddMember("depth", c.depth, alloc);
            comp.AddMember("max_height", c.max_height, alloc);
            comp.AddMember("heightmap_path", rapidjson::Value(c.heightmap_path.c_str(), alloc), alloc);
            comp.AddMember("texture_path", rapidjson::Value(c.texture_path.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Terrain", alloc), alloc); }
    }
    if (registry.all_of<GrassComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<GrassComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Grass", alloc);
            comp.AddMember("density", c.density, alloc);
            comp.AddMember("spawn_radius", c.spawn_radius, alloc);
            comp.AddMember("blade_width", c.blade_width, alloc);
            comp.AddMember("blade_height", c.blade_height, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Grass", alloc), alloc); }
    }
    if (registry.all_of<LODGroupComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<LODGroupComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "LODGroup", alloc);
            comp.AddMember("global_scale", c.global_scale, alloc);
            comp.AddMember("hysteresis", c.hysteresis, alloc);
            comp.AddMember("lod_count", static_cast<int>(c.levels.size()), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("LODGroup", alloc), alloc); }
    }
    if (registry.all_of<LightProbeComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<LightProbeComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "LightProbe", alloc);
            comp.AddMember("influence_radius", c.influence_radius, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("LightProbe", alloc), alloc); }
    }
    if (registry.all_of<ReflectionProbeComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<ReflectionProbeComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "ReflectionProbe", alloc);
            comp.AddMember("influence_radius", c.influence_radius, alloc);
            comp.AddMember("use_box_projection", c.use_box_projection, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("ReflectionProbe", alloc), alloc); }
    }
    if (registry.all_of<GIProbeVolumeComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<GIProbeVolumeComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "GIProbeVolume", alloc);
            comp.AddMember("gi_intensity", c.gi_intensity, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("GIProbeVolume", alloc), alloc); }
    }
    if (registry.all_of<ImpostorComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<ImpostorComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Impostor", alloc);
            comp.AddMember("atlas_path", rapidjson::Value(c.atlas_path.c_str(), alloc), alloc);
            comp.AddMember("transition_distance", c.transition_distance, alloc);
            comp.AddMember("fade_range", c.fade_range, alloc);
            comp.AddMember("cast_shadow", c.cast_shadow, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Impostor", alloc), alloc); }
    }
    if (registry.all_of<SubSceneComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SubSceneComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "SubScene", alloc);
            comp.AddMember("scene_path", rapidjson::Value(c.scene_path.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("SubScene", alloc), alloc); }
    }
    // ─── Animation (full property readback) ──────────────────────────────────
    if (registry.all_of<Animator3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<Animator3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Animator3D", alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            comp.AddMember("dskel_path", rapidjson::Value(c.dskel_path.c_str(), alloc), alloc);
            comp.AddMember("danim_path", rapidjson::Value(c.danim_path.c_str(), alloc), alloc);
            comp.AddMember("speed", c.speed, alloc);
            comp.AddMember("loop", c.loop, alloc);
            comp.AddMember("current_time", c.current_time, alloc);
            comp.AddMember("lock_root_motion", c.lock_root_motion, alloc);
            comp.AddMember("extract_root_motion", c.extract_root_motion, alloc);
            if (!c.current_state_name.empty())
                comp.AddMember("current_state", rapidjson::Value(c.current_state_name.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Animator3D", alloc), alloc); }
    }
    if (registry.all_of<BoneAttachmentComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<BoneAttachmentComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "BoneAttachment", alloc);
            comp.AddMember("bone_name", rapidjson::Value(c.bone_name.c_str(), alloc), alloc);
            comp.AddMember("target_entity", static_cast<uint32_t>(c.target_entity), alloc);
            rapidjson::Value off(rapidjson::kArrayType);
            off.PushBack(c.offset_position.x, alloc).PushBack(c.offset_position.y, alloc).PushBack(c.offset_position.z, alloc);
            comp.AddMember("offset_position", off, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("BoneAttachment", alloc), alloc); }
    }
    // ─── Particle (full property readback) ───────────────────────────────────
    if (registry.all_of<ParticleSystem3DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<ParticleSystem3DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "ParticleSystem3D", alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            comp.AddMember("max_particles", c.max_particles, alloc);
            comp.AddMember("emission_rate", c.emission_rate, alloc);
            comp.AddMember("start_speed_min", c.start_speed_min, alloc);
            comp.AddMember("start_speed_max", c.start_speed_max, alloc);
            rapidjson::Value sc2(rapidjson::kArrayType);
            sc2.PushBack(c.start_color.r, alloc).PushBack(c.start_color.g, alloc).PushBack(c.start_color.b, alloc).PushBack(c.start_color.a, alloc);
            comp.AddMember("start_color", sc2, alloc);
            comp.AddMember("texture_path", rapidjson::Value(c.texture_path.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("ParticleSystem3D", alloc), alloc); }
    }
    // ─── AI (full property readback) ─────────────────────────────────────────
    if (registry.all_of<BehaviorTreeComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<BehaviorTreeComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "BehaviorTree", alloc);
            comp.AddMember("tree_name", rapidjson::Value(c.tree_name.c_str(), alloc), alloc);
            comp.AddMember("auto_restart", c.auto_restart, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("BehaviorTree", alloc), alloc); }
    }
    if (registry.all_of<CutsceneComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<CutsceneComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Cutscene", alloc);
            comp.AddMember("sequence_name", rapidjson::Value(c.sequence_name.c_str(), alloc), alloc);
            comp.AddMember("auto_play", c.auto_play, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Cutscene", alloc), alloc); }
    }
    if (registry.all_of<SteeringComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SteeringComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Steering", alloc);
            comp.AddMember("max_velocity", c.max_velocity, alloc);
            comp.AddMember("max_force", c.max_force, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Steering", alloc), alloc); }
    }
    // ─── Script (full property readback) ─────────────────────────────────────
    if (registry.all_of<ScriptComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<ScriptComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Script", alloc);
            comp.AddMember("script_path", rapidjson::Value(c.script_path.c_str(), alloc), alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Script", alloc), alloc); }
    }
    if (registry.all_of<LuaScriptComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<LuaScriptComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "LuaScript", alloc);
            comp.AddMember("script_path", rapidjson::Value(c.script_path.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("LuaScript", alloc), alloc); }
    }
    if (registry.all_of<CSharpScriptComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<CSharpScriptComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "CSharpScript", alloc);
            comp.AddMember("class_name", rapidjson::Value(c.class_name.c_str(), alloc), alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("CSharpScript", alloc), alloc); }
    }
    if (registry.all_of<BlueprintComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<BlueprintComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Blueprint", alloc);
            comp.AddMember("blueprint_asset_path", rapidjson::Value(c.blueprint_asset_path.c_str(), alloc), alloc);
            comp.AddMember("enabled", c.enabled, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Blueprint", alloc), alloc); }
    }
    // ─── UI (full property readback) ─────────────────────────────────────────
    if (registry.all_of<UIRendererComponent>(entity)) addComp("UIRenderer");
    if (registry.all_of<UIPanelComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIPanelComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIPanel", alloc);
            comp.AddMember("blocks_input", c.blocks_input, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIPanel", alloc), alloc); }
    }
    if (registry.all_of<UIButtonComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIButtonComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIButton", alloc);
            rapidjson::Value nc(rapidjson::kArrayType);
            nc.PushBack(c.normal_color.r, alloc).PushBack(c.normal_color.g, alloc).PushBack(c.normal_color.b, alloc).PushBack(c.normal_color.a, alloc);
            comp.AddMember("normal_color", nc, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIButton", alloc), alloc); }
    }
    if (registry.all_of<UILabelComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UILabelComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UILabel", alloc);
            comp.AddMember("text", rapidjson::Value(c.text.c_str(), alloc), alloc);
            comp.AddMember("font_size", c.font_size, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc).PushBack(c.color.a, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UILabel", alloc), alloc); }
    }
    if (registry.all_of<UIMaskComponent>(entity)) addComp("UIMask");
    if (registry.all_of<UIRichTextComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIRichTextComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIRichText", alloc);
            comp.AddMember("text", rapidjson::Value(c.text.c_str(), alloc), alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIRichText", alloc), alloc); }
    }
    if (registry.all_of<UIJoystickComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIJoystickComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIJoystick", alloc);
            comp.AddMember("max_radius", c.max_radius, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIJoystick", alloc), alloc); }
    }
    if (registry.all_of<UIAnchorComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIAnchorComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIAnchor", alloc);
            comp.AddMember("anchor", c.anchor, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIAnchor", alloc), alloc); }
    }
    if (registry.all_of<UIGridLayoutComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIGridLayoutComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIGridLayout", alloc);
            comp.AddMember("columns", c.columns, alloc);
            rapidjson::Value cs(rapidjson::kArrayType);
            cs.PushBack(c.cell_size.x, alloc).PushBack(c.cell_size.y, alloc);
            comp.AddMember("cell_size", cs, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIGridLayout", alloc), alloc); }
    }
    if (registry.all_of<UICanvasScalerComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UICanvasScalerComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UICanvasScaler", alloc);
            rapidjson::Value rr(rapidjson::kArrayType);
            rr.PushBack(c.reference_resolution.x, alloc).PushBack(c.reference_resolution.y, alloc);
            comp.AddMember("reference_resolution", rr, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UICanvasScaler", alloc), alloc); }
    }
    if (registry.all_of<UIBoxLayoutComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIBoxLayoutComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIBoxLayout", alloc);
            comp.AddMember("vertical", c.vertical, alloc);
            comp.AddMember("spacing", c.spacing, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIBoxLayout", alloc), alloc); }
    }
    if (registry.all_of<UIContentSizeFitterComponent>(entity)) addComp("UIContentSizeFitter");
    if (registry.all_of<UIAnimationComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIAnimationComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIAnimation", alloc);
            comp.AddMember("duration", c.duration, alloc);
            comp.AddMember("loop", c.loop, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIAnimation", alloc), alloc); }
    }
    if (registry.all_of<UITextInputComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UITextInputComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UITextInput", alloc);
            comp.AddMember("placeholder", rapidjson::Value(c.placeholder.c_str(), alloc), alloc);
            comp.AddMember("is_password", c.is_password, alloc);
            comp.AddMember("multiline", c.multiline, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UITextInput", alloc), alloc); }
    }
    if (registry.all_of<UIScrollViewComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIScrollViewComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIScrollView", alloc);
            comp.AddMember("horizontal", c.horizontal, alloc);
            comp.AddMember("vertical", c.vertical, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIScrollView", alloc), alloc); }
    }
    if (registry.all_of<UISliderComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UISliderComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UISlider", alloc);
            comp.AddMember("value", c.value, alloc);
            comp.AddMember("min_value", c.min_value, alloc);
            comp.AddMember("max_value", c.max_value, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UISlider", alloc), alloc); }
    }
    if (registry.all_of<UIToggleComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIToggleComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIToggle", alloc);
            comp.AddMember("is_on", c.is_on, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIToggle", alloc), alloc); }
    }
    if (registry.all_of<UIProgressBarComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<UIProgressBarComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "UIProgressBar", alloc);
            comp.AddMember("value", c.value, alloc);
            comp.AddMember("max_value", c.max_value, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("UIProgressBar", alloc), alloc); }
    }
    if (registry.all_of<UIDropdownComponent>(entity)) addComp("UIDropdown");
    // ─── 2D (full property readback) ─────────────────────────────────────────
    if (registry.all_of<SpriteRendererComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<SpriteRendererComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "SpriteRenderer", alloc);
            comp.AddMember("shader_variant", rapidjson::Value(c.shader_variant.c_str(), alloc), alloc);
            comp.AddMember("visible", c.visible, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc).PushBack(c.color.a, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("SpriteRenderer", alloc), alloc); }
    }
    if (registry.all_of<Light2DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<Light2DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "Light2D", alloc);
            comp.AddMember("intensity", c.intensity, alloc);
            comp.AddMember("range", c.range, alloc);
            rapidjson::Value col(rapidjson::kArrayType);
            col.PushBack(c.color.r, alloc).PushBack(c.color.g, alloc).PushBack(c.color.b, alloc);
            comp.AddMember("color", col, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("Light2D", alloc), alloc); }
    }
    if (registry.all_of<TrailRenderer2DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<TrailRenderer2DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "TrailRenderer2D", alloc);
            comp.AddMember("lifetime", c.lifetime, alloc);
            comp.AddMember("start_width", c.start_width, alloc);
            comp.AddMember("end_width", c.end_width, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("TrailRenderer2D", alloc), alloc); }
    }
    if (registry.all_of<LineRenderer2DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<LineRenderer2DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "LineRenderer2D", alloc);
            comp.AddMember("width", c.width, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("LineRenderer2D", alloc), alloc); }
    }
    if (registry.all_of<ParallaxComponent>(entity)) addComp("Parallax");
    if (registry.all_of<CameraController2DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<CameraController2DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "CameraController2D", alloc);
            comp.AddMember("target_zoom", c.target_zoom, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("CameraController2D", alloc), alloc); }
    }
    if (registry.all_of<AudioSpatial2DComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<AudioSpatial2DComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "AudioSpatial2D", alloc);
            comp.AddMember("min_distance", c.min_distance, alloc);
            comp.AddMember("max_distance", c.max_distance, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("AudioSpatial2D", alloc), alloc); }
    }
    if (registry.all_of<AudioListener2DComponent>(entity)) addComp("AudioListener2D");
    if (registry.all_of<FreeCameraControllerComponent>(entity)) {
        if (include_properties) {
            const auto& c = registry.get<FreeCameraControllerComponent>(entity);
            rapidjson::Value comp(rapidjson::kObjectType);
            comp.AddMember("type", "FreeCameraController", alloc);
            comp.AddMember("move_speed", c.move_speed, alloc);
            comp.AddMember("mouse_sensitivity", c.mouse_sensitivity, alloc);
            arr.PushBack(comp, alloc);
        } else { arr.PushBack(rapidjson::Value("FreeCameraController", alloc), alloc); }
    }
}

// ─── Tool: dsengine_entity_get_components ───────────────────────────────────

static JsonRpcResponse HandleEntityGetComponents(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    bool detailed = true;
    if (params.HasMember("detailed") && params["detailed"].IsBool()) {
        detailed = params["detailed"].GetBool();
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);

    if (registry.all_of<EditorNameComponent>(entity)) {
        result.AddMember("name",
            rapidjson::Value(registry.get<EditorNameComponent>(entity).name.c_str(), alloc), alloc);
    }

    rapidjson::Value comps(rapidjson::kArrayType);
    CollectEntityComponents(registry, entity, comps, alloc, detailed);
    result.AddMember("components", comps, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_save ──────────────────────────────────────────────

static JsonRpcResponse HandleSceneSave(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string path;
    if (params.HasMember("path") && params["path"].IsString()) {
        path = params["path"].GetString();
    } else {
        path = GetCurrentScenePath();
        if (path.empty() || path == "Untitled") {
            return MakeToolError(-32602, "No path specified and no current scene file");
        }
    }

    auto& registry = engine.pipeline()->world().registry();
    SaveScene(registry, path);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("saved", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_load ──────────────────────────────────────────────

static JsonRpcResponse HandleSceneLoad(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::string path = params["path"].GetString();
    if (!std::filesystem::exists(path)) {
        return MakeToolError(-32602, "Scene file not found: " + path);
    }

    auto& registry = engine.pipeline()->world().registry();
    LoadScene(registry, path);
    SetCurrentScenePath(path);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("loaded", rapidjson::Value(true), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    return MakeOk(std::move(result));
}

// ─── Helper: Base64 编码 ────────────────────────────────────────────────────

static const char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<unsigned int>(data[i + 2]);
        out.push_back(kBase64Table[(n >> 18) & 0x3F]);
        out.push_back(kBase64Table[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kBase64Table[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kBase64Table[n & 0x3F] : '=');
    }
    return out;
}

// stb_image_write 回调：将 PNG 数据追加到 vector
static void StbWriteCallback(void* context, void* data, int size) {
    auto* buf = static_cast<std::vector<unsigned char>*>(context);
    auto* bytes = static_cast<unsigned char*>(data);
    buf->insert(buf->end(), bytes, bytes + size);
}

// ─── Tool: dsengine_editor_screenshot ───────────────────────────────────────

static JsonRpcResponse HandleEditorScreenshot(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string target = "scene";
    if (params.HasMember("target") && params["target"].IsString()) {
        target = params["target"].GetString();
    }

    RenderTargetReadback readback;
    if (target == "game") {
        readback = engine.pipeline()->ReadMainColorRgba8WithSize();
    } else {
        readback = engine.pipeline()->ReadSceneColorRgba8WithSize();
    }

    if (readback.pixels.empty() || readback.width <= 0 || readback.height <= 0) {
        return MakeToolError(-32603, "Failed to read framebuffer (no active render target)");
    }

    // Y-flip if needed (OpenGL)
    if (engine.pipeline()->NeedsReadbackYFlip()) {
        const int stride = readback.width * 4;
        std::vector<unsigned char> row(stride);
        for (int y = 0; y < readback.height / 2; ++y) {
            unsigned char* top = readback.pixels.data() + y * stride;
            unsigned char* bot = readback.pixels.data() + (readback.height - 1 - y) * stride;
            std::memcpy(row.data(), top, stride);
            std::memcpy(top, bot, stride);
            std::memcpy(bot, row.data(), stride);
        }
    }

    // Encode to PNG in memory
    std::vector<unsigned char> png_buf;
    png_buf.reserve(readback.width * readback.height); // rough estimate
    int ok = stbi_write_png_to_func(
        StbWriteCallback, &png_buf,
        readback.width, readback.height, 4,
        readback.pixels.data(), readback.width * 4);

    if (!ok || png_buf.empty()) {
        return MakeToolError(-32603, "PNG encoding failed");
    }

    std::string b64 = Base64Encode(png_buf.data(), png_buf.size());

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("width", readback.width, alloc);
    result.AddMember("height", readback.height, alloc);
    result.AddMember("format", "png", alloc);
    result.AddMember("encoding", "base64", alloc);
    result.AddMember("data", rapidjson::Value(b64.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_asset_import ─────────────────────────────────────────────

static JsonRpcResponse HandleAssetImport(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }
    std::string path = params["path"].GetString();

    std::string type = "auto";
    if (params.HasMember("type") && params["type"].IsString()) {
        type = params["type"].GetString();
    }

    auto* am = engine.asset_manager();
    if (!am) {
        return MakeToolError(-32603, "AssetManager not available");
    }

    // auto-detect type from extension
    if (type == "auto") {
        auto ext_pos = path.rfind('.');
        if (ext_pos != std::string::npos) {
            std::string ext = path.substr(ext_pos);
            for (auto& c : ext) c = static_cast<char>(std::tolower(c));
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                ext == ".tga" || ext == ".dds" || ext == ".hdr" || ext == ".ppm")
                type = "texture";
            else if (ext == ".dmesh")
                type = "mesh";
            else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac")
                type = "audio";
            else if (ext == ".dmat")
                type = "material";
        }
        if (type == "auto") {
            return MakeToolError(-32602, "Cannot auto-detect asset type for: " + path);
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (type == "texture") {
        auto tex = am->LoadTexture(path);
        if (!tex) return MakeToolError(-32603, "Failed to load texture: " + path);
        result.AddMember("type", "texture", alloc);
        result.AddMember("handle", tex->GetHandle(), alloc);
        result.AddMember("width", tex->GetWidth(), alloc);
        result.AddMember("height", tex->GetHeight(), alloc);
        result.AddMember("channels", tex->GetChannels(), alloc);
    } else if (type == "mesh") {
        auto mesh = am->LoadDmesh(path);
        if (!mesh) return MakeToolError(-32603, "Failed to load mesh: " + path);
        result.AddMember("type", "mesh", alloc);
        result.AddMember("path", rapidjson::Value(mesh->GetPath().c_str(), alloc), alloc);
        result.AddMember("size_bytes", static_cast<uint64_t>(mesh->GetData().size()), alloc);
    } else if (type == "audio") {
        auto clip = am->LoadAudioClip(path);
        if (!clip) return MakeToolError(-32603, "Failed to load audio: " + path);
        result.AddMember("type", "audio", alloc);
        result.AddMember("path", rapidjson::Value(clip->GetPath().c_str(), alloc), alloc);
        result.AddMember("size_bytes", static_cast<uint64_t>(clip->GetData().size()), alloc);
    } else if (type == "material") {
        std::size_t idx = 0;
        if (params.HasMember("material_index") && params["material_index"].IsUint())
            idx = params["material_index"].GetUint();
        auto mat = am->LoadMaterialInstanceFromDmat(path, idx);
        if (!mat) return MakeToolError(-32603, "Failed to load material: " + path);
        result.AddMember("type", "material", alloc);
        result.AddMember("material_id", mat->GetId(), alloc);
        result.AddMember("name", rapidjson::Value(mat->GetName().c_str(), alloc), alloc);
    } else {
        return MakeToolError(-32602, "Unknown asset type: " + type);
    }

    result.AddMember("success", true, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_material_create ─────────────────────────────────────────

static JsonRpcResponse HandleMaterialCreate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    std::string name = "untitled_material";
    if (params.HasMember("name") && params["name"].IsString()) {
        name = params["name"].GetString();
    }

    auto* am = engine.asset_manager();
    if (!am) {
        return MakeToolError(-32603, "AssetManager not available");
    }

    // Build .dmat JSON
    rapidjson::Document dmat;
    dmat.SetObject();
    auto& da = dmat.GetAllocator();

    rapidjson::Value mat_obj(rapidjson::kObjectType);
    mat_obj.AddMember("name", rapidjson::Value(name.c_str(), da), da);

    // shader_variant
    std::string shader_variant = "MESH_PBR";
    if (params.HasMember("shader_variant") && params["shader_variant"].IsString())
        shader_variant = params["shader_variant"].GetString();
    mat_obj.AddMember("shader_variant", rapidjson::Value(shader_variant.c_str(), da), da);

    // base_color [r,g,b,a]
    if (params.HasMember("base_color") && params["base_color"].IsArray()) {
        rapidjson::Value bc(rapidjson::kArrayType);
        for (auto& v : params["base_color"].GetArray()) bc.PushBack(v.GetFloat(), da);
        mat_obj.AddMember("base_color", bc, da);
    } else {
        rapidjson::Value bc(rapidjson::kArrayType);
        bc.PushBack(1.0f, da).PushBack(1.0f, da).PushBack(1.0f, da).PushBack(1.0f, da);
        mat_obj.AddMember("base_color", bc, da);
    }

    // emissive [r,g,b]
    if (params.HasMember("emissive") && params["emissive"].IsArray()) {
        rapidjson::Value em(rapidjson::kArrayType);
        for (auto& v : params["emissive"].GetArray()) em.PushBack(v.GetFloat(), da);
        mat_obj.AddMember("emissive", em, da);
    }

    // scalars
    auto addFloat = [&](const char* key) {
        if (params.HasMember(key) && params[key].IsNumber())
            mat_obj.AddMember(rapidjson::Value(key, da),
                              rapidjson::Value(params[key].GetFloat()), da);
    };
    addFloat("metallic");
    addFloat("roughness");
    addFloat("occlusion_strength");
    addFloat("normal_scale");
    addFloat("alpha_cutoff");

    if (params.HasMember("alpha_test") && params["alpha_test"].IsBool())
        mat_obj.AddMember("alpha_test", params["alpha_test"].GetBool(), da);
    if (params.HasMember("double_sided") && params["double_sided"].IsBool())
        mat_obj.AddMember("double_sided", params["double_sided"].GetBool(), da);

    // texture paths
    auto addTex = [&](const char* key) {
        if (params.HasMember(key) && params[key].IsString())
            mat_obj.AddMember(rapidjson::Value(key, da),
                              rapidjson::Value(params[key].GetString(), da), da);
    };
    addTex("base_color_texture");
    addTex("normal_texture");
    addTex("metallic_roughness_texture");
    addTex("emissive_texture");
    addTex("occlusion_texture");

    rapidjson::Value materials_arr(rapidjson::kArrayType);
    materials_arr.PushBack(mat_obj, da);
    dmat.AddMember("materials", materials_arr, da);

    // Serialize to JSON string
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    dmat.Accept(writer);
    std::string json_str = sb.GetString();

    // Save to file
    std::string save_path;
    if (params.HasMember("save_path") && params["save_path"].IsString()) {
        save_path = params["save_path"].GetString();
    } else {
        std::string data_root = am->GetDataRoot();
        save_path = data_root + "/materials/" + name + ".dmat";
    }

    // Ensure directory exists
    std::filesystem::path file_path(save_path);
    if (file_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
    }

    {
        std::ofstream f(save_path, std::ios::binary);
        if (!f.is_open()) {
            return MakeToolError(-32603, "Failed to write material file: " + save_path);
        }
        f.write(json_str.data(), static_cast<std::streamsize>(json_str.size()));
    }

    // Load into engine
    auto loaded = am->LoadMaterialInstanceFromDmat(save_path, 0);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("success", true, alloc);
    result.AddMember("file_path", rapidjson::Value(save_path.c_str(), alloc), alloc);
    if (loaded) {
        result.AddMember("material_id", loaded->GetId(), alloc);
        result.AddMember("material_name", rapidjson::Value(loaded->GetName().c_str(), alloc), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_get_state ────────────────────────────────────────

static JsonRpcResponse HandleEntityGetState(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);

    if (registry.all_of<EditorNameComponent>(entity)) {
        result.AddMember("name",
            rapidjson::Value(registry.get<EditorNameComponent>(entity).name.c_str(), alloc), alloc);
    }

    if (registry.all_of<TransformComponent>(entity)) {
        const auto& t = registry.get<TransformComponent>(entity);
        rapidjson::Value tf(rapidjson::kObjectType);
        rapidjson::Value pos(rapidjson::kArrayType);
        pos.PushBack(t.position.x, alloc); pos.PushBack(t.position.y, alloc); pos.PushBack(t.position.z, alloc);
        tf.AddMember("position", pos, alloc);
        rapidjson::Value rot(rapidjson::kArrayType);
        rot.PushBack(t.rotation.x, alloc); rot.PushBack(t.rotation.y, alloc);
        rot.PushBack(t.rotation.z, alloc); rot.PushBack(t.rotation.w, alloc);
        tf.AddMember("rotation", rot, alloc);
        rapidjson::Value scl(rapidjson::kArrayType);
        scl.PushBack(t.scale.x, alloc); scl.PushBack(t.scale.y, alloc); scl.PushBack(t.scale.z, alloc);
        tf.AddMember("scale", scl, alloc);
        result.AddMember("transform", tf, alloc);
    }

    rapidjson::Value comps(rapidjson::kArrayType);
    CollectEntityComponents(registry, entity, comps, alloc, true);
    result.AddMember("components", comps, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_duplicate ────────────────────────────────────────

static JsonRpcResponse HandleEntityDuplicate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto src = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& world = engine.pipeline()->world();
    auto& registry = world.registry();

    if (!registry.valid(src)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    // 全组件复制（与层级面板 Duplicate 收敛后一致）：用快照捕获 src 的所有组件，
    // 还原为新实体（运行时状态自动重置）。挂到根（清除 parent/sibling）、改名 + 偏移，
    // 与既有行为保持一致。
    EntitySnapshot copy_snap = EntitySnapshot::Capture(registry, src);
    copy_snap.parent.reset();
    copy_snap.sibling_index.reset();
    auto dst = copy_snap.Restore(world, registry);

    std::string new_name = "Copy";
    if (registry.all_of<EditorNameComponent>(dst)) {
        new_name = registry.get<EditorNameComponent>(dst).name + " (Copy)";
        registry.get<EditorNameComponent>(dst).name = new_name;
    } else {
        registry.emplace<EditorNameComponent>(dst, new_name);
    }

    if (registry.all_of<TransformComponent>(dst)) {
        auto& tf = registry.get<TransformComponent>(dst);
        tf.position += glm::vec3(0.5f, 0.0f, 0.5f);
        tf.dirty = true;
    } else {
        registry.emplace<TransformComponent>(dst);
    }

    // Undo/Redo（全组件保真）：do/redo 重建副本快照、undo 销毁副本。
    {
        auto& w = world;
        auto& reg = registry;
        auto snapshot = std::make_shared<EntitySnapshot>(EntitySnapshot::Capture(reg, dst));
        auto tracked = std::make_shared<entt::entity>(dst);
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Duplicate Entity (RPC)",
            [tracked, snapshot, &w, &reg]() {
                if (*tracked != entt::null && reg.valid(*tracked)) return;
                *tracked = snapshot->Restore(w, reg);
            },
            [tracked, &w, &reg]() {
                if (*tracked != entt::null && reg.valid(*tracked)) {
                    w.DestroyEntity(*tracked);
                    *tracked = entt::null;
                }
            }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(dst), alloc);
    result.AddMember("name", rapidjson::Value(new_name.c_str(), alloc), alloc);
    result.AddMember("source_entity_id", static_cast<uint32_t>(src), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_prefab_save ──────────────────────────────────────────────

static JsonRpcResponse HandlePrefabSave(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }
    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    std::string path = params["path"].GetString();
    if (path.find("..") != std::string::npos) {
        return MakeToolError(-32602, "Path must not contain '..'");
    }

    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (!SaveEntityAsPrefab(registry, entity, path)) {
        return MakeToolError(-32603, "Failed to save prefab: " + path);
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("saved", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_prefab_instantiate ───────────────────────────────────────

static JsonRpcResponse HandlePrefabInstantiate(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::string path = params["path"].GetString();
    if (!std::filesystem::exists(path)) {
        return MakeToolError(-32602, "Prefab file not found: " + path);
    }

    auto& world = engine.pipeline()->world();
    auto& registry = world.registry();
    auto entity = InstantiatePrefab(world, registry, path);

    if (entity == entt::null) {
        return MakeToolError(-32603, "Failed to instantiate prefab: " + path);
    }

    // Undo: destroy the instantiated entity
    {
        entt::entity inst = entity;
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Instantiate Prefab (RPC)",
            []() {},
            [inst, &reg]() { if (reg.valid(inst)) reg.destroy(inst); }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    std::string inst_name = "Prefab Instance";
    if (registry.all_of<EditorNameComponent>(entity))
        inst_name = registry.get<EditorNameComponent>(entity).name;
    result.AddMember("name", rapidjson::Value(inst_name.c_str(), alloc), alloc);
    result.AddMember("source_path", rapidjson::Value(path.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_new ────────────────────────────────────────────────

static JsonRpcResponse HandleSceneNew(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    registry.clear();
    GetUndoRedoManager().Clear();
    SetCurrentScenePath("Untitled");

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("cleared", rapidjson::Value(true), alloc);
    result.AddMember("path", rapidjson::Value("Untitled", alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_reparent ──────────────────────────────────────────

static JsonRpcResponse HandleEntityReparent(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint()) {
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    }

    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    auto& registry = engine.pipeline()->world().registry();

    if (!registry.valid(entity)) {
        return MakeToolError(-32602, "Invalid entity_id");
    }

    // parent_id: optional uint. Omit or 0xFFFFFFFF to detach (set root).
    entt::entity new_parent = entt::null;
    if (params.HasMember("parent_id") && params["parent_id"].IsUint()) {
        uint32_t pid = params["parent_id"].GetUint();
        if (pid != 0xFFFFFFFFu) {
            new_parent = static_cast<entt::entity>(pid);
            if (!registry.valid(new_parent)) {
                return MakeToolError(-32602, "Invalid parent_id");
            }
            // Cycle detection: new_parent must not be a descendant of entity
            entt::entity cur = new_parent;
            while (cur != entt::null && registry.valid(cur)) {
                if (cur == entity) return MakeToolError(-32602, "Circular parenting detected");
                if (!registry.all_of<ParentComponent>(cur)) break;
                cur = registry.get<ParentComponent>(cur).parent;
            }
        }
    }

    entt::entity old_parent = entt::null;
    int old_sibling = 0;
    if (registry.all_of<ParentComponent>(entity))
        old_parent = registry.get<ParentComponent>(entity).parent;
    if (registry.all_of<SiblingIndexComponent>(entity))
        old_sibling = registry.get<SiblingIndexComponent>(entity).index;

    // Apply new parent
    if (new_parent == entt::null) {
        registry.remove<ParentComponent>(entity);
    } else if (registry.all_of<ParentComponent>(entity)) {
        registry.get<ParentComponent>(entity).parent = new_parent;
    } else {
        registry.emplace<ParentComponent>(entity, new_parent);
    }

    // Apply sibling_index if provided
    bool has_new_sibling = false;
    int new_sibling = 0;
    if (params.HasMember("sibling_index") && params["sibling_index"].IsInt()) {
        has_new_sibling = true;
        new_sibling = params["sibling_index"].GetInt();
        if (registry.all_of<SiblingIndexComponent>(entity))
            registry.get<SiblingIndexComponent>(entity).index = new_sibling;
        else
            registry.emplace<SiblingIndexComponent>(entity, new_sibling);
    }

    // Dirty transform
    if (registry.all_of<TransformComponent>(entity))
        registry.get<TransformComponent>(entity).dirty = true;

    // Undo / Redo（redo 与面板拖拽 reparent 行为一致：重新施加 new_parent 及
    // 显式给定的 sibling_index；redo lambda 即时执行一次为幂等再应用）。
    {
        auto& reg = registry;
        GetUndoRedoManager().Execute(std::make_unique<LambdaCommand>(
            "Reparent Entity (RPC)",
            [&reg, entity, new_parent, has_new_sibling, new_sibling]() {
                if (!reg.valid(entity)) return;
                if (new_parent == entt::null) {
                    reg.remove<ParentComponent>(entity);
                } else if (reg.all_of<ParentComponent>(entity)) {
                    reg.get<ParentComponent>(entity).parent = new_parent;
                } else {
                    reg.emplace<ParentComponent>(entity, new_parent);
                }
                if (has_new_sibling) {
                    if (reg.all_of<SiblingIndexComponent>(entity))
                        reg.get<SiblingIndexComponent>(entity).index = new_sibling;
                    else
                        reg.emplace<SiblingIndexComponent>(entity, new_sibling);
                }
                if (reg.all_of<TransformComponent>(entity))
                    reg.get<TransformComponent>(entity).dirty = true;
            },
            [&reg, entity, old_parent, old_sibling]() {
                if (!reg.valid(entity)) return;
                if (old_parent == entt::null) {
                    reg.remove<ParentComponent>(entity);
                } else if (reg.all_of<ParentComponent>(entity)) {
                    reg.get<ParentComponent>(entity).parent = old_parent;
                } else {
                    reg.emplace<ParentComponent>(entity, old_parent);
                }
                if (reg.all_of<SiblingIndexComponent>(entity))
                    reg.get<SiblingIndexComponent>(entity).index = old_sibling;
                if (reg.all_of<TransformComponent>(entity))
                    reg.get<TransformComponent>(entity).dirty = true;
            }
        ));
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
    result.AddMember("parent_id",
        new_parent == entt::null ? rapidjson::Value(rapidjson::kNullType) :
        rapidjson::Value(static_cast<uint32_t>(new_parent)),
        alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_find_by_name ─────────────────────────────────────

static JsonRpcResponse HandleEntityFindByName(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("name") || !params["name"].IsString()) {
        return MakeToolError(-32602, "Missing required param: name (string)");
    }

    std::string target = params["name"].GetString();
    bool exact = !params.HasMember("partial") || !params["partial"].GetBool();

    auto& registry = engine.pipeline()->world().registry();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    rapidjson::Value matches(rapidjson::kArrayType);
    auto view = registry.view<EditorNameComponent>();
    for (auto entity : view) {
        const std::string& n = view.get<EditorNameComponent>(entity).name;
        bool hit = exact ? (n == target) : (n.find(target) != std::string::npos);
        if (!hit) continue;
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("entity_id", rapidjson::Value(static_cast<uint32_t>(entity)), alloc);
        obj.AddMember("name", rapidjson::Value(n.c_str(), alloc), alloc);
        matches.PushBack(obj, alloc);
    }

    int match_count = static_cast<int>(matches.Size());
    uint32_t first_id = 0;
    bool has_match = match_count > 0;
    if (has_match) first_id = matches[0]["entity_id"].GetUint();

    result.AddMember("matches", matches, alloc);
    result.AddMember("count", rapidjson::Value(match_count), alloc);
    if (has_match) {
        result.AddMember("entity_id", rapidjson::Value(first_id), alloc);
    } else {
        result.AddMember("entity_id", rapidjson::Value(rapidjson::kNullType), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_undo_history ────────────────────────────────────────────

static JsonRpcResponse HandleUndoHistory(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& mgr = GetUndoRedoManager();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    result.AddMember("can_undo", rapidjson::Value(mgr.CanUndo()), alloc);
    result.AddMember("can_redo", rapidjson::Value(mgr.CanRedo()), alloc);
    result.AddMember("undo_count", rapidjson::Value(mgr.GetUndoCount()), alloc);
    result.AddMember("redo_count", rapidjson::Value(mgr.GetRedoCount()), alloc);
    result.AddMember("undo_description",
        rapidjson::Value(mgr.GetUndoDescription().c_str(), alloc), alloc);
    result.AddMember("redo_description",
        rapidjson::Value(mgr.GetRedoDescription().c_str(), alloc), alloc);

    rapidjson::Value undo_list(rapidjson::kArrayType);
    for (const auto& s : mgr.GetUndoHistory())
        undo_list.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
    result.AddMember("undo_history", undo_list, alloc);

    rapidjson::Value redo_list(rapidjson::kArrayType);
    for (const auto& s : mgr.GetRedoHistory())
        redo_list.PushBack(rapidjson::Value(s.c_str(), alloc), alloc);
    result.AddMember("redo_history", redo_list, alloc);

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_selection_get ───────────────────────────────────────────

static JsonRpcResponse HandleSelectionGet(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& sel = SelectionManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    rapidjson::Value ids(rapidjson::kArrayType);
    for (auto ent : sel.GetAll()) {
        ids.PushBack(static_cast<uint32_t>(ent), alloc);
    }
    result.AddMember("entity_ids", ids, alloc);
    result.AddMember("count", rapidjson::Value(sel.Count()), alloc);
    if (sel.GetPrimary() == entt::null) {
        result.AddMember("primary_id", rapidjson::Value(rapidjson::kNullType), alloc);
    } else {
        result.AddMember("primary_id",
            rapidjson::Value(static_cast<uint32_t>(sel.GetPrimary())), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_selection_set ───────────────────────────────────────────

static JsonRpcResponse HandleSelectionSet(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_ids") || !params["entity_ids"].IsArray()) {
        return MakeToolError(-32602, "Missing required param: entity_ids (array of uint)");
    }

    auto& registry = engine.pipeline()->world().registry();
    auto& sel = SelectionManager::Get();
    sel.Clear();

    int added = 0;
    for (const auto& v : params["entity_ids"].GetArray()) {
        if (!v.IsUint()) continue;
        auto ent = static_cast<entt::entity>(v.GetUint());
        if (!registry.valid(ent)) continue;
        sel.Add(ent);
        ++added;
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("count", rapidjson::Value(added), alloc);
    if (sel.GetPrimary() == entt::null) {
        result.AddMember("primary_id", rapidjson::Value(rapidjson::kNullType), alloc);
    } else {
        result.AddMember("primary_id",
            rapidjson::Value(static_cast<uint32_t>(sel.GetPrimary())), alloc);
    }
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_selection_clear ─────────────────────────────────────────

static JsonRpcResponse HandleSelectionClear(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    SelectionManager::Get().Clear();

    rapidjson::Document result;
    result.SetObject();
    result.AddMember("cleared", rapidjson::Value(true), result.GetAllocator());
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_project_open ────────────────────────────────────────────
// 打开工程级（.dseproj）：加载项目描述符 → 同步 data root → 刷新资产库 →
// 加载项目默认场景。现有 dsengine_scene_load 仅 scene 级，本工具补齐工程级语义。

static JsonRpcResponse HandleProjectOpen(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("path") || !params["path"].IsString()) {
        return MakeToolError(-32602, "Missing required param: path");
    }

    std::filesystem::path dseproj = params["path"].GetString();
    if (!std::filesystem::exists(dseproj)) {
        return MakeToolError(-32602, "Project file not found: " + dseproj.string());
    }

    auto& pm = dse::editor::ProjectManager::Get();
    if (!pm.OpenProject(dseproj)) {
        return MakeToolError(-32603, "Failed to open project: " + dseproj.string());
    }
    pm.ApplyDataRoot();
    if (auto* am = engine.asset_manager()) {
        am->ConfigureDataRoot(pm.GetAssetDir().string());
    }
    dse::editor::AssetDatabase::Get().Refresh();

    auto& registry = engine.pipeline()->world().registry();
    bool scene_loaded = false;
    std::string scene_path;
    {
        std::filesystem::path candidate = pm.GetProjectRoot() / pm.GetDescriptor().default_scene;
        if (std::filesystem::exists(candidate)) {
            LoadScene(registry, candidate.string());
            SetCurrentScenePath(candidate.string());
            scene_path = candidate.string();
            scene_loaded = true;
        }
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("opened", rapidjson::Value(true), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    result.AddMember("scene_loaded", rapidjson::Value(scene_loaded), alloc);
    if (scene_loaded) {
        result.AddMember("scene_path", rapidjson::Value(scene_path.c_str(), alloc), alloc);
    }
    rapidjson::Value project(rapidjson::kObjectType);
    project.AddMember("name", rapidjson::Value(pm.GetDescriptor().name.c_str(), alloc), alloc);
    project.AddMember("root", rapidjson::Value(pm.GetProjectRoot().string().c_str(), alloc), alloc);
    project.AddMember("default_scene", rapidjson::Value(pm.GetDescriptor().default_scene.c_str(), alloc), alloc);
    result.AddMember("project", project, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_quit ─────────────────────────────────────────────
// 置退出标志，主循环下一帧返回 false。供自动化会话结束时优雅退出编辑器。

static JsonRpcResponse HandleEditorQuit(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    dse::editor::RequestExit();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("ok", rapidjson::Value(true), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_idle ─────────────────────────────────────────────
// 返回当前帧的状态快照（同步点）。编辑器主循环每帧持续渲染，调用间隔内帧已自然推进，
// 因此本工具用于"等待渲染稳定后读取状态"，frames 参数记录期望推进帧数（供报告参考）。

static JsonRpcResponse HandleEditorIdle(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    int frames = 1;
    if (params.HasMember("frames") && params["frames"].IsInt()) {
        frames = params["frames"].GetInt();
    }

    auto& registry = engine.pipeline()->world().registry();
    const float dt = Time::delta_time();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("frames_requested", frames, alloc);
    result.AddMember("delta_ms", dt * 1000.0f, alloc);
    result.AddMember("fps", dt > 0.0f ? (1.0f / dt) : 0.0f, alloc);
    result.AddMember("time_since_startup", Time::TimeSinceStartup(), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    const char* state_str =
        dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
        dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_get_metrics ──────────────────────────────────────
// 返回 FPS / DrawCall / 实体数等运行时指标，供 soak/性能监控使用。

static JsonRpcResponse HandleEditorGetMetrics(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    const float dt = Time::delta_time();

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("fps", dt > 0.0f ? (1.0f / dt) : 0.0f, alloc);
    result.AddMember("delta_ms", dt * 1000.0f, alloc);
    result.AddMember("draw_calls", engine.pipeline()->LastDrawCalls(), alloc);
    result.AddMember("entity_count", CountValidEntities(registry), alloc);
    result.AddMember("time_since_startup", Time::TimeSinceStartup(), alloc);
    const char* state_str =
        dse::editor::GetEditorState() == dse::editor::EditorState::Play ? "play" :
        dse::editor::GetEditorState() == dse::editor::EditorState::Pause ? "pause" : "edit";
    result.AddMember("editor_state", rapidjson::Value(state_str, alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_list_tools ──────────────────────────────────────────────
// 自省：返回已注册的全部工具名，供用例 schema 校验、杜绝命名漂移。

static std::vector<std::string> g_registered_tool_names;

static JsonRpcResponse HandleListTools(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    rapidjson::Value arr(rapidjson::kArrayType);
    for (const auto& name : g_registered_tool_names) {
        arr.PushBack(rapidjson::Value(name.c_str(), alloc), alloc);
    }
    result.AddMember("tools", arr, alloc);
    result.AddMember("count", static_cast<int>(g_registered_tool_names.size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_set_active ────────────────────────────────────────

struct EditorDisabledTag {};

static JsonRpcResponse HandleEntitySetActive(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint())
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    if (!params.HasMember("active") || !params["active"].IsBool())
        return MakeToolError(-32602, "Missing required param: active (bool)");

    auto& registry = engine.pipeline()->world().registry();
    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    if (!registry.valid(entity))
        return MakeToolError(-32602, "Invalid entity_id");

    bool active = params["active"].GetBool();
    if (active) {
        if (registry.all_of<EditorDisabledTag>(entity))
            registry.remove<EditorDisabledTag>(entity);
    } else {
        registry.emplace_or_replace<EditorDisabledTag>(entity);
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", params["entity_id"].GetUint(), alloc);
    result.AddMember("active", active, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_entity_get_children ─────────────────────────────────────

static JsonRpcResponse HandleEntityGetChildren(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint())
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");

    auto& registry = engine.pipeline()->world().registry();
    auto parent_entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    if (!registry.valid(parent_entity))
        return MakeToolError(-32602, "Invalid entity_id");

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    rapidjson::Value children(rapidjson::kArrayType);
    auto view = registry.view<ParentComponent>();
    for (auto entity : view) {
        const auto& pc = view.get<ParentComponent>(entity);
        if (pc.parent == parent_entity) {
            rapidjson::Value child(rapidjson::kObjectType);
            child.AddMember("entity_id", static_cast<uint32_t>(entity), alloc);
            if (registry.all_of<EditorNameComponent>(entity)) {
                const auto& nc = registry.get<EditorNameComponent>(entity);
                child.AddMember("name", rapidjson::Value(nc.name.c_str(), alloc), alloc);
            }
            children.PushBack(child, alloc);
        }
    }
    result.AddMember("parent_id", params["entity_id"].GetUint(), alloc);
    result.AddMember("children", children, alloc);
    result.AddMember("count", static_cast<int>(children.Size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_script_attach ───────────────────────────────────────────

static JsonRpcResponse HandleScriptAttach(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("entity_id") || !params["entity_id"].IsUint())
        return MakeToolError(-32602, "Missing required param: entity_id (uint)");
    if (!params.HasMember("script_path") || !params["script_path"].IsString())
        return MakeToolError(-32602, "Missing required param: script_path (string)");

    auto& registry = engine.pipeline()->world().registry();
    auto entity = static_cast<entt::entity>(params["entity_id"].GetUint());
    if (!registry.valid(entity))
        return MakeToolError(-32602, "Invalid entity_id");

    std::string path = params["script_path"].GetString();
    std::string lang = "lua";
    if (params.HasMember("language") && params["language"].IsString())
        lang = params["language"].GetString();

    if (lang == "lua") {
        auto& sc = registry.emplace_or_replace<LuaScriptComponent>(entity);
        sc.script_path = path;
    } else if (lang == "csharp" || lang == "cs") {
        auto& sc = registry.emplace_or_replace<CSharpScriptComponent>(entity);
        sc.class_name = path;
        sc.enabled = true;
    } else if (lang == "blueprint") {
        auto& sc = registry.emplace_or_replace<BlueprintComponent>(entity);
        sc.blueprint_asset_path = path;
        sc.enabled = true;
    } else {
        auto& sc = registry.emplace_or_replace<ScriptComponent>(entity);
        sc.script_path = path;
        sc.enabled = true;
    }

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("entity_id", params["entity_id"].GetUint(), alloc);
    result.AddMember("script_path", rapidjson::Value(path.c_str(), alloc), alloc);
    result.AddMember("language", rapidjson::Value(lang.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_editor_pause ────────────────────────────────────────────

static JsonRpcResponse HandleEditorPause(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto state = dse::editor::GetEditorState();
    if (state == dse::editor::EditorState::Edit)
        return MakeToolError(-32600, "Cannot pause: editor is in edit mode, not play mode");

    const char* prev = state == dse::editor::EditorState::Play ? "play" : "pause";
    dse::editor::ToggleEditorPause();
    auto new_state = dse::editor::GetEditorState();
    const char* curr = new_state == dse::editor::EditorState::Play ? "play" :
                       new_state == dse::editor::EditorState::Pause ? "pause" : "edit";

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("previous_state", rapidjson::Value(prev, alloc), alloc);
    result.AddMember("editor_state", rapidjson::Value(curr, alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_scene_list ──────────────────────────────────────────────

static JsonRpcResponse HandleSceneList(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& pm = dse::editor::ProjectManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    rapidjson::Value scenes(rapidjson::kArrayType);
    if (pm.HasOpenProject()) {
        auto scene_dir = pm.GetSceneDir();
        if (std::filesystem::exists(scene_dir)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(scene_dir)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                if (ext == ".json" || ext == ".dscene") {
                    auto rel = std::filesystem::relative(entry.path(), pm.GetProjectRoot());
                    scenes.PushBack(rapidjson::Value(rel.string().c_str(), alloc), alloc);
                }
            }
        }
    }
    result.AddMember("scenes", scenes, alloc);
    result.AddMember("count", static_cast<int>(scenes.Size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_project_get_info ────────────────────────────────────────

static JsonRpcResponse HandleProjectGetInfo(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& pm = dse::editor::ProjectManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (!pm.HasOpenProject()) {
        result.AddMember("has_project", false, alloc);
        return MakeOk(std::move(result));
    }

    const auto& desc = pm.GetDescriptor();
    result.AddMember("has_project", true, alloc);
    result.AddMember("name", rapidjson::Value(desc.name.c_str(), alloc), alloc);
    result.AddMember("version", rapidjson::Value(desc.version.c_str(), alloc), alloc);
    result.AddMember("engine_version", rapidjson::Value(desc.engine_version.c_str(), alloc), alloc);
    result.AddMember("description", rapidjson::Value(desc.description.c_str(), alloc), alloc);
    result.AddMember("root", rapidjson::Value(pm.GetProjectRoot().string().c_str(), alloc), alloc);
    result.AddMember("default_scene", rapidjson::Value(desc.default_scene.c_str(), alloc), alloc);
    result.AddMember("asset_dir", rapidjson::Value(desc.asset_dir.c_str(), alloc), alloc);
    result.AddMember("scene_dir", rapidjson::Value(desc.scene_dir.c_str(), alloc), alloc);
    result.AddMember("script_dir", rapidjson::Value(desc.script_dir.c_str(), alloc), alloc);
    result.AddMember("entry_script", rapidjson::Value(desc.entry_script.c_str(), alloc), alloc);

    rapidjson::Value features(rapidjson::kArrayType);
    for (const auto& f : desc.features)
        features.PushBack(rapidjson::Value(f.c_str(), alloc), alloc);
    result.AddMember("features", features, alloc);

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_gizmo_set_mode ──────────────────────────────────────────

static int* g_gizmo_operation_ptr = nullptr;
static int* g_gizmo_mode_ptr = nullptr;

void SetGizmoPointers(int* op, int* mode) {
    g_gizmo_operation_ptr = op;
    g_gizmo_mode_ptr = mode;
}

static JsonRpcResponse HandleGizmoSetMode(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (params.HasMember("operation") && params["operation"].IsString() && g_gizmo_operation_ptr) {
        std::string op = params["operation"].GetString();
        if (op == "translate" || op == "move")   *g_gizmo_operation_ptr = 7;   // ImGuizmo::TRANSLATE
        else if (op == "rotate")                 *g_gizmo_operation_ptr = 120; // ImGuizmo::ROTATE
        else if (op == "scale")                  *g_gizmo_operation_ptr = 896; // ImGuizmo::SCALE
        result.AddMember("operation", rapidjson::Value(op.c_str(), alloc), alloc);
    }

    if (params.HasMember("space") && params["space"].IsString() && g_gizmo_mode_ptr) {
        std::string mode = params["space"].GetString();
        if (mode == "local")       *g_gizmo_mode_ptr = 0; // ImGuizmo::LOCAL
        else if (mode == "world")  *g_gizmo_mode_ptr = 1; // ImGuizmo::WORLD
        result.AddMember("space", rapidjson::Value(mode.c_str(), alloc), alloc);
    }

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_physics_raycast ─────────────────────────────────────────

static JsonRpcResponse HandlePhysicsRaycast(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("origin") || !params["origin"].IsArray() || params["origin"].Size() < 3)
        return MakeToolError(-32602, "Missing required param: origin ([x,y,z])");
    if (!params.HasMember("direction") || !params["direction"].IsArray() || params["direction"].Size() < 3)
        return MakeToolError(-32602, "Missing required param: direction ([x,y,z])");

    glm::vec3 origin = ParseVec3(params["origin"]);
    glm::vec3 direction = ParseVec3(params["direction"]);
    float max_dist = 1000.0f;
    if (params.HasMember("max_distance") && params["max_distance"].IsNumber())
        max_dist = params["max_distance"].GetFloat();

    std::string lua_code = "local r = DSE.Physics3D.Raycast("
        + std::to_string(origin.x) + "," + std::to_string(origin.y) + "," + std::to_string(origin.z) + ","
        + std::to_string(direction.x) + "," + std::to_string(direction.y) + "," + std::to_string(direction.z) + ","
        + std::to_string(max_dist) + ") "
        "return r";

    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    result.AddMember("output", rapidjson::Value(output.c_str(), alloc), alloc);

    rapidjson::Value orig(rapidjson::kArrayType);
    orig.PushBack(origin.x, alloc).PushBack(origin.y, alloc).PushBack(origin.z, alloc);
    result.AddMember("origin", orig, alloc);
    rapidjson::Value dir(rapidjson::kArrayType);
    dir.PushBack(direction.x, alloc).PushBack(direction.y, alloc).PushBack(direction.z, alloc);
    result.AddMember("direction", dir, alloc);
    result.AddMember("max_distance", max_dist, alloc);

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_physics_set_gravity ─────────────────────────────────────

static JsonRpcResponse HandlePhysicsSetGravity(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    float gx = 0.0f, gy = -9.81f, gz = 0.0f;
    if (params.HasMember("gravity") && params["gravity"].IsArray() && params["gravity"].Size() >= 3) {
        gx = params["gravity"][0].GetFloat();
        gy = params["gravity"][1].GetFloat();
        gz = params["gravity"][2].GetFloat();
    } else if (params.HasMember("y") && params["y"].IsNumber()) {
        gy = params["y"].GetFloat();
    }

    std::string lua_code = "DSE.Physics3D.SetGravity(" +
        std::to_string(gx) + "," + std::to_string(gy) + "," + std::to_string(gz) + ")";

    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    if (!output.empty()) result.AddMember("output", rapidjson::Value(output.c_str(), alloc), alloc);
    rapidjson::Value grav(rapidjson::kArrayType);
    grav.PushBack(gx, alloc).PushBack(gy, alloc).PushBack(gz, alloc);
    result.AddMember("gravity", grav, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_audio_play ──────────────────────────────────────────────

static JsonRpcResponse HandleAudioPlay(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    if (!params.HasMember("clip_path") || !params["clip_path"].IsString())
        return MakeToolError(-32602, "Missing required param: clip_path (string)");

    std::string clip = params["clip_path"].GetString();
    float volume = 1.0f;
    if (params.HasMember("volume") && params["volume"].IsNumber())
        volume = params["volume"].GetFloat();
    bool loop = false;
    if (params.HasMember("loop") && params["loop"].IsBool())
        loop = params["loop"].GetBool();

    std::string lua_code = "DSE.Audio.PlaySfx(\"" + clip + "\", " +
        std::to_string(volume) + ", " + (loop ? "true" : "false") + ")";

    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    result.AddMember("clip_path", rapidjson::Value(clip.c_str(), alloc), alloc);
    result.AddMember("volume", volume, alloc);
    result.AddMember("loop", loop, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_audio_stop ──────────────────────────────────────────────

static JsonRpcResponse HandleAudioStop(
    const rapidjson::Document& /*params*/,
    dse::runtime::EngineInstance& engine) {

    std::string lua_code = "DSE.Audio.StopAllSfx() DSE.Audio.StopBgm()";
    std::string output;
    bool ok = dse::runtime::ExecuteLuaString(lua_code.c_str(), &output);

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();
    result.AddMember("executed", ok, alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_camera_set_view ─────────────────────────────────────────

static JsonRpcResponse HandleCameraSetView(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& engine) {

    auto& registry = engine.pipeline()->world().registry();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (params.HasMember("entity_id") && params["entity_id"].IsUint()) {
        auto target = static_cast<entt::entity>(params["entity_id"].GetUint());
        if (!registry.valid(target))
            return MakeToolError(-32602, "Invalid entity_id");

        if (registry.all_of<TransformComponent>(target)) {
            const auto& tt = registry.get<TransformComponent>(target);
            // Find the first Camera3D and move it to look at the target
            auto cam_view = registry.view<Camera3DComponent, TransformComponent>();
            for (auto cam_entity : cam_view) {
                auto& cam_transform = cam_view.get<TransformComponent>(cam_entity);
                glm::vec3 offset(0.0f, 2.0f, 5.0f);
                cam_transform.position = tt.position + offset;
                cam_transform.dirty = true;
                result.AddMember("camera_entity", static_cast<uint32_t>(cam_entity), alloc);
                break;
            }
            rapidjson::Value pos(rapidjson::kArrayType);
            pos.PushBack(tt.position.x, alloc).PushBack(tt.position.y, alloc).PushBack(tt.position.z, alloc);
            result.AddMember("target_position", pos, alloc);
        }
    } else if (params.HasMember("position") && params["position"].IsArray()) {
        glm::vec3 pos = ParseVec3(params["position"]);
        auto cam_view = registry.view<Camera3DComponent, TransformComponent>();
        for (auto cam_entity : cam_view) {
            auto& cam_transform = cam_view.get<TransformComponent>(cam_entity);
            cam_transform.position = pos;
            cam_transform.dirty = true;
            result.AddMember("camera_entity", static_cast<uint32_t>(cam_entity), alloc);
            break;
        }
        rapidjson::Value p(rapidjson::kArrayType);
        p.PushBack(pos.x, alloc).PushBack(pos.y, alloc).PushBack(pos.z, alloc);
        result.AddMember("position", p, alloc);
    }

    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_asset_list ──────────────────────────────────────────────

static JsonRpcResponse HandleAssetList(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    auto& pm = dse::editor::ProjectManager::Get();
    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (!pm.HasOpenProject()) {
        result.AddMember("error", "No project open", alloc);
        return MakeOk(std::move(result));
    }

    std::string filter;
    if (params.HasMember("filter") && params["filter"].IsString())
        filter = params["filter"].GetString();

    std::string subdir;
    if (params.HasMember("directory") && params["directory"].IsString())
        subdir = params["directory"].GetString();

    auto asset_dir = pm.GetAssetDir();
    if (!subdir.empty())
        asset_dir = asset_dir / subdir;

    rapidjson::Value assets(rapidjson::kArrayType);
    if (std::filesystem::exists(asset_dir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(asset_dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (!filter.empty() && ext != filter) continue;
            auto rel = std::filesystem::relative(entry.path(), pm.GetProjectRoot());
            rapidjson::Value item(rapidjson::kObjectType);
            item.AddMember("path", rapidjson::Value(rel.string().c_str(), alloc), alloc);
            item.AddMember("extension", rapidjson::Value(ext.c_str(), alloc), alloc);
            item.AddMember("size", static_cast<int64_t>(entry.file_size()), alloc);
            assets.PushBack(item, alloc);
        }
    }
    result.AddMember("assets", assets, alloc);
    result.AddMember("count", static_cast<int>(assets.Size()), alloc);
    return MakeOk(std::move(result));
}

// ─── Tool: dsengine_asset_delete ────────────────────────────────────────────

static JsonRpcResponse HandleAssetDelete(
    const rapidjson::Document& params,
    dse::runtime::EngineInstance& /*engine*/) {

    if (!params.HasMember("path") || !params["path"].IsString())
        return MakeToolError(-32602, "Missing required param: path (string)");

    auto& pm = dse::editor::ProjectManager::Get();
    if (!pm.HasOpenProject())
        return MakeToolError(-32600, "No project open");

    std::string rel_path = params["path"].GetString();
    auto full_path = pm.GetProjectRoot() / rel_path;

    rapidjson::Document result;
    result.SetObject();
    auto& alloc = result.GetAllocator();

    if (!std::filesystem::exists(full_path)) {
        result.AddMember("deleted", false, alloc);
        result.AddMember("error", "File not found", alloc);
    } else {
        std::error_code ec;
        bool ok = std::filesystem::remove(full_path, ec);
        result.AddMember("deleted", ok, alloc);
        if (!ok)
            result.AddMember("error", rapidjson::Value(ec.message().c_str(), alloc), alloc);
    }
    result.AddMember("path", rapidjson::Value(rel_path.c_str(), alloc), alloc);
    return MakeOk(std::move(result));
}

// ─── 注册表 ─────────────────────────────────────────────────────────────────

struct ToolEntry {
    const char* method;
    ToolHandler handler;
};

static const ToolEntry kBuiltinTools[] = {
    { "dsengine_ping",                      HandlePing },
    { "dsengine_lua_execute",               HandleLuaExecute },
    { "dsengine_scene_get_state",           HandleSceneGetState },
    { "dsengine_entity_create",             HandleEntityCreate },
    { "dsengine_entity_delete",             HandleEntityDelete },
    { "dsengine_entity_batch_delete",       HandleEntityBatchDelete },
    { "dsengine_entity_modify",             HandleEntityModify },
    { "dsengine_entity_add_component",      HandleEntityAddComponent },
    { "dsengine_entity_remove_component",   HandleEntityRemoveComponent },
    { "dsengine_entity_get_components",     HandleEntityGetComponents },
    { "dsengine_script_create",             HandleScriptCreate },
    { "dsengine_editor_get_state",          HandleEditorGetState },
    { "dsengine_editor_play",               HandleEditorPlay },
    { "dsengine_editor_stop",               HandleEditorStop },
    { "dsengine_editor_undo",               HandleEditorUndo },
    { "dsengine_editor_redo",               HandleEditorRedo },
    { "dsengine_editor_screenshot",         HandleEditorScreenshot },
    { "dsengine_scene_save",                HandleSceneSave },
    { "dsengine_scene_load",                HandleSceneLoad },
    { "dsengine_asset_import",              HandleAssetImport },
    { "dsengine_material_create",           HandleMaterialCreate },
    { "dsengine_entity_get_state",          HandleEntityGetState },
    { "dsengine_entity_duplicate",          HandleEntityDuplicate },
    { "dsengine_prefab_save",               HandlePrefabSave },
    { "dsengine_prefab_instantiate",        HandlePrefabInstantiate },
    { "dsengine_scene_new",                 HandleSceneNew },
    { "dsengine_entity_reparent",           HandleEntityReparent },
    { "dsengine_entity_find_by_name",       HandleEntityFindByName },
    { "dsengine_undo_history",              HandleUndoHistory },
    { "dsengine_selection_get",             HandleSelectionGet },
    { "dsengine_selection_set",             HandleSelectionSet },
    { "dsengine_selection_clear",           HandleSelectionClear },
    { "dsengine_project_open",              HandleProjectOpen },
    { "dsengine_editor_quit",               HandleEditorQuit },
    { "dsengine_editor_idle",               HandleEditorIdle },
    { "dsengine_editor_get_metrics",        HandleEditorGetMetrics },
    { "dsengine_list_tools",                HandleListTools },
    { "dsengine_entity_set_active",         HandleEntitySetActive },
    { "dsengine_entity_get_children",       HandleEntityGetChildren },
    { "dsengine_script_attach",             HandleScriptAttach },
    { "dsengine_editor_pause",              HandleEditorPause },
    { "dsengine_scene_list",                HandleSceneList },
    { "dsengine_project_get_info",          HandleProjectGetInfo },
    { "dsengine_gizmo_set_mode",            HandleGizmoSetMode },
    { "dsengine_physics_raycast",           HandlePhysicsRaycast },
    { "dsengine_physics_set_gravity",       HandlePhysicsSetGravity },
    { "dsengine_audio_play",                HandleAudioPlay },
    { "dsengine_audio_stop",                HandleAudioStop },
    { "dsengine_camera_set_view",           HandleCameraSetView },
    { "dsengine_asset_list",                HandleAssetList },
    { "dsengine_asset_delete",              HandleAssetDelete },
};

void RegisterBuiltinTools(ControlServer& server) {
    g_registered_tool_names.clear();
    for (const auto& tool : kBuiltinTools) {
        server.RegisterTool(tool.method, tool.handler);
        g_registered_tool_names.emplace_back(tool.method);
    }
}

} // namespace dse::editor

/**
 * @file lua_binding_cabi_gap.cpp
 * @brief 补齐手写 C ABI 中尚未映射到 Lua 的函数。
 *
 * 此文件是"差距修补"层 — 填补 dse_api.h 中已有但 Lua 端遗漏的函数。
 * 随着手写 C ABI 逐步稳定，此文件不应增长；新组件应通过 codegen 自动生成绑定。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <cstring>
#include <string>
#include <vector>

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(lua_State* L, int i) {
    return static_cast<uint32_t>(luaL_checkinteger(L, i));
}

// ============================================================
// dse.api_version() — ABI 版本检查
// ============================================================
int L_DseApiVersion(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(dse_api_version()));
    return 1;
}

// ============================================================
// Transform
// ============================================================
int L_EcsTransformAdd(lua_State* L) {
    uint32_t e = EID(L, 1);
    float x = helper::CheckFloat(L, 2);
    float y = helper::CheckFloat(L, 3);
    float z = helper::CheckFloat(L, 4);
    float sx = helper::OptFloat(L, 5, 1.0f);
    float sy = helper::OptFloat(L, 6, 1.0f);
    float sz = helper::OptFloat(L, 7, 1.0f);
    dse_transform_add(e, x, y, z, sx, sy, sz);
    return 0;
}

// ============================================================
// App — 补齐 dse_app_get_delta_time / get_target_fps / get_time
// ============================================================
int L_AppGetDeltaTime(lua_State* L) {
    helper::PushFloat(L, dse_app_get_delta_time());
    return 1;
}

int L_AppGetTargetFps(lua_State* L) {
    helper::PushFloat(L, dse_app_get_target_fps());
    return 1;
}

int L_AppGetTime(lua_State* L) {
    helper::PushFloat(L, dse_app_get_time());
    return 1;
}

// ============================================================
// Input — mouse_scroll / touch
// ============================================================
int L_AppGetMouseScroll(lua_State* L) {
    helper::PushFloat(L, dse_input_get_mouse_scroll());
    return 1;
}

int L_AppGetTouchCount(lua_State* L) {
    helper::PushInt(L, dse_input_get_touch_count());
    return 1;
}

int L_AppGetTouch(lua_State* L) {
    int index = helper::CheckInt(L, 1);
    float x = 0, y = 0;
    int phase = 0;
    dse_input_get_touch(index, &x, &y, &phase);
    helper::PushFloat(L, x);
    helper::PushFloat(L, y);
    helper::PushInt(L, phase);
    return 3;
}

// ============================================================
// Audio
// ============================================================
int L_AudioFadeOutAllSfx(lua_State* L) {
    dse_audio_fade_out_all_sfx(helper::CheckFloat(L, 1));
    return 0;
}

int L_AudioSourceIsPlaying(lua_State* L) {
    helper::PushInt(L, dse_audio_source_is_playing(EID(L, 1)));
    return 1;
}

// ============================================================
// Animation 3D — blend tree / layer
// ============================================================
int L_EcsAnim3DGetBlendParam(lua_State* L) {
    helper::PushFloat(L, dse_anim3d_get_blend_param(EID(L, 1)));
    return 1;
}

int L_EcsAnim3DSetBlendParam(lua_State* L) {
    dse_anim3d_set_blend_param(EID(L, 1), helper::CheckFloat(L, 2));
    return 0;
}

int L_EcsAnim3DGetLayerWeight(lua_State* L) {
    helper::PushFloat(L, dse_anim3d_get_layer_weight(EID(L, 1), helper::CheckInt(L, 2)));
    return 1;
}

int L_EcsAnim3DSetLayerWeight(lua_State* L) {
    dse_anim3d_set_layer_weight(EID(L, 1), helper::CheckInt(L, 2), helper::CheckFloat(L, 3));
    return 0;
}

int L_EcsAnim3DSetLayerMask(lua_State* L) {
    uint32_t e = EID(L, 1);
    int layer = helper::CheckInt(L, 2);
    // Lua: 传入字符串数组 { "bone1", "bone2", ... }
    luaL_checktype(L, 3, LUA_TTABLE);
    int count = static_cast<int>(luaL_len(L, 3));
    std::vector<const char*> bones;
    std::vector<std::string> bone_storage;
    bones.reserve(count);
    bone_storage.reserve(count);
    for (int i = 1; i <= count; ++i) {
        lua_geti(L, 3, i);
        bone_storage.push_back(luaL_checkstring(L, -1));
        lua_pop(L, 1);
    }
    for (auto& s : bone_storage) bones.push_back(s.c_str());
    dse_anim3d_set_layer_mask(e, layer, bones.data(), count);
    return 0;
}

int L_EcsAnim3DSetBlendTree1D(lua_State* L) {
    uint32_t e = EID(L, 1);
    // Lua: clips_table, thresholds_table, speeds_table
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TTABLE);
    int count = static_cast<int>(luaL_len(L, 2));
    std::vector<const char*> clips;
    std::vector<float> thresholds(count);
    std::vector<float> speeds(count);
    std::vector<std::string> clip_storage;
    clip_storage.reserve(count);
    clips.reserve(count);
    for (int i = 1; i <= count; ++i) {
        lua_geti(L, 2, i);
        clip_storage.push_back(luaL_checkstring(L, -1));
        lua_pop(L, 1);
        lua_geti(L, 3, i);
        thresholds[i - 1] = helper::CheckFloat(L, -1);
        lua_pop(L, 1);
        lua_geti(L, 4, i);
        speeds[i - 1] = helper::OptFloat(L, -1, 1.0f);
        lua_pop(L, 1);
    }
    for (auto& s : clip_storage) clips.push_back(s.c_str());
    dse_anim3d_set_blend_tree_1d(e, clips.data(), thresholds.data(), speeds.data(), count);
    return 0;
}

// ============================================================
// Character
// ============================================================
int L_EcsCharacterCheckGround(lua_State* L) {
    float normal[3] = {0, 0, 0};
    int hit = dse_character_check_ground(EID(L, 1), normal);
    helper::PushInt(L, hit);
    helper::PushFloat(L, normal[0]);
    helper::PushFloat(L, normal[1]);
    helper::PushFloat(L, normal[2]);
    return 4;
}

// ============================================================
// Decal
// ============================================================
int L_EcsDecalAdd(lua_State* L) {
    dse_decal_add(EID(L, 1), static_cast<uint32_t>(helper::CheckInt(L, 2)));
    return 0;
}

int L_EcsDecalSet(lua_State* L) {
    dse_decal_set(EID(L, 1),
                  helper::CheckFloat(L, 2),
                  helper::CheckFloat(L, 3),
                  helper::CheckFloat(L, 4),
                  helper::CheckFloat(L, 5),
                  helper::OptFloat(L, 6, 0.0f));
    return 0;
}

// ============================================================
// Mesh
// ============================================================
int L_EcsMeshRendererAdd(lua_State* L) {
    dse_mesh_renderer_add(EID(L, 1), helper::CheckString(L, 2));
    return 0;
}

int L_EcsMeshSetDepthState(lua_State* L) {
    dse_mesh_set_depth_state(EID(L, 1), helper::CheckInt(L, 2), helper::CheckInt(L, 3));
    return 0;
}

int L_EcsMeshSetEmissive(lua_State* L) {
    dse_mesh_set_emissive(EID(L, 1),
                          helper::CheckFloat(L, 2),
                          helper::CheckFloat(L, 3),
                          helper::CheckFloat(L, 4));
    return 0;
}

int L_EcsMeshSetMaterial(lua_State* L) {
    dse_mesh_set_material(EID(L, 1), helper::CheckString(L, 2));
    return 0;
}

int L_EcsMeshSetMaterialScalar(lua_State* L) {
    dse_mesh_set_material_scalar(EID(L, 1), helper::CheckString(L, 2), helper::CheckFloat(L, 3));
    return 0;
}

int L_EcsMeshSetTextureHandle(lua_State* L) {
    dse_mesh_set_texture_handle(EID(L, 1), helper::CheckString(L, 2),
                                static_cast<uint32_t>(helper::CheckInt(L, 3)));
    return 0;
}

// ============================================================
// Physics3D — boxcast / spherecast / rigidbody extras
// ============================================================
int L_Physics3DBoxcast(lua_State* L) {
    float ox = helper::CheckFloat(L, 1);
    float oy = helper::CheckFloat(L, 2);
    float oz = helper::CheckFloat(L, 3);
    float dx = helper::CheckFloat(L, 4);
    float dy = helper::CheckFloat(L, 5);
    float dz = helper::CheckFloat(L, 6);
    float hx = helper::CheckFloat(L, 7);
    float hy = helper::CheckFloat(L, 8);
    float hz = helper::CheckFloat(L, 9);
    float max_dist = helper::CheckFloat(L, 10);
    uint32_t entity = 0;
    float point[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    float dist = 0;
    int hit = dse_physics3d_boxcast(ox, oy, oz, dx, dy, dz, hx, hy, hz, max_dist,
                                    &entity, point, normal, &dist);
    helper::PushInt(L, hit);
    helper::PushInt(L, static_cast<int>(entity));
    helper::PushFloat(L, point[0]);
    helper::PushFloat(L, point[1]);
    helper::PushFloat(L, point[2]);
    helper::PushFloat(L, normal[0]);
    helper::PushFloat(L, normal[1]);
    helper::PushFloat(L, normal[2]);
    helper::PushFloat(L, dist);
    return 9;
}

int L_Physics3DSpherecast(lua_State* L) {
    float ox = helper::CheckFloat(L, 1);
    float oy = helper::CheckFloat(L, 2);
    float oz = helper::CheckFloat(L, 3);
    float dx = helper::CheckFloat(L, 4);
    float dy = helper::CheckFloat(L, 5);
    float dz = helper::CheckFloat(L, 6);
    float radius = helper::CheckFloat(L, 7);
    float max_dist = helper::CheckFloat(L, 8);
    uint32_t entity = 0;
    float point[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    float dist = 0;
    int hit = dse_physics3d_spherecast(ox, oy, oz, dx, dy, dz, radius, max_dist,
                                       &entity, point, normal, &dist);
    helper::PushInt(L, hit);
    helper::PushInt(L, static_cast<int>(entity));
    helper::PushFloat(L, point[0]);
    helper::PushFloat(L, point[1]);
    helper::PushFloat(L, point[2]);
    helper::PushFloat(L, normal[0]);
    helper::PushFloat(L, normal[1]);
    helper::PushFloat(L, normal[2]);
    helper::PushFloat(L, dist);
    return 9;
}

int L_EcsRigidBody3DSetKinematic(lua_State* L) {
    dse_rigidbody3d_set_kinematic(EID(L, 1), helper::CheckInt(L, 2));
    return 0;
}

int L_EcsRigidBody3DGetLinearDamping(lua_State* L) {
    helper::PushFloat(L, dse_rigidbody3d_get_linear_damping(EID(L, 1)));
    return 1;
}

int L_EcsRigidBody3DSetLinearDamping(lua_State* L) {
    dse_rigidbody3d_set_linear_damping(EID(L, 1), helper::CheckFloat(L, 2));
    return 0;
}

int L_EcsRigidBody3DGetAngularDamping(lua_State* L) {
    helper::PushFloat(L, dse_rigidbody3d_get_angular_damping(EID(L, 1)));
    return 1;
}

int L_EcsRigidBody3DSetAngularDamping(lua_State* L) {
    dse_rigidbody3d_set_angular_damping(EID(L, 1), helper::CheckFloat(L, 2));
    return 0;
}

int L_EcsRigidBody3DAddForceAtPosition(lua_State* L) {
    dse_rigidbody3d_add_force_at_position(EID(L, 1),
        helper::CheckFloat(L, 2), helper::CheckFloat(L, 3), helper::CheckFloat(L, 4),
        helper::CheckFloat(L, 5), helper::CheckFloat(L, 6), helper::CheckFloat(L, 7));
    return 0;
}

// ============================================================
// Rendering — probes / post_process
// ============================================================
int L_EcsRenderingSetLightProbe(lua_State* L) {
    dse_rendering_set_light_probe(EID(L, 1), helper::CheckFloat(L, 2));
    return 0;
}

int L_EcsRenderingSetReflectionProbe(lua_State* L) {
    dse_rendering_set_reflection_probe(EID(L, 1),
        helper::CheckFloat(L, 2), helper::OptInt(L, 3, 128));
    return 0;
}

int L_EcsPostProcessGetState(lua_State* L) {
    int enabled = 0, bloom = 0, ssao = 0, ssr = 0, fxaa = 0, dof = 0;
    dse_post_process_get_state(EID(L, 1), &enabled, &bloom, &ssao, &ssr, &fxaa, &dof);
    helper::PushInt(L, enabled);
    helper::PushInt(L, bloom);
    helper::PushInt(L, ssao);
    helper::PushInt(L, ssr);
    helper::PushInt(L, fxaa);
    helper::PushInt(L, dof);
    return 6;
}

// ============================================================
// UI
// ============================================================
int L_UiIsHovered(lua_State* L) {
    helper::PushInt(L, dse_ui_is_hovered(EID(L, 1)));
    return 1;
}

int L_UiIsPressed(lua_State* L) {
    helper::PushInt(L, dse_ui_is_pressed(EID(L, 1)));
    return 1;
}

// ============================================================
// Localization
// ============================================================
int L_L10nLoad(lua_State* L) {
    int ok = dse_l10n_load(helper::CheckString(L, 1), helper::CheckString(L, 2));
    helper::PushInt(L, ok);
    return 1;
}

} // namespace

void RegisterCabiGapBindings(lua_State* L) {
    // dse 顶层 — api_version
    lua_getglobal(L, "dse");
    helper::RegisterFn(L, "api_version", L_DseApiVersion);
    lua_pop(L, 1);

    // dse.app — 补齐 input/app 缺失函数
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "app");
    helper::RegisterBindings(L, {
        {"get_delta_time",  L_AppGetDeltaTime},
        {"get_target_fps",  L_AppGetTargetFps},
        {"get_time",        L_AppGetTime},
        {"get_mouse_scroll", L_AppGetMouseScroll},
        {"get_touch_count", L_AppGetTouchCount},
        {"get_touch",       L_AppGetTouch},
    });
    lua_pop(L, 2);

    // dse.audio
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "audio");
    helper::RegisterBindings(L, {
        {"fade_out_all_sfx",   L_AudioFadeOutAllSfx},
        {"source_is_playing",  L_AudioSourceIsPlaying},
    });
    lua_pop(L, 2);

    // dse.ecs — 补齐所有 ECS 相关缺失函数
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"transform_add",                L_EcsTransformAdd},
        // Animation 3D
        {"anim3d_get_blend_param",       L_EcsAnim3DGetBlendParam},
        {"anim3d_set_blend_param",       L_EcsAnim3DSetBlendParam},
        {"anim3d_get_layer_weight",      L_EcsAnim3DGetLayerWeight},
        {"anim3d_set_layer_weight",      L_EcsAnim3DSetLayerWeight},
        {"anim3d_set_layer_mask",        L_EcsAnim3DSetLayerMask},
        {"anim3d_set_blend_tree_1d",     L_EcsAnim3DSetBlendTree1D},
        // Character
        {"character_check_ground",       L_EcsCharacterCheckGround},
        // Decal
        {"decal_add",                    L_EcsDecalAdd},
        {"decal_set",                    L_EcsDecalSet},
        // Mesh
        {"mesh_renderer_add",            L_EcsMeshRendererAdd},
        {"mesh_set_depth_state",         L_EcsMeshSetDepthState},
        {"mesh_set_emissive",            L_EcsMeshSetEmissive},
        {"mesh_set_material",            L_EcsMeshSetMaterial},
        {"mesh_set_material_scalar",     L_EcsMeshSetMaterialScalar},
        {"mesh_set_texture_handle",      L_EcsMeshSetTextureHandle},
        // Physics3D
        {"physics_3d_boxcast",           L_Physics3DBoxcast},
        {"physics_3d_spherecast",        L_Physics3DSpherecast},
        {"rigidbody_3d_set_kinematic",          L_EcsRigidBody3DSetKinematic},
        {"rigidbody_3d_get_linear_damping",     L_EcsRigidBody3DGetLinearDamping},
        {"rigidbody_3d_set_linear_damping",     L_EcsRigidBody3DSetLinearDamping},
        {"rigidbody_3d_get_angular_damping",    L_EcsRigidBody3DGetAngularDamping},
        {"rigidbody_3d_set_angular_damping",    L_EcsRigidBody3DSetAngularDamping},
        {"rigidbody_3d_add_force_at_position",  L_EcsRigidBody3DAddForceAtPosition},
        // Rendering
        {"set_light_probe",              L_EcsRenderingSetLightProbe},
        {"set_reflection_probe",         L_EcsRenderingSetReflectionProbe},
        {"post_process_get_state",       L_EcsPostProcessGetState},
    });
    lua_pop(L, 2);

    // dse.ui
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ui");
    helper::RegisterBindings(L, {
        {"is_hovered", L_UiIsHovered},
        {"is_pressed", L_UiIsPressed},
    });
    lua_pop(L, 2);

    // dse.localization
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "localization");
    helper::RegisterFn(L, "load", L_L10nLoad);
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding

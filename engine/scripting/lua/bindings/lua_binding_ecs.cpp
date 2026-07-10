/**
 * @file lua_binding_ecs.cpp
 * @brief ECS Lua binding dispatcher + table-to-array functions (ex-cabi_gap)
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace dse::runtime::lua_binding {
namespace {

// Table-to-array functions (merged from lua_binding_cabi_gap.cpp)
inline uint32_t EID(lua_State* L, int i) {
    return static_cast<uint32_t>(luaL_checkinteger(L, i));
}

// 可选浮点：缺省/nil => NaN（C ABI 约定为“保持当前值/取默认”）。
inline float OptF(lua_State* L, int i) {
    return lua_isnoneornil(L, i) ? std::nanf("") : static_cast<float>(luaL_checknumber(L, i));
}

int L_EcsAnim3DSetLayerMask(lua_State* L) {
    uint32_t e = EID(L, 1);
    int layer = helper::CheckInt(L, 2);
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

// ---- Jiggle Bone（乳摇）运行期列表操作 ----
int L_EcsJiggleAddComponent(lua_State* L) { dse_jiggle_add_component(EID(L, 1)); return 0; }
int L_EcsJiggleRemoveComponent(lua_State* L) { dse_jiggle_remove_component(EID(L, 1)); return 0; }
int L_EcsJiggleClearBones(lua_State* L) { dse_jiggle_clear_bones(EID(L, 1)); return 0; }
int L_EcsJiggleClearColliders(lua_State* L) { dse_jiggle_clear_colliders(EID(L, 1)); return 0; }

int L_EcsJiggleGetBoneCount(lua_State* L) {
    lua_pushinteger(L, dse_jiggle_get_bone_count(EID(L, 1)));
    return 1;
}
int L_EcsJiggleGetColliderCount(lua_State* L) {
    lua_pushinteger(L, dse_jiggle_get_collider_count(EID(L, 1)));
    return 1;
}

// add_bone(e, name [, stiffness, damping, gravity, bone_length]) -> index
int L_EcsJiggleAddBone(lua_State* L) {
    uint32_t e = EID(L, 1);
    const char* name = luaL_checkstring(L, 2);
    int idx = dse_jiggle_add_bone(e, name, OptF(L, 3), OptF(L, 4), OptF(L, 5), OptF(L, 6));
    lua_pushinteger(L, idx);
    return 1;
}

// set_bone_params(e, index [, stiffness, damping, gravity, bone_length])
int L_EcsJiggleSetBoneParams(lua_State* L) {
    uint32_t e = EID(L, 1);
    int index = helper::CheckInt(L, 2);
    dse_jiggle_set_bone_params(e, index, OptF(L, 3), OptF(L, 4), OptF(L, 5), OptF(L, 6));
    return 0;
}

// set_bone_gravity_dir(e, index, x, y, z)
int L_EcsJiggleSetBoneGravityDir(lua_State* L) {
    uint32_t e = EID(L, 1);
    int index = helper::CheckInt(L, 2);
    dse_jiggle_set_bone_gravity_dir(e, index,
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_checknumber(L, 5)));
    return 0;
}

// add_collider(e, name, cx, cy, cz [, radius]) -> index
int L_EcsJiggleAddCollider(lua_State* L) {
    uint32_t e = EID(L, 1);
    const char* name = lua_isnoneornil(L, 2) ? nullptr : luaL_checkstring(L, 2);
    int idx = dse_jiggle_add_collider(e, name,
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_checknumber(L, 5)),
        OptF(L, 6));
    lua_pushinteger(L, idx);
    return 1;
}

} // namespace

void RegisterEcsBindings(lua_State* L) {
    int _top = lua_gettop(L);

    RegisterEcsCoreBindings(L);
    RegisterEcsRenderingBindings(L);
    RegisterEcsPhysics2DBindings(L);
    RegisterEcsPhysics3DBindings(L);
    RegisterEcsAnimationBindings(L);
    RegisterEcsParticlesBindings(L);
    RegisterEcsGameplay3DBindings(L);
    Register2DSystemsBindings(L);
    // Table-to-array functions (ex-cabi_gap, registered directly on ecs table)
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"anim3d_set_layer_mask",    L_EcsAnim3DSetLayerMask},
        {"anim3d_set_blend_tree_1d", L_EcsAnim3DSetBlendTree1D},
        {"jiggle_add_component",     L_EcsJiggleAddComponent},
        {"jiggle_remove_component",  L_EcsJiggleRemoveComponent},
        {"jiggle_clear_bones",       L_EcsJiggleClearBones},
        {"jiggle_clear_colliders",   L_EcsJiggleClearColliders},
        {"jiggle_get_bone_count",    L_EcsJiggleGetBoneCount},
        {"jiggle_get_collider_count",L_EcsJiggleGetColliderCount},
        {"jiggle_add_bone",          L_EcsJiggleAddBone},
        {"jiggle_set_bone_params",   L_EcsJiggleSetBoneParams},
        {"jiggle_set_bone_gravity_dir", L_EcsJiggleSetBoneGravityDir},
        {"jiggle_add_collider",      L_EcsJiggleAddCollider},
    });
    lua_remove(L, -2); // remove dse, keep ecs on top for component gen bindings

    // Codegen 生成的组件属性绑定（全量注册�?
    RegisterTransformComponentGenBindings(L);
    RegisterCamera3DComponentGenBindings(L);
    RegisterDirectionalLight3DComponentGenBindings(L);
    RegisterPointLightComponentGenBindings(L);
    RegisterMeshRendererComponentGenBindings(L);
    RegisterSpotLightComponentGenBindings(L);
    RegisterSkyLightComponentGenBindings(L);
    RegisterTreeComponentGenBindings(L);
    RegisterTerrainTileManagerComponentGenBindings(L);
    RegisterDynamicObstacleComponentGenBindings(L);
    RegisterNavMeshAutoRebakeComponentGenBindings(L);
    RegisterPostProcessComponentGenBindings(L);
    RegisterAnimator3DComponentGenBindings(L);
    RegisterDecalComponentGenBindings(L);
    RegisterSkyboxComponentGenBindings(L);
    RegisterFreeCameraControllerComponentGenBindings(L);
    RegisterSubSceneComponentGenBindings(L);
    RegisterBoundingBoxComponentGenBindings(L);
    RegisterWaterComponentGenBindings(L);
    RegisterLightProbeComponentGenBindings(L);
    RegisterReflectionProbeComponentGenBindings(L);
    RegisterGIProbeVolumeComponentGenBindings(L);
    RegisterFoliageComponentGenBindings(L);
    RegisterAtmosphereComponentGenBindings(L);
    RegisterVolumetricCloudComponentGenBindings(L);
    RegisterDayNightCycleComponentGenBindings(L);
    RegisterHairComponentGenBindings(L);
    RegisterImpostorComponentGenBindings(L);
    RegisterStreamingOriginComponentGenBindings(L);
    RegisterWorldPartitionConfigComponentGenBindings(L);
    RegisterHLODConfigComponentGenBindings(L);
    RegisterVirtualTextureComponentGenBindings(L);
    RegisterLightmapComponentGenBindings(L);
    RegisterRigidBody3DComponentGenBindings(L);
    RegisterBoxCollider3DComponentGenBindings(L);
    RegisterSphereCollider3DComponentGenBindings(L);
    RegisterCapsuleCollider3DComponentGenBindings(L);
    RegisterMeshCollider3DComponentGenBindings(L);
    RegisterCharacterController3DComponentGenBindings(L);
    RegisterJoint3DComponentGenBindings(L);
    RegisterRagdollComponentGenBindings(L);
    RegisterSoftBodyComponentGenBindings(L);
    RegisterVehicleComponentGenBindings(L);
    RegisterRopeComponentGenBindings(L);
    RegisterBuoyancyComponentGenBindings(L);
    RegisterCharacterMovementStateGenBindings(L);
    RegisterCharacterMovementConfigGenBindings(L);
    RegisterSpringArm3DComponentGenBindings(L);
    RegisterPlayerControllerComponentGenBindings(L);
    RegisterJiggleBoneComponentGenBindings(L);
    lua_pop(L, 1); // pop ecs table
    RegisterFreeGapBindings(L);
    RegisterAudioBindings(L);
    RegisterDSSLBindings(L);
    RegisterEcsRenderingCameraBindings(L);
    RegisterEcsRenderingFxBindings(L);
    RegisterEcsRenderingLightBindings(L);
    RegisterEcsRenderingMeshBindings(L);
    RegisterEcsRenderingPostBindings(L);
    RegisterEcsRenderingTerrainBindings(L);
    RegisterFontBindings(L);
    RegisterLocalizationBindings(L);
    RegisterMeshletBindings(L);
#ifdef DSE_ENABLE_NAVMESH
    RegisterNavigationBindings(L);
#endif
    RegisterOpenWorldBindings(L);
    RegisterOpenWorldP2P5Bindings(L);
    RegisterSpineBindings(L);
    RegisterStreamingBindings(L);
    RegisterUiBindings(L);
    RegisterFreeFn_ui(L);
    RegisterFreeFn_localization(L);
    RegisterFreeFn_ecs_gap(L);
    RegisterFreeFn_audio(L);
    RegisterFreeFn_app(L);
    RegisterFreeFn_api_core(L);
    lua_settop(L, _top);
}

} // namespace dse::runtime::lua_binding

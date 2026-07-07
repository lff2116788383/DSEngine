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

#include <cstring>
#include <string>
#include <vector>

namespace dse::runtime::lua_binding {
namespace {

// Table-to-array functions (merged from lua_binding_cabi_gap.cpp)
inline uint32_t EID(lua_State* L, int i) {
    return static_cast<uint32_t>(luaL_checkinteger(L, i));
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

} // namespace

void RegisterEcsBindings(lua_State* L) {
    lua_newtable(L);

    RegisterEcsCoreBindings(L);
    RegisterEcsRenderingBindings(L);
    RegisterEcsPhysics2DBindings(L);
    RegisterEcsPhysics3DBindings(L);
    RegisterEcsAnimationBindings(L);
    RegisterEcsParticlesBindings(L);
    RegisterEcsGameplay3DBindings(L);
    Register2DSystemsBindings(L);
    // Table-to-array functions (ex-cabi_gap, registered directly on ecs table)
    helper::RegisterBindings(L, {
        {"anim3d_set_layer_mask",    L_EcsAnim3DSetLayerMask},
        {"anim3d_set_blend_tree_1d", L_EcsAnim3DSetBlendTree1D},
    });

    // Codegen 生成的组件属性绑定（全量注册）
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
}

} // namespace dse::runtime::lua_binding

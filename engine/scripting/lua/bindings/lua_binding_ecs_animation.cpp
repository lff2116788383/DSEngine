/**
 * @file lua_binding_ecs_animation.cpp
 * @brief ECS Lua 绑定 — 2D 动画 + 3D 骨骼动画 / FSM。薄包装委托至手写 C ABI（dse_anim*）。
 *
 * 本文件仅负责 Lua 参数读取与默认值解析，所有组件操作下沉到 dse_api_animation.cpp 的
 * C ABI。浮点可选项以 NaN 传入表示「保持当前值」；数组以扁平指针 + count 传入。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

/// Entity → C ABI uint32_t（entt id）。
inline uint32_t EID(Entity e) {
    return static_cast<uint32_t>(static_cast<entt::id_type>(e));
}

// ============================================================
// 2D 动画
// ============================================================

int L_EcsAddAnimator(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_anim2d_add(EID(e));
    return 0;
}

int L_EcsAddAnimationState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    float fps = helper::CheckFloat(L, 3);
    bool loop = helper::CheckBool(L, 4);
    std::vector<uint32_t> handles;
    if (lua_istable(L, 5)) {
        int len = static_cast<int>(lua_rawlen(L, 5));
        handles.reserve(static_cast<size_t>(len));
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 5, i);
            handles.push_back(static_cast<uint32_t>(lua_tointeger(L, -1)));
            lua_pop(L, 1);
        }
    }
    dse_anim2d_add_state(EID(e), state_name, fps, loop ? 1 : 0,
                         handles.empty() ? nullptr : handles.data(),
                         static_cast<int>(handles.size()));
    return 0;
}

int L_EcsAddAnimationEvent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    float normalized_time = helper::CheckFloat(L, 3);
    const char* event_name = luaL_checkstring(L, 4);
    dse_anim2d_add_event(EID(e), state_name, normalized_time, event_name);
    return 0;
}

int L_EcsPlayAnimation(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    dse_anim2d_play(EID(e), state_name);
    return 0;
}

int L_EcsPlayAnimationSegment(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int start_frame = helper::CheckInt(L, 2);
    int end_frame = helper::CheckInt(L, 3);
    bool loop = helper::CheckBool(L, 4);
    dse_anim2d_play_segment(EID(e), start_frame, end_frame, loop ? 1 : 0);
    return 0;
}

int L_EcsPopAnimationEvent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    char buf[256];
    dse_anim2d_pop_event(EID(e), buf, static_cast<int>(sizeof(buf)));
    lua_pushstring(L, buf);
    return 1;
}

// ============================================================
// 3D 骨骼动画
// ============================================================

int L_EcsAddAnimator3D(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* danim_path = luaL_optstring(L, 2, "");
    const char* dskel_path = luaL_optstring(L, 3, "");
    dse_anim3d_add(EID(e), danim_path, dskel_path);
    return 0;
}

int L_EcsSetAnimator3DState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = (lua_gettop(L) >= 2 && lua_isstring(L, 2))
                                 ? lua_tostring(L, 2) : nullptr;
    float speed = (lua_gettop(L) >= 3) ? helper::CheckFloat(L, 3) : NAN;
    int loop = (lua_gettop(L) >= 4) ? (helper::CheckBool(L, 4) ? 1 : 0) : -1;
    dse_anim3d_set_state(EID(e), state_name, speed, loop);
    return 0;
}

int L_EcsGetAnimator3DState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    char state[256];
    float norm = 0.0f, time = 0.0f, speed = 0.0f;
    int loop = 0, transitioning = 0, bone_count = 0, has_skel = 0;
    int found = dse_anim3d_get_state(EID(e), state, static_cast<int>(sizeof(state)),
                                     &norm, &time, &speed, &loop, &transitioning,
                                     &bone_count, &has_skel);
    if (!found) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    lua_pushstring(L, state);
    helper::PushFloat(L, norm);
    helper::PushFloat(L, time);
    helper::PushFloat(L, speed);
    helper::PushBool(L, loop != 0);
    helper::PushBool(L, transitioning != 0);
    helper::PushInt(L, bone_count);
    helper::PushBool(L, has_skel != 0);
    return 9;
}

int L_EcsInitAnimator3DStateMachine(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_anim3d_init_fsm(EID(e));
    return 0;
}

int L_EcsAddAnimator3DState(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* state_name = luaL_checkstring(L, 2);
    const char* danim_path = luaL_checkstring(L, 3);
    bool loop = helper::CheckBool(L, 4);
    float speed = helper::OptFloat(L, 5, 1.0f);
    dse_anim3d_add_fsm_state(EID(e), state_name, danim_path, loop ? 1 : 0, speed);
    return 0;
}

int L_EcsAddAnimator3DTransition(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* from_state = luaL_checkstring(L, 2);
    const char* to_state = luaL_checkstring(L, 3);
    float transition_duration = helper::OptFloat(L, 4, 0.25f);
    bool has_exit_time = helper::CheckBool(L, 5);
    float exit_time = helper::OptFloat(L, 6, 1.0f);

    // 读取条件表（Arg 7）→ 并行扁平数组。
    std::vector<std::string> name_store;
    std::vector<int> modes;
    std::vector<float> thresholds;
    std::vector<int> int_vals;
    if (lua_istable(L, 7)) {
        lua_pushnil(L);
        while (lua_next(L, 7) != 0) {
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1);
                name_store.emplace_back(lua_isstring(L, -1) ? lua_tostring(L, -1) : "");
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 2);
                modes.push_back(static_cast<int>(lua_tointeger(L, -1)));
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 3);
                thresholds.push_back(lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 0.0f);
                int_vals.push_back(lua_isnumber(L, -1) ? static_cast<int>(lua_tointeger(L, -1)) : 0);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    std::vector<const char*> names;
    names.reserve(name_store.size());
    for (const auto& s : name_store) names.push_back(s.c_str());

    dse_anim3d_add_transition(EID(e), from_state, to_state, transition_duration,
                              has_exit_time ? 1 : 0, exit_time,
                              static_cast<int>(names.size()),
                              names.empty() ? nullptr : names.data(),
                              modes.empty() ? nullptr : modes.data(),
                              thresholds.empty() ? nullptr : thresholds.data(),
                              int_vals.empty() ? nullptr : int_vals.data());
    return 0;
}

int L_EcsSetAnimator3DParamFloat(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* param_name = luaL_checkstring(L, 2);
    float value = helper::CheckFloat(L, 3);
    dse_anim3d_set_param_float(EID(e), param_name, value);
    return 0;
}

int L_EcsSetAnimator3DParamTrigger(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* param_name = luaL_checkstring(L, 2);
    dse_anim3d_set_param_trigger(EID(e), param_name);
    return 0;
}

int L_EcsSetAnimator3DLockRootMotion(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_anim3d_set_lock_root_motion(EID(e), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}

// ============================================================
// 动画层系统 (AnimLayerComponent)
// ============================================================

int L_EcsAddAnimLayerComponent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_animlayer_add_component(EID(e));
    return 0;
}

int L_EcsAddAnimLayer(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_optstring(L, 2, "");
    float weight = helper::OptFloat(L, 3, 1.0f);
    int mode = helper::OptInt(L, 4, 0);
    helper::PushInt(L, dse_animlayer_add(EID(e), name, weight, mode));
    return 1;
}

int L_EcsSetAnimLayerClip(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    const char* danim_path = luaL_checkstring(L, 3);
    float speed = helper::OptFloat(L, 4, 1.0f);
    bool loop = helper::OptBool(L, 5, true);
    dse_animlayer_set_clip(EID(e), idx, danim_path, speed, loop ? 1 : 0);
    return 0;
}

int L_EcsSetAnimLayerWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float w = helper::CheckFloat(L, 3);
    dse_animlayer_set_weight(EID(e), idx, w);
    return 0;
}

int L_EcsSetAnimLayerBoneMask(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    std::vector<std::string> store;
    if (lua_istable(L, 3)) {
        int len = static_cast<int>(lua_rawlen(L, 3));
        for (int i = 1; i <= len; ++i) {
            lua_rawgeti(L, 3, i);
            if (lua_isstring(L, -1)) store.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    std::vector<const char*> bones;
    bones.reserve(store.size());
    for (const auto& s : store) bones.push_back(s.c_str());
    dse_animlayer_set_bone_mask(EID(e), idx,
                                bones.empty() ? nullptr : bones.data(),
                                static_cast<int>(bones.size()));
    return 0;
}

int L_EcsSetAnimLayerBlendTree1D(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    std::vector<std::string> path_store;
    std::vector<float> thresholds;
    std::vector<float> speeds;
    if (lua_istable(L, 3)) {
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            if (lua_istable(L, -1)) {
                lua_rawgeti(L, -1, 1);
                path_store.emplace_back(luaL_optstring(L, -1, ""));
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 2);
                thresholds.push_back(static_cast<float>(luaL_optnumber(L, -1, 0.0)));
                lua_pop(L, 1);
                lua_rawgeti(L, -1, 3);
                speeds.push_back(static_cast<float>(luaL_optnumber(L, -1, 1.0)));
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
    }
    std::vector<const char*> paths;
    paths.reserve(path_store.size());
    for (const auto& s : path_store) paths.push_back(s.c_str());
    dse_animlayer_set_blend_tree_1d(EID(e), idx,
                                    paths.empty() ? nullptr : paths.data(),
                                    thresholds.empty() ? nullptr : thresholds.data(),
                                    speeds.empty() ? nullptr : speeds.data(),
                                    static_cast<int>(paths.size()));
    return 0;
}

int L_EcsSetAnimLayerBlendParam(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float val = helper::CheckFloat(L, 3);
    dse_animlayer_set_blend_param(EID(e), idx, val);
    return 0;
}

int L_EcsSetAnimLayerEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_animlayer_set_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

// ============================================================
// IK 系统 (IKChain3DComponent)
// ============================================================

int L_EcsAddIKComponent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_ik_add_component(EID(e));
    return 0;
}

int L_EcsAddIKChain(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_optstring(L, 2, "");
    int type = helper::OptInt(L, 3, 0);
    const char* root_bone = luaL_optstring(L, 4, "");
    const char* tip_bone = luaL_optstring(L, 5, "");
    float weight = helper::OptFloat(L, 6, 1.0f);
    helper::PushInt(L, dse_ik_add_chain(EID(e), name, type, root_bone, tip_bone, weight));
    return 1;
}

int L_EcsSetIKTarget(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    glm::vec3 t = helper::CheckVec3(L, 3);
    dse_ik_set_target(EID(e), idx, t.x, t.y, t.z);
    return 0;
}

int L_EcsSetIKTargetEntity(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    uint32_t target = UINT32_MAX;
    if (!lua_isnil(L, 3)) {
        target = static_cast<uint32_t>(helper::CheckEntity(L, 3));
    }
    dse_ik_set_target_entity(EID(e), idx, target);
    return 0;
}

int L_EcsSetIKWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    float w = helper::CheckFloat(L, 3);
    dse_ik_set_weight(EID(e), idx, w);
    return 0;
}

int L_EcsSetIKPoleVector(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    glm::vec3 p = helper::CheckVec3(L, 3);
    dse_ik_set_pole_vector(EID(e), idx, p.x, p.y, p.z);
    return 0;
}

int L_EcsSetIKIterations(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    int iters = helper::CheckInt(L, 3);
    dse_ik_set_iterations(EID(e), idx, iters);
    return 0;
}

int L_EcsSetIKEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_ik_set_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

// ============================================================
// FootIK 系统 (FootIK3DComponent) — 脚部贴地，依赖物理 Raycast
// ============================================================

// ecs.add_foot_ik_component(entity)
int L_EcsAddFootIKComponent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_foot_ik_add_component(EID(e));
    return 0;
}

// ecs.add_foot_ik_foot(entity, name, foot_bone, hip_bone,
//                      [foot_height], [max_ground_distance], [blend_speed], [weight]) -> index
int L_EcsAddFootIKFoot(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_optstring(L, 2, "");
    const char* foot_bone = luaL_optstring(L, 3, "");
    const char* hip_bone = luaL_optstring(L, 4, "");
    float foot_height = (lua_gettop(L) >= 5) ? helper::CheckFloat(L, 5) : NAN;
    float max_ground_distance = (lua_gettop(L) >= 6) ? helper::CheckFloat(L, 6) : NAN;
    float blend_speed = (lua_gettop(L) >= 7) ? helper::CheckFloat(L, 7) : NAN;
    float weight = (lua_gettop(L) >= 8) ? helper::CheckFloat(L, 8) : NAN;
    helper::PushInt(L, dse_foot_ik_add_foot(EID(e), name, foot_bone, hip_bone,
                                            foot_height, max_ground_distance, blend_speed, weight));
    return 1;
}

// ecs.set_foot_ik_foot_weight(entity, idx, weight)
int L_EcsSetFootIKFootWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    dse_foot_ik_set_foot_weight(EID(e), idx, helper::CheckFloat(L, 3));
    return 0;
}

// ecs.set_foot_ik_foot_height(entity, idx, height)
int L_EcsSetFootIKFootHeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = helper::CheckInt(L, 2);
    dse_foot_ik_set_foot_height(EID(e), idx, helper::CheckFloat(L, 3));
    return 0;
}

// ecs.set_foot_ik_pelvis(entity, [pelvis_weight], [max_pelvis_offset])
int L_EcsSetFootIKPelvis(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float pelvis_weight = (lua_gettop(L) >= 2) ? helper::CheckFloat(L, 2) : NAN;
    float max_pelvis_offset = (lua_gettop(L) >= 3) ? helper::CheckFloat(L, 3) : NAN;
    dse_foot_ik_set_pelvis(EID(e), pelvis_weight, max_pelvis_offset);
    return 0;
}

// ecs.set_foot_ik_enabled(entity, enabled)
int L_EcsSetFootIKEnabled(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_foot_ik_set_enabled(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

// ============================================================
// 3D 动画事件 (Animator3DComponent)
// ============================================================

int L_EcsAddAnimator3DEvent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* event_name = luaL_checkstring(L, 2);
    float trigger_time = helper::CheckFloat(L, 3);
    dse_anim3d_add_event(EID(e), event_name, trigger_time);
    return 0;
}

int L_EcsPopAnimator3DEvent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    char buf[256];
    dse_anim3d_pop_event(EID(e), buf, static_cast<int>(sizeof(buf)));
    lua_pushstring(L, buf);
    return 1;
}

int L_EcsSetAnimator3DExtractRootMotion(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_anim3d_set_extract_root_motion(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

int L_EcsGetAnimator3DRootMotionDelta(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float xyz[3];
    if (!dse_anim3d_get_root_motion_delta(EID(e), xyz)) return 0;
    helper::PushVec3(L, glm::vec3(xyz[0], xyz[1], xyz[2]));
    return 3;
}

// ============================================================
// 骨骼挂点系统 (BoneAttachmentComponent)
// ============================================================

// ecs.add_bone_attachment(entity, target_entity, bone_name)
int L_EcsAddBoneAttachment(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    Entity target = helper::CheckEntity(L, 2);
    const char* bone = luaL_checkstring(L, 3);
    dse_bone_attach_add(EID(e), EID(target), bone);
    return 0;
}

// ecs.set_bone_attachment_offset(entity, px,py,pz, rx,ry,rz,rw, [sx,sy,sz])
int L_EcsSetBoneAttachmentOffset(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float px = helper::CheckFloat(L, 2);
    float py = helper::CheckFloat(L, 3);
    float pz = helper::CheckFloat(L, 4);
    float qx = helper::CheckFloat(L, 5);
    float qy = helper::CheckFloat(L, 6);
    float qz = helper::CheckFloat(L, 7);
    float qw = helper::CheckFloat(L, 8);
    float sx = helper::OptFloat(L, 9, 1.0f);
    float sy = helper::OptFloat(L, 10, 1.0f);
    float sz = helper::OptFloat(L, 11, 1.0f);
    dse_bone_attach_set_offset(EID(e), px, py, pz, qx, qy, qz, qw, sx, sy, sz);
    return 0;
}

// ecs.set_bone_attachment_bone(entity, bone_name)
int L_EcsSetBoneAttachmentBone(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_bone_attach_set_bone(EID(e), luaL_checkstring(L, 2));
    return 0;
}

// ecs.set_bone_attachment_target(entity, target_entity)
int L_EcsSetBoneAttachmentTarget(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    Entity target = helper::CheckEntity(L, 2);
    dse_bone_attach_set_target(EID(e), EID(target));
    return 0;
}

// ecs.get_bone_world_position(target_entity, bone_name) -> x, y, z
int L_EcsGetBoneWorldPosition(lua_State* L) {
    Entity target = helper::CheckEntity(L, 1);
    const char* bone_name = luaL_checkstring(L, 2);
    float xyz[3];
    dse_bone_attach_get_world_pos(EID(target), bone_name, xyz);
    helper::PushVec3(L, glm::vec3(xyz[0], xyz[1], xyz[2]));
    return 3;
}

// ecs.remove_bone_attachment(entity)
int L_EcsRemoveBoneAttachment(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_bone_attach_remove(EID(e));
    return 0;
}

// ============================================================
// Morph Target (Blend Shape)
// ============================================================

// ecs.add_morph_target_component(entity)
int L_EcsAddMorphTargetComponent(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_morph_add_component(EID(e));
    return 0;
}

// ecs.morph_add_target(entity, name, {deltas})
// deltas = flat array: {dx,dy,dz, dnx,dny,dnz, ...}
int L_EcsMorphAddTarget(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    int len = static_cast<int>(lua_rawlen(L, 3));
    std::vector<float> deltas;
    deltas.reserve(static_cast<size_t>(len));
    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(L, 3, i);
        deltas.push_back(static_cast<float>(lua_tonumber(L, -1)));
        lua_pop(L, 1);
    }
    dse_morph_add_target(EID(e), name,
                         deltas.empty() ? nullptr : deltas.data(),
                         static_cast<int>(deltas.size()));
    return 0;
}

// ecs.morph_set_weight(entity, name, weight)
int L_EcsMorphSetWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float w = helper::CheckFloat(L, 3);
    dse_morph_set_weight(EID(e), name, w);
    return 0;
}

// ecs.morph_set_weight_index(entity, index, weight)  (0-based)
int L_EcsMorphSetWeightIndex(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = helper::CheckFloat(L, 3);
    dse_morph_set_weight_index(EID(e), idx, w);
    return 0;
}

// ecs.morph_get_weight(entity, name) -> number
int L_EcsMorphGetWeight(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    const char* name = luaL_checkstring(L, 2);
    lua_pushnumber(L, dse_morph_get_weight(EID(e), name));
    return 1;
}

// ecs.morph_get_target_count(entity) -> int
int L_EcsMorphGetTargetCount(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    lua_pushinteger(L, dse_morph_get_target_count(EID(e)));
    return 1;
}

} // namespace

void RegisterEcsAnimationBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        // 2D 动画
        {"add_animator",              L_EcsAddAnimator},
        {"add_animation_state",       L_EcsAddAnimationState},
        {"add_animation_event",       L_EcsAddAnimationEvent},
        {"play_animation",            L_EcsPlayAnimation},
        {"play_animation_segment",    L_EcsPlayAnimationSegment},
        {"pop_animation_event",       L_EcsPopAnimationEvent},
        // 3D 骨骼动画
        {"add_animator_3d",           L_EcsAddAnimator3D},
        {"set_animator_3d_state",     L_EcsSetAnimator3DState},
        {"get_animator_3d_state",     L_EcsGetAnimator3DState},
        {"init_animator_3d_fsm",      L_EcsInitAnimator3DStateMachine},
        {"add_animator_3d_state",     L_EcsAddAnimator3DState},
        {"add_animator_3d_transition", L_EcsAddAnimator3DTransition},
        {"set_animator_3d_param_float",  L_EcsSetAnimator3DParamFloat},
        {"set_animator_3d_param_trigger", L_EcsSetAnimator3DParamTrigger},
        {"set_animator_3d_lock_root_motion", L_EcsSetAnimator3DLockRootMotion},
        // 3D 动画事件 + Root Motion
        {"add_animator_3d_event",            L_EcsAddAnimator3DEvent},
        {"pop_animator_3d_event",            L_EcsPopAnimator3DEvent},
        {"set_animator_3d_extract_root_motion", L_EcsSetAnimator3DExtractRootMotion},
        {"get_animator_3d_root_motion_delta", L_EcsGetAnimator3DRootMotionDelta},
        // 动画层系统
        {"add_anim_layer_component",         L_EcsAddAnimLayerComponent},
        {"add_anim_layer",                   L_EcsAddAnimLayer},
        {"set_anim_layer_clip",              L_EcsSetAnimLayerClip},
        {"set_anim_layer_weight",            L_EcsSetAnimLayerWeight},
        {"set_anim_layer_bone_mask",         L_EcsSetAnimLayerBoneMask},
        {"set_anim_layer_blend_tree_1d",     L_EcsSetAnimLayerBlendTree1D},
        {"set_anim_layer_blend_param",       L_EcsSetAnimLayerBlendParam},
        {"set_anim_layer_enabled",           L_EcsSetAnimLayerEnabled},
        // IK 系统
        {"add_ik_component",                 L_EcsAddIKComponent},
        {"add_ik_chain",                     L_EcsAddIKChain},
        {"set_ik_target",                    L_EcsSetIKTarget},
        {"set_ik_target_entity",             L_EcsSetIKTargetEntity},
        {"set_ik_weight",                    L_EcsSetIKWeight},
        {"set_ik_pole_vector",               L_EcsSetIKPoleVector},
        {"set_ik_iterations",                L_EcsSetIKIterations},
        {"set_ik_enabled",                   L_EcsSetIKEnabled},
        // FootIK 系统
        {"add_foot_ik_component",            L_EcsAddFootIKComponent},
        {"add_foot_ik_foot",                 L_EcsAddFootIKFoot},
        {"set_foot_ik_foot_weight",          L_EcsSetFootIKFootWeight},
        {"set_foot_ik_foot_height",          L_EcsSetFootIKFootHeight},
        {"set_foot_ik_pelvis",               L_EcsSetFootIKPelvis},
        {"set_foot_ik_enabled",              L_EcsSetFootIKEnabled},
        // 骨骼挂点系统
        {"add_bone_attachment",              L_EcsAddBoneAttachment},
        {"set_bone_attachment_offset",       L_EcsSetBoneAttachmentOffset},
        {"set_bone_attachment_bone",         L_EcsSetBoneAttachmentBone},
        {"set_bone_attachment_target",       L_EcsSetBoneAttachmentTarget},
        {"get_bone_world_position",          L_EcsGetBoneWorldPosition},
        {"remove_bone_attachment",           L_EcsRemoveBoneAttachment},
        // Morph Target (Blend Shape)
        {"add_morph_target_component",       L_EcsAddMorphTargetComponent},
        {"morph_add_target",                 L_EcsMorphAddTarget},
        {"morph_set_weight",                 L_EcsMorphSetWeight},
        {"morph_set_weight_index",           L_EcsMorphSetWeightIndex},
        {"morph_get_weight",                 L_EcsMorphGetWeight},
        {"morph_get_target_count",           L_EcsMorphGetTargetCount},
    });
}

} // namespace dse::runtime::lua_binding

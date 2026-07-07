/**
 * @file lua_binding_free_ecs_animation.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_anim2d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_anim2d_add(e);
    return 0;
}

int L_dse_anim2d_add_event(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    float normalized_time = static_cast<float>(luaL_checknumber(L, 3));
    const char* event_name = luaL_checkstring(L, 4);
    dse_anim2d_add_event(e, state_name, normalized_time, event_name);
    return 0;
}

int L_dse_anim2d_play(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    dse_anim2d_play(e, state_name);
    return 0;
}

int L_dse_anim2d_play_segment(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int start_frame = static_cast<int>(luaL_checkinteger(L, 2));
    int end_frame = static_cast<int>(luaL_checkinteger(L, 3));
    int loop = static_cast<int>(luaL_checkinteger(L, 4));
    dse_anim2d_play_segment(e, start_frame, end_frame, loop);
    return 0;
}

int L_dse_anim3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* danim_path = luaL_checkstring(L, 2);
    const char* dskel_path = luaL_checkstring(L, 3);
    dse_anim3d_add(e, danim_path, dskel_path);
    return 0;
}

int L_dse_anim3d_set_state(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    float speed = static_cast<float>(luaL_checknumber(L, 3));
    int loop = static_cast<int>(luaL_checkinteger(L, 4));
    dse_anim3d_set_state(e, state_name, speed, loop);
    return 0;
}

int L_dse_anim3d_init_fsm(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_anim3d_init_fsm(e);
    return 0;
}

int L_dse_anim3d_add_fsm_state(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* state_name = luaL_checkstring(L, 2);
    const char* danim_path = luaL_checkstring(L, 3);
    int loop = static_cast<int>(luaL_checkinteger(L, 4));
    float speed = static_cast<float>(luaL_checknumber(L, 5));
    dse_anim3d_add_fsm_state(e, state_name, danim_path, loop, speed);
    return 0;
}

int L_dse_anim3d_set_param_float(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    dse_anim3d_set_param_float(e, param_name, value);
    return 0;
}

int L_dse_anim3d_set_param_trigger(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    dse_anim3d_set_param_trigger(e, param_name);
    return 0;
}

int L_dse_anim3d_set_lock_root_motion(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int lock = static_cast<int>(luaL_checkinteger(L, 2));
    dse_anim3d_set_lock_root_motion(e, lock);
    return 0;
}

int L_dse_animlayer_add_component(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animlayer_add_component(e);
    return 0;
}

int L_dse_animlayer_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float weight = static_cast<float>(luaL_checknumber(L, 3));
    int blend_mode = static_cast<int>(luaL_checkinteger(L, 4));
    int _ret = dse_animlayer_add(e, name, weight, blend_mode);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_animlayer_set_clip(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    const char* danim_path = luaL_checkstring(L, 3);
    float speed = static_cast<float>(luaL_checknumber(L, 4));
    int loop = static_cast<int>(luaL_checkinteger(L, 5));
    dse_animlayer_set_clip(e, idx, danim_path, speed, loop);
    return 0;
}

int L_dse_animlayer_set_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_animlayer_set_weight(e, idx, w);
    return 0;
}

int L_dse_animlayer_set_blend_param(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float val = static_cast<float>(luaL_checknumber(L, 3));
    dse_animlayer_set_blend_param(e, idx, val);
    return 0;
}

int L_dse_animlayer_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_animlayer_set_enabled(e, enabled);
    return 0;
}

int L_dse_ik_add_component(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ik_add_component(e);
    return 0;
}

int L_dse_ik_add_chain(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    int type = static_cast<int>(luaL_checkinteger(L, 3));
    const char* root_bone = luaL_checkstring(L, 4);
    const char* tip_bone = luaL_checkstring(L, 5);
    float weight = static_cast<float>(luaL_checknumber(L, 6));
    int _ret = dse_ik_add_chain(e, name, type, root_bone, tip_bone, weight);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ik_set_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    dse_ik_set_target(e, idx, x, y, z);
    return 0;
}

int L_dse_ik_set_target_entity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_ik_set_target_entity(e, idx, target);
    return 0;
}

int L_dse_ik_set_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_ik_set_weight(e, idx, w);
    return 0;
}

int L_dse_ik_set_pole_vector(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    dse_ik_set_pole_vector(e, idx, x, y, z);
    return 0;
}

int L_dse_ik_set_iterations(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    int iters = static_cast<int>(luaL_checkinteger(L, 3));
    dse_ik_set_iterations(e, idx, iters);
    return 0;
}

int L_dse_ik_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_ik_set_enabled(e, enabled);
    return 0;
}

int L_dse_foot_ik_add_component(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_foot_ik_add_component(e);
    return 0;
}

int L_dse_foot_ik_add_foot(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    const char* foot_bone = luaL_checkstring(L, 3);
    const char* hip_bone = luaL_checkstring(L, 4);
    float foot_height = static_cast<float>(luaL_checknumber(L, 5));
    float max_ground_distance = static_cast<float>(luaL_checknumber(L, 6));
    float blend_speed = static_cast<float>(luaL_checknumber(L, 7));
    float weight = static_cast<float>(luaL_checknumber(L, 8));
    int _ret = dse_foot_ik_add_foot(e, name, foot_bone, hip_bone, foot_height, max_ground_distance, blend_speed, weight);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_foot_ik_set_foot_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_foot_ik_set_foot_weight(e, idx, w);
    return 0;
}

int L_dse_foot_ik_set_foot_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    dse_foot_ik_set_foot_height(e, idx, h);
    return 0;
}

int L_dse_foot_ik_set_pelvis(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float pelvis_weight = static_cast<float>(luaL_checknumber(L, 2));
    float max_pelvis_offset = static_cast<float>(luaL_checknumber(L, 3));
    dse_foot_ik_set_pelvis(e, pelvis_weight, max_pelvis_offset);
    return 0;
}

int L_dse_foot_ik_set_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_foot_ik_set_enabled(e, enabled);
    return 0;
}

int L_dse_anim3d_add_event(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* event_name = luaL_checkstring(L, 2);
    float trigger_time = static_cast<float>(luaL_checknumber(L, 3));
    dse_anim3d_add_event(e, event_name, trigger_time);
    return 0;
}

int L_dse_anim3d_set_extract_root_motion(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    dse_anim3d_set_extract_root_motion(e, enabled);
    return 0;
}

int L_dse_bone_attach_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    const char* bone_name = luaL_checkstring(L, 3);
    dse_bone_attach_add(e, target, bone_name);
    return 0;
}

int L_dse_bone_attach_set_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float px = static_cast<float>(luaL_checknumber(L, 2));
    float py = static_cast<float>(luaL_checknumber(L, 3));
    float pz = static_cast<float>(luaL_checknumber(L, 4));
    float qx = static_cast<float>(luaL_checknumber(L, 5));
    float qy = static_cast<float>(luaL_checknumber(L, 6));
    float qz = static_cast<float>(luaL_checknumber(L, 7));
    float qw = static_cast<float>(luaL_checknumber(L, 8));
    float sx = static_cast<float>(luaL_checknumber(L, 9));
    float sy = static_cast<float>(luaL_checknumber(L, 10));
    float sz = static_cast<float>(luaL_checknumber(L, 11));
    dse_bone_attach_set_offset(e, px, py, pz, qx, qy, qz, qw, sx, sy, sz);
    return 0;
}

int L_dse_bone_attach_set_bone(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* bone_name = luaL_checkstring(L, 2);
    dse_bone_attach_set_bone(e, bone_name);
    return 0;
}

int L_dse_bone_attach_set_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_bone_attach_set_target(e, target);
    return 0;
}

int L_dse_bone_attach_remove(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_bone_attach_remove(e);
    return 0;
}

int L_dse_morph_add_component(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_morph_add_component(e);
    return 0;
}

int L_dse_morph_set_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_set_weight(e, name, w);
    return 0;
}

int L_dse_morph_set_weight_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_set_weight_index(e, idx, w);
    return 0;
}

int L_dse_morph_get_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float _ret = dse_morph_get_weight(e, name);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_morph_get_target_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_morph_get_target_count(e);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterEcsAnimationBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_animator", L_dse_anim2d_add},
        {"add_animation_event", L_dse_anim2d_add_event},
        {"play_animation", L_dse_anim2d_play},
        {"play_animation_segment", L_dse_anim2d_play_segment},
        {"add_animator_3d", L_dse_anim3d_add},
        {"set_animator_3d_state", L_dse_anim3d_set_state},
        {"init_animator_3d_fsm", L_dse_anim3d_init_fsm},
        {"add_animator_3d_state", L_dse_anim3d_add_fsm_state},
        {"set_animator_3d_param_float", L_dse_anim3d_set_param_float},
        {"set_animator_3d_param_trigger", L_dse_anim3d_set_param_trigger},
        {"set_animator_3d_lock_root_motion", L_dse_anim3d_set_lock_root_motion},
        {"add_anim_layer_component", L_dse_animlayer_add_component},
        {"add_anim_layer", L_dse_animlayer_add},
        {"set_anim_layer_clip", L_dse_animlayer_set_clip},
        {"set_anim_layer_weight", L_dse_animlayer_set_weight},
        {"set_anim_layer_blend_param", L_dse_animlayer_set_blend_param},
        {"set_anim_layer_enabled", L_dse_animlayer_set_enabled},
        {"add_ik_component", L_dse_ik_add_component},
        {"add_ik_chain", L_dse_ik_add_chain},
        {"set_ik_target", L_dse_ik_set_target},
        {"set_ik_target_entity", L_dse_ik_set_target_entity},
        {"set_ik_weight", L_dse_ik_set_weight},
        {"set_ik_pole_vector", L_dse_ik_set_pole_vector},
        {"set_ik_iterations", L_dse_ik_set_iterations},
        {"set_ik_enabled", L_dse_ik_set_enabled},
        {"add_foot_ik_component", L_dse_foot_ik_add_component},
        {"add_foot_ik_foot", L_dse_foot_ik_add_foot},
        {"set_foot_ik_foot_weight", L_dse_foot_ik_set_foot_weight},
        {"set_foot_ik_foot_height", L_dse_foot_ik_set_foot_height},
        {"set_foot_ik_pelvis", L_dse_foot_ik_set_pelvis},
        {"set_foot_ik_enabled", L_dse_foot_ik_set_enabled},
        {"add_animator_3d_event", L_dse_anim3d_add_event},
        {"set_animator_3d_extract_root_motion", L_dse_anim3d_set_extract_root_motion},
        {"add_bone_attachment", L_dse_bone_attach_add},
        {"set_bone_attachment_offset", L_dse_bone_attach_set_offset},
        {"set_bone_attachment_bone", L_dse_bone_attach_set_bone},
        {"set_bone_attachment_target", L_dse_bone_attach_set_target},
        {"remove_bone_attachment", L_dse_bone_attach_remove},
        {"add_morph_target_component", L_dse_morph_add_component},
        {"morph_set_weight", L_dse_morph_set_weight},
        {"morph_set_weight_index", L_dse_morph_set_weight_index},
        {"morph_get_weight", L_dse_morph_get_weight},
        {"morph_get_target_count", L_dse_morph_get_target_count},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding

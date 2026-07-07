/**
 * @file lua_binding_cabi_gap.cpp
 * @brief 手写 Lua 绑定 — 仅保留无法模板化的复杂函数。
 *
 * 大部分函数已迁移至 codegen（function_defs.json → lua_binding_free_*.gen.cpp）。
 * 此文件仅保留需要 Lua table ↔ C array 转换的函数。
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
// Animation 3D — set_layer_mask (string array param)
// ============================================================
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

// ============================================================
// Animation 3D — set_blend_tree_1d (multiple array params)
// ============================================================
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

void RegisterCabiGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"anim3d_set_layer_mask",    L_EcsAnim3DSetLayerMask},
        {"anim3d_set_blend_tree_1d", L_EcsAnim3DSetBlendTree1D},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding

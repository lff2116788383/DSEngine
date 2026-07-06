/**
 * @file lua_binding_ai.cpp
 * @brief AI 行为树 + GOAP 规划器 Lua 绑定。薄包装委托至 C ABI。
 *
 * 全局表 `ai`:
 *   ai.create_tree(name)                → tree_id
 *   ai.destroy_tree(tree_id)
 *   ai.tick_tree(tree_id, dt)           → "success"|"failure"|"running"
 *   ai.reset_tree(tree_id)
 *
 *   -- 黑板
 *   ai.bb_set_bool(tree_id, key, value)
 *   ai.bb_set_int(tree_id, key, value)
 *   ai.bb_set_float(tree_id, key, value)
 *   ai.bb_set_string(tree_id, key, value)
 *   ai.bb_set_vec3(tree_id, key, x, y, z)
 *   ai.bb_get_bool(tree_id, key)        → bool
 *   ai.bb_get_int(tree_id, key)         → int
 *   ai.bb_get_float(tree_id, key)       → float
 *   ai.bb_get_string(tree_id, key)      → string
 *   ai.bb_get_vec3(tree_id, key)        → x,y,z
 *
 *   -- 树构建（栈式 API）
 *   ai.begin_sequence(tree_id, name)
 *   ai.begin_selector(tree_id, name)
 *   ai.begin_parallel(tree_id, policy, name)   policy: "all"|"one"
 *   ai.end_composite(tree_id)
 *   ai.add_condition(tree_id, name, lua_func)   lua_func(bb_table) → bool
 *   ai.add_action(tree_id, name, lua_func)      lua_func(dt, bb_table) → "success"|"failure"|"running"
 *   ai.add_inverter(tree_id, name)               — 下一个节点被包装
 *   ai.add_succeeder(tree_id, name)
 *   ai.add_repeater(tree_id, name, max_repeats)
 *
 *   -- GOAP
 *   ai.goap_create()                    → planner_id
 *   ai.goap_destroy(planner_id)
 *   ai.goap_add_action(planner_id, action_table)
 *   ai.goap_plan(planner_id, current_state, goal) → {action_names} | nil
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

using namespace helper;

// ============================================================
// Lua 回调 trampoline（user_data 持有 lua_State + registry ref）
// ============================================================

struct LuaCallbackCtx {
    lua_State* L = nullptr;
    int func_ref = LUA_NOREF;
};

extern "C" int LuaConditionTrampoline(void* user_data) {
    auto* ctx = static_cast<LuaCallbackCtx*>(user_data);
    if (!ctx || !ctx->L || ctx->func_ref == LUA_NOREF) return 0;
    lua_State* L = ctx->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->func_ref);
    // 传入黑板简化表（仅传空表，由用户通过 bb_get 访问）
    lua_newtable(L);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return 0;
    }
    const int result = lua_toboolean(L, -1) != 0 ? 1 : 0;
    lua_pop(L, 1);
    return result;
}

extern "C" int LuaActionTrampoline(float dt, void* user_data) {
    auto* ctx = static_cast<LuaCallbackCtx*>(user_data);
    if (!ctx || !ctx->L || ctx->func_ref == LUA_NOREF) return 0;
    lua_State* L = ctx->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->func_ref);
    lua_pushnumber(L, dt);
    lua_newtable(L); // bb table placeholder
    if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
        lua_pop(L, 1);
        return 0;
    }
    const char* result_str = lua_tostring(L, -1);
    int status = 0;
    if (result_str) {
        if (result_str[0] == 's') status = 1;
        else if (result_str[0] == 'r') status = 2;
    }
    lua_pop(L, 1);
    return status;
}

extern "C" void LuaCallbackDestroy(void* user_data) {
    auto* ctx = static_cast<LuaCallbackCtx*>(user_data);
    if (!ctx) return;
    if (ctx->L && ctx->func_ref != LUA_NOREF) {
        luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->func_ref);
    }
    delete ctx;
}

// ============================================================
// Lua C Functions — Behavior Tree
// ============================================================

int L_AICreateTree(lua_State* L) {
    const char* name = luaL_optstring(L, 1, "unnamed");
    lua_pushinteger(L, dse_ai_tree_create(name));
    return 1;
}

int L_AIDestroyTree(lua_State* L) {
    dse_ai_tree_destroy(CheckInt(L, 1));
    return 0;
}

int L_AITickTree(lua_State* L) {
    int id = CheckInt(L, 1);
    float dt = CheckFloat(L, 2);
    switch (dse_ai_tree_tick(id, dt)) {
        case 1: lua_pushstring(L, "success"); break;
        case 2: lua_pushstring(L, "running"); break;
        default: lua_pushstring(L, "failure"); break;
    }
    return 1;
}

int L_AIResetTree(lua_State* L) {
    dse_ai_tree_reset(CheckInt(L, 1));
    return 0;
}

// --- Blackboard ---

int L_AIBBSetBool(lua_State* L) {
    dse_ai_bb_set_bool(CheckInt(L, 1), luaL_checkstring(L, 2), lua_toboolean(L, 3) != 0 ? 1 : 0);
    return 0;
}

int L_AIBBSetInt(lua_State* L) {
    dse_ai_bb_set_int(CheckInt(L, 1), luaL_checkstring(L, 2), CheckInt(L, 3));
    return 0;
}

int L_AIBBSetFloat(lua_State* L) {
    dse_ai_bb_set_float(CheckInt(L, 1), luaL_checkstring(L, 2), CheckFloat(L, 3));
    return 0;
}

int L_AIBBSetString(lua_State* L) {
    dse_ai_bb_set_string(CheckInt(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    return 0;
}

int L_AIBBSetVec3(lua_State* L) {
    dse_ai_bb_set_vec3(CheckInt(L, 1), luaL_checkstring(L, 2),
                       CheckFloat(L, 3), CheckFloat(L, 4), CheckFloat(L, 5));
    return 0;
}

int L_AIBBGetBool(lua_State* L) {
    lua_pushboolean(L, dse_ai_bb_get_bool(CheckInt(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

int L_AIBBGetInt(lua_State* L) {
    lua_pushinteger(L, dse_ai_bb_get_int(CheckInt(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

int L_AIBBGetFloat(lua_State* L) {
    lua_pushnumber(L, dse_ai_bb_get_float(CheckInt(L, 1), luaL_checkstring(L, 2)));
    return 1;
}

int L_AIBBGetString(lua_State* L) {
    char buf[1024] = {0};
    const int len = dse_ai_bb_get_string(CheckInt(L, 1), luaL_checkstring(L, 2),
                                         buf, static_cast<int>(sizeof(buf)));
    lua_pushlstring(L, buf, static_cast<size_t>(len));
    return 1;
}

int L_AIBBGetVec3(lua_State* L) {
    float xyz[3] = {0.0f, 0.0f, 0.0f};
    dse_ai_bb_get_vec3(CheckInt(L, 1), luaL_checkstring(L, 2), xyz);
    lua_pushnumber(L, xyz[0]);
    lua_pushnumber(L, xyz[1]);
    lua_pushnumber(L, xyz[2]);
    return 3;
}

// --- Tree Construction ---

int L_AIBeginSequence(lua_State* L) {
    dse_ai_begin_sequence(CheckInt(L, 1), luaL_optstring(L, 2, "Sequence"));
    return 0;
}

int L_AIBeginSelector(lua_State* L) {
    dse_ai_begin_selector(CheckInt(L, 1), luaL_optstring(L, 2, "Selector"));
    return 0;
}

int L_AIBeginParallel(lua_State* L) {
    int id = CheckInt(L, 1);
    const char* policy_str = luaL_optstring(L, 2, "all");
    const char* name = luaL_optstring(L, 3, "Parallel");
    dse_ai_begin_parallel(id, policy_str[0] == 'o' ? 1 : 0, name);
    return 0;
}

int L_AIEndComposite(lua_State* L) {
    dse_ai_end_composite(CheckInt(L, 1));
    return 0;
}

int L_AIAddCondition(lua_State* L) {
    int id = CheckInt(L, 1);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    auto* ctx = new LuaCallbackCtx{L, luaL_ref(L, LUA_REGISTRYINDEX)};
    dse_ai_add_condition(id, name, LuaConditionTrampoline, ctx, LuaCallbackDestroy);
    return 0;
}

int L_AIAddAction(lua_State* L) {
    int id = CheckInt(L, 1);
    const char* name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    auto* ctx = new LuaCallbackCtx{L, luaL_ref(L, LUA_REGISTRYINDEX)};
    dse_ai_add_action(id, name, LuaActionTrampoline, ctx, LuaCallbackDestroy);
    return 0;
}

int L_AIAddInverter(lua_State* L) {
    dse_ai_add_inverter(CheckInt(L, 1), luaL_optstring(L, 2, "Inverter"));
    return 0;
}

int L_AIAddSucceeder(lua_State* L) {
    dse_ai_add_succeeder(CheckInt(L, 1), luaL_optstring(L, 2, "Succeeder"));
    return 0;
}

int L_AIAddRepeater(lua_State* L) {
    dse_ai_add_repeater(CheckInt(L, 1), luaL_optstring(L, 2, "Repeater"), OptInt(L, 3, -1));
    return 0;
}

// ============================================================
// Lua C Functions — GOAP
// ============================================================

int L_GOAPCreate(lua_State* L) {
    lua_pushinteger(L, dse_ai_goap_create());
    return 1;
}

int L_GOAPDestroy(lua_State* L) {
    dse_ai_goap_destroy(CheckInt(L, 1));
    return 0;
}

// ai.goap_add_action(planner_id, { name="...", cost=1, preconditions={k=bool}, effects={k=bool} })
int L_GOAPAddAction(lua_State* L) {
    int id = CheckInt(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    const char* name = "";
    float cost = 1.0f;
    lua_getfield(L, 2, "name");
    if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
    lua_getfield(L, 2, "cost");
    if (lua_isnumber(L, -1)) cost = static_cast<float>(lua_tonumber(L, -1));
    dse_ai_goap_action_begin(id, name, cost);
    lua_pop(L, 2);

    lua_getfield(L, 2, "preconditions");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_isstring(L, -2)) {
                dse_ai_goap_action_precondition(id, lua_tostring(L, -2),
                                                lua_toboolean(L, -1) != 0 ? 1 : 0);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, 2, "effects");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (lua_isstring(L, -2)) {
                dse_ai_goap_action_effect(id, lua_tostring(L, -2),
                                          lua_toboolean(L, -1) != 0 ? 1 : 0);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    dse_ai_goap_action_commit(id);
    return 0;
}

// ai.goap_plan(planner_id, current_state_table, goal_table) → {action_names} | nil
int L_GOAPPlan(lua_State* L) {
    int id = CheckInt(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);

    dse_ai_goap_state_clear(id);

    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        if (lua_isstring(L, -2)) {
            dse_ai_goap_state_set(id, 0, lua_tostring(L, -2),
                                  lua_toboolean(L, -1) != 0 ? 1 : 0);
        }
        lua_pop(L, 1);
    }

    lua_pushnil(L);
    while (lua_next(L, 3) != 0) {
        if (lua_isstring(L, -2)) {
            dse_ai_goap_state_set(id, 1, lua_tostring(L, -2),
                                  lua_toboolean(L, -1) != 0 ? 1 : 0);
        }
        lua_pop(L, 1);
    }

    char buf[4096] = {0};
    const int len = dse_ai_goap_plan(id, buf, static_cast<int>(sizeof(buf)));
    if (len < 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    int index = 1;
    int start = 0;
    for (int i = 0; i <= len; ++i) {
        if (i == len || buf[i] == '\n') {
            if (i > start) {
                lua_pushlstring(L, buf + start, static_cast<size_t>(i - start));
                lua_rawseti(L, -2, index++);
            }
            start = i + 1;
        }
    }
    return 1;
}

} // namespace

void ShutdownAIBindings() {
    dse_ai_shutdown();
}

void RegisterAIBindings(lua_State* L) {
    static bool registered = false;
    if (!registered) {
        BindingCleanupRegistry::Instance().Register(ShutdownAIBindings);
        registered = true;
    }
    lua_newtable(L);

    // Tree management
    RegisterFn(L, "create_tree", L_AICreateTree);
    RegisterFn(L, "destroy_tree", L_AIDestroyTree);
    RegisterFn(L, "tick_tree", L_AITickTree);
    RegisterFn(L, "reset_tree", L_AIResetTree);

    // Blackboard
    RegisterFn(L, "bb_set_bool", L_AIBBSetBool);
    RegisterFn(L, "bb_set_int", L_AIBBSetInt);
    RegisterFn(L, "bb_set_float", L_AIBBSetFloat);
    RegisterFn(L, "bb_set_string", L_AIBBSetString);
    RegisterFn(L, "bb_set_vec3", L_AIBBSetVec3);
    RegisterFn(L, "bb_get_bool", L_AIBBGetBool);
    RegisterFn(L, "bb_get_int", L_AIBBGetInt);
    RegisterFn(L, "bb_get_float", L_AIBBGetFloat);
    RegisterFn(L, "bb_get_string", L_AIBBGetString);
    RegisterFn(L, "bb_get_vec3", L_AIBBGetVec3);

    // Tree construction
    RegisterFn(L, "begin_sequence", L_AIBeginSequence);
    RegisterFn(L, "begin_selector", L_AIBeginSelector);
    RegisterFn(L, "begin_parallel", L_AIBeginParallel);
    RegisterFn(L, "end_composite", L_AIEndComposite);
    RegisterFn(L, "add_condition", L_AIAddCondition);
    RegisterFn(L, "add_action", L_AIAddAction);
    RegisterFn(L, "add_inverter", L_AIAddInverter);
    RegisterFn(L, "add_succeeder", L_AIAddSucceeder);
    RegisterFn(L, "add_repeater", L_AIAddRepeater);

    // GOAP
    RegisterFn(L, "goap_create", L_GOAPCreate);
    RegisterFn(L, "goap_destroy", L_GOAPDestroy);
    RegisterFn(L, "goap_add_action", L_GOAPAddAction);
    RegisterFn(L, "goap_plan", L_GOAPPlan);

    lua_setglobal(L, "ai");
}

} // namespace dse::runtime::lua_binding

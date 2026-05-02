/**
 * @file lua_binding_helper.h
 * @brief Lua 绑定辅助工具集 — 减少手写 C API 绑定的样板代码
 *
 * 提供三类辅助：
 * 1. 参数提取与返回值推入（类型安全包装，替代重复的 luaL_check* + static_cast）
 * 2. 组件字段访问宏（一行声明 getter/setter，替代手写 L_EcsXxx 函数）
 * 3. 注册辅助（简化 pushcfunction + setfield 模式）
 *
 * @note 所有辅助均为零开销内联/宏，不影响运行时性能。
 *       Lua 侧 API 名称完全不变，仅减少 C++ 侧的重复代码。
 */

#ifndef DSE_LUA_BINDING_HELPER_H
#define DSE_LUA_BINDING_HELPER_H

#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/ecs/world.h"
extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding::helper {

// ============================================================
// 参数提取 — 类型安全包装，替代 luaL_checkinteger + static_cast
// ============================================================

/// 从 Lua 栈位置 i 提取 float
inline float CheckFloat(lua_State* L, int i) {
    return static_cast<float>(luaL_checknumber(L, i));
}

/// 从 Lua 栈位置 i 提取可选 float，缺省使用 default_val
inline float OptFloat(lua_State* L, int i, float default_val) {
    return static_cast<float>(luaL_optnumber(L, i, static_cast<lua_Number>(default_val)));
}

/// 从 Lua 栈位置 i 提取 int
inline int CheckInt(lua_State* L, int i) {
    return static_cast<int>(luaL_checkinteger(L, i));
}

/// 从 Lua 栈位置 i 提取可选 int，缺省使用 default_val
inline int OptInt(lua_State* L, int i, int default_val) {
    return static_cast<int>(luaL_optinteger(L, i, static_cast<lua_Integer>(default_val)));
}

/// 从 Lua 栈位置 i 提取 bool
inline bool CheckBool(lua_State* L, int i) {
    return lua_toboolean(L, i) != 0;
}

/// 从 Lua 栈位置 i 提取可选 bool，缺省使用 default_val
inline bool OptBool(lua_State* L, int i, bool default_val) {
    return lua_isnoneornil(L, i) ? default_val : (lua_toboolean(L, i) != 0);
}

/// 从 Lua 栈位置 i 提取 const char*
inline const char* CheckString(lua_State* L, int i) {
    return luaL_checkstring(L, i);
}

/// 从 Lua 栈位置 i 提取可选 const char*，缺省使用 default_val
inline const char* OptString(lua_State* L, int i, const char* default_val) {
    return luaL_optstring(L, i, default_val);
}

/// 从 Lua 栈位置 i 提取 Entity
inline Entity CheckEntity(lua_State* L, int i) {
    return LuaEntityFromInteger(luaL_checkinteger(L, i));
}

/// 从 Lua 栈位置 i,i+1,i+2 提取 glm::vec3
inline glm::vec3 CheckVec3(lua_State* L, int i) {
    return glm::vec3(CheckFloat(L, i), CheckFloat(L, i + 1), CheckFloat(L, i + 2));
}

/// 从 Lua 栈位置 i,i+1,i+2,i+3 提取 glm::vec4
inline glm::vec4 CheckVec4(lua_State* L, int i) {
    return glm::vec4(CheckFloat(L, i), CheckFloat(L, i + 1), CheckFloat(L, i + 2), CheckFloat(L, i + 3));
}

// ============================================================
// 返回值推入 — 替代重复的 lua_push* + static_cast<lua_Number>
// ============================================================

/// 推入 float 到 Lua 栈
inline void PushFloat(lua_State* L, float v) {
    lua_pushnumber(L, static_cast<lua_Number>(v));
}

/// 推入 int 到 Lua 栈
inline void PushInt(lua_State* L, int v) {
    lua_pushinteger(L, static_cast<lua_Integer>(v));
}

/// 推入 size_t 到 Lua 栈
inline void PushSize(lua_State* L, std::size_t v) {
    lua_pushinteger(L, static_cast<lua_Integer>(v));
}

/// 推入 bool 到 Lua 栈
inline void PushBool(lua_State* L, bool v) {
    lua_pushboolean(L, v ? 1 : 0);
}

/// 推入 Entity 到 Lua 栈
inline void PushEntity(lua_State* L, Entity e) {
    lua_pushinteger(L, static_cast<lua_Integer>(static_cast<std::uint32_t>(e)));
}

/// 推入 glm::vec3 到 Lua 栈（3 个返回值）
inline void PushVec3(lua_State* L, const glm::vec3& v) {
    PushFloat(L, v.x);
    PushFloat(L, v.y);
    PushFloat(L, v.z);
}

/// 推入 glm::vec4 到 Lua 栈（4 个返回值）
inline void PushVec4(lua_State* L, const glm::vec4& v) {
    PushFloat(L, v.x);
    PushFloat(L, v.y);
    PushFloat(L, v.z);
    PushFloat(L, v.w);
}

// ============================================================
// 安全 World 访问 — 替代重复的 null 检查
// ============================================================

/// 获取 World 指针，若不可用则推入 default_return 个零值并返回
/// 用法: DSE_LUA_GET_WORLD(L, 0) 或 DSE_LUA_GET_WORLD(L, 1) 等
#define DSE_LUA_GET_WORLD(L, default_return) \
    ::dse::runtime::lua_binding::GetWorld(); \
    if (!world) return (default_return)

// 注意：上面的宏有隐式变量声明，在函数开头使用。下面提供更清晰的方式：

/// 获取 World 引用，不可用时推入错误信息并返回指定数量的值
/// 返回 true 表示成功，false 表示 world 不可用（已处理返回）
inline bool TryGetWorld(lua_State* L, World*& out_world) {
    out_world = GetWorld();
    return out_world != nullptr;
}

// ============================================================
// 安全组件访问 — 替代重复的 valid + all_of + get 模式
// ============================================================

/// 获取实体的组件引用，失败时返回 nullptr
template<typename Component>
Component* TryGetComponent(World& world, Entity e) {
    if (!world.registry().valid(e) || !world.registry().all_of<Component>(e)) {
        return nullptr;
    }
    return &world.registry().get<Component>(e);
}

/// 获取实体的 const 组件指针，失败时返回 nullptr
template<typename Component>
const Component* TryGetComponentConst(World& world, Entity e) {
    if (!world.registry().valid(e) || !world.registry().all_of<Component>(e)) {
        return nullptr;
    }
    // try_get 返回指针，get 返回引用；此处需要 const 指针
    return world.registry().try_get<Component>(e);
}

// ============================================================
// 注册辅助 — 简化 pushcfunction + setfield 模式
// ============================================================

/// 在栈顶表中注册一个 C 函数
inline void RegisterFn(lua_State* L, const char* name, lua_CFunction fn) {
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name);
}

/// 批量注册函数到栈顶表 — 接受 {name, fn} 列表
struct BindingEntry {
    const char* name;
    lua_CFunction fn;
};

inline void RegisterBindings(lua_State* L, const BindingEntry* entries, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        RegisterFn(L, entries[i].name, entries[i].fn);
    }
}

/// 使用初始化列表批量注册
inline void RegisterBindings(lua_State* L, std::initializer_list<BindingEntry> entries) {
    for (const auto& entry : entries) {
        RegisterFn(L, entry.name, entry.fn);
    }
}

// ============================================================
// 组件字段 Setter/Getter 生成宏
// ============================================================

/**
 * @def DSE_LUA_COMPONENT_SETTER
 * @brief 生成一个简单的组件字段 setter 绑定函数
 *
 * 生成函数签名: int L_EcsSet##Name(lua_State* L)
 * 函数体: 获取 entity -> 获取 Component -> 设置 field = value
 *
 * @note 必须在 namespace dse::runtime::lua_binding 内使用
 *
 * @param Name     Lua API 后缀（如 TransformPosition）
 * @param Component 组件类型（如 TransformComponent）
 * @param field     字段名（如 position）
 * @param Type      字段类型（如 glm::vec3）
 * @param extract   从 Lua 栈提取值的表达式（使用 L 和起始参数索引）
 */
#define DSE_LUA_COMPONENT_SETTER(Name, Component, field, Type, extract) \
    static int L_EcsSet##Name(lua_State* L) { \
        World* world = GetWorld(); \
        if (!world) return 0; \
        Entity e = helper::CheckEntity(L, 1); \
        auto* comp = helper::TryGetComponent<Component>(*world, e); \
        if (!comp) return 0; \
        comp->field = (extract); \
        return 0; \
    }

/**
 * @def DSE_LUA_COMPONENT_SETTER_POST
 * @brief 生成带后置动作的组件字段 setter 绑定函数
 *
 * 与 SETTER 相同，但在赋值后执行 post_expr（如设置 dirty 标记）。
 *
 * @param Name      Lua API 后缀
 * @param Component 组件类型
 * @param field     字段名
 * @param extract   从 Lua 栈提取值的表达式
 * @param post_expr 赋值后执行的表达式（可使用 comp 指针）
 */
#define DSE_LUA_COMPONENT_SETTER_POST(Name, Component, field, extract, post_expr) \
    static int L_EcsSet##Name(lua_State* L) { \
        World* world = GetWorld(); \
        if (!world) return 0; \
        Entity e = helper::CheckEntity(L, 1); \
        auto* comp = helper::TryGetComponent<Component>(*world, e); \
        if (!comp) return 0; \
        comp->field = (extract); \
        (post_expr); \
        return 0; \
    }

/**
 * @def DSE_LUA_COMPONENT_GETTER
 * @brief 生成一个简单的组件字段 getter 绑定函数
 *
 * 生成函数签名: int L_EcsGet##Name(lua_State* L)
 * 函数体: 获取 entity -> 获取 const Component -> push 字段值
 *
 * @note 必须在 namespace dse::runtime::lua_binding 内使用
 *
 * @param Name     Lua API 后缀（如 TransformPosition）
 * @param Component 组件类型（如 TransformComponent）
 * @param field     字段名（如 position）
 * @param push_expr 将字段值推入 Lua 栈的表达式（使用 comp 和 L）
 * @param ret_count 返回值数量
 */
#define DSE_LUA_COMPONENT_GETTER(Name, Component, field, push_expr, ret_count) \
    static int L_EcsGet##Name(lua_State* L) { \
        World* world = GetWorld(); \
        if (!world) return 0; \
        Entity e = helper::CheckEntity(L, 1); \
        const auto* comp = helper::TryGetComponentConst<Component>(*world, e); \
        if (!comp) return 0; \
        (push_expr); \
        return (ret_count); \
    }

// ============================================================
// 常用组合宏 — 简单字段的 getter/setter 对
// ============================================================

/// 生成 glm::vec3 字段的 getter + setter 对
#define DSE_LUA_COMPONENT_VEC3(Name, Component, field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushVec3(L, comp->field), 3) \
    DSE_LUA_COMPONENT_SETTER(Name, Component, field, glm::vec3, helper::CheckVec3(L, 2))

/// 生成 float 字段的 getter + setter 对
#define DSE_LUA_COMPONENT_FLOAT(Name, Component, field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushFloat(L, comp->field), 1) \
    DSE_LUA_COMPONENT_SETTER(Name, Component, field, float, helper::CheckFloat(L, 2))

/// 生成 bool 字段的 getter + setter 对
#define DSE_LUA_COMPONENT_BOOL(Name, Component, field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushBool(L, comp->field), 1) \
    DSE_LUA_COMPONENT_SETTER(Name, Component, field, bool, helper::CheckBool(L, 2))

/// 生成 int 字段的 getter + setter 对
#define DSE_LUA_COMPONENT_INT(Name, Component, field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushInt(L, comp->field), 1) \
    DSE_LUA_COMPONENT_SETTER(Name, Component, field, int, helper::CheckInt(L, 2))

/// 生成 string 字段的 getter（仅 getter，setter 需要特殊处理内存）
#define DSE_LUA_COMPONENT_STRING_GETTER(Name, Component, field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, lua_pushstring(L, comp->field.c_str()), 1)

// ============================================================
// 带后置动作的组合宏 — 支持 dirty 标记等常见模式
// ============================================================

/// 生成 glm::vec3 字段的 getter + setter（setter 设置后标记 dirty_field = true）
#define DSE_LUA_COMPONENT_VEC3_DIRTY(Name, Component, field, dirty_field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushVec3(L, comp->field), 3) \
    DSE_LUA_COMPONENT_SETTER_POST(Name, Component, field, helper::CheckVec3(L, 2), comp->dirty_field = true)

/// 生成 float 字段的 getter + setter（setter 设置后标记 dirty_field = true）
#define DSE_LUA_COMPONENT_FLOAT_DIRTY(Name, Component, field, dirty_field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushFloat(L, comp->field), 1) \
    DSE_LUA_COMPONENT_SETTER_POST(Name, Component, field, helper::CheckFloat(L, 2), comp->dirty_field = true)

/// 生成 bool 字段的 getter + setter（setter 设置后标记 dirty_field = true）
#define DSE_LUA_COMPONENT_BOOL_DIRTY(Name, Component, field, dirty_field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushBool(L, comp->field), 1) \
    DSE_LUA_COMPONENT_SETTER_POST(Name, Component, field, helper::CheckBool(L, 2), comp->dirty_field = true)

/// 生成 int 字段的 getter + setter（setter 设置后标记 dirty_field = true）
#define DSE_LUA_COMPONENT_INT_DIRTY(Name, Component, field, dirty_field) \
    DSE_LUA_COMPONENT_GETTER(Name, Component, field, helper::PushInt(L, comp->field), 1) \
    DSE_LUA_COMPONENT_SETTER_POST(Name, Component, field, helper::CheckInt(L, 2), comp->dirty_field = true)

} // namespace dse::runtime::lua_binding::helper

#endif // DSE_LUA_BINDING_HELPER_H

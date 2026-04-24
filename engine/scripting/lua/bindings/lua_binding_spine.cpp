#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/ecs/world.h"
#include "engine/ecs/sprite.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

int L_SpineAddRenderer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* skel_path = luaL_checkstring(L, 2);
    const char* atlas_path = luaL_checkstring(L, 3);
    
    auto& spine = world->registry().emplace_or_replace<SpineRendererComponent>(e);
    spine.skeleton_data_path = skel_path;
    spine.atlas_path = atlas_path;
    return 0;
}

int L_SpineSetAnimation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* anim_name = luaL_checkstring(L, 2);
    bool loop = lua_toboolean(L, 3);
    
    if (world->registry().valid(e) && world->registry().all_of<SpineRendererComponent>(e)) {
        auto& spine = world->registry().get<SpineRendererComponent>(e);
        spine.current_animation = anim_name;
        spine.loop = loop;
        spine.dirty_animation = true;
    }
    return 0;
}

void RegisterSpineBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("add_renderer", L_SpineAddRenderer);
    set_fn("set_animation", L_SpineSetAnimation);
}

}

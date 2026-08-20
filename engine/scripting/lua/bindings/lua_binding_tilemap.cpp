/**
 * @file lua_binding_tilemap.cpp
 * @brief Lua 绑定：增强版瓦片地图 (dse.tilemap)
 *
 * API:
 *   -- 创建/配置
 *   local tm = dse.tilemap.create(world, entity)
 *   tm:set_grid(width, height, tile_size)
 *   tm:set_tileset(texture_path, cols, rows)
 *
 *   -- 单层模式（兼容旧 API）
 *   tm:set_tile(x, y, tile_id)
 *   tm:get_tile(x, y) -> tile_id
 *   tm:fill(tile_id)
 *   tm:clear()
 *   tm:mark_dirty()
 *
 *   -- 多层模式
 *   local layer_idx = tm:add_layer(name)
 *   tm:set_layer_tile(layer_idx, x, y, tile_id)
 *   tm:get_layer_tile(layer_idx, x, y) -> tile_id
 *   tm:set_layer_visible(layer_idx, visible)
 *   tm:set_layer_opacity(layer_idx, opacity)
 *   tm:layer_count() -> int
 *
 *   -- 动画瓦片
 *   tm:add_animation(tile_id, frames_table, loop)
 *   -- frames_table: { {id=1, dur=0.2}, {id=2, dur=0.2}, ... }
 *
 *   -- 瓦片属性
 *   tm:set_tile_property(tile_id, {
 *       solid = true,
 *       collision_type = 1,
 *       friction = 0.4,
 *       restitution = 0.0,
 *       custom = { key = "value", ... }
 *   })
 *   tm:get_tile_property(tile_id) -> table
 *
 *   -- 动画时间
 *   tm:set_animation_time(t)
 *   tm:get_animation_time() -> float
 *
 *   -- 序列化
 *   local data = dse.tilemap.save(tm, "path/to/tileset.png")
 *   -- data: 二进制字符串
 *   dse.tilemap.save_to_file(filepath, tm, tileset_path)
 *   local tm2 = dse.tilemap.load(data)  -- 从二进制字符串加载
 *   local tm3 = dse.tilemap.load_from_file(filepath)
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/ecs/tilemap.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dse::runtime::lua_binding {
namespace {

static const char* kMetatableName = "dse_tilemap";

struct TilemapLuaHandle {
    World* world = nullptr;
    entt::entity entity = entt::null;
};

static TilemapLuaHandle* ToHandle(lua_State* L, int idx) {
    return static_cast<TilemapLuaHandle*>(luaL_checkudata(L, idx, kMetatableName));
}

static TilemapComponent* GetTilemap(lua_State* L, int idx) {
    auto* handle = ToHandle(L, idx);
    if (!handle->world || !handle->world->registry().valid(handle->entity)) {
        luaL_error(L, "tilemap: invalid entity handle");
        return nullptr;
    }
    auto* tm = handle->world->registry().try_get<TilemapComponent>(handle->entity);
    if (!tm) {
        luaL_error(L, "tilemap: entity has no TilemapComponent");
        return nullptr;
    }
    return tm;
}

// ── 生命周期 ──────────────────────────────────────────────────────────────

static int L_Create(lua_State* L) {
    // dse.tilemap.create(world_ptr, entity_id)
    auto* world = static_cast<World*>(lua_touserdata(L, 1));
    auto entity = static_cast<entt::entity>(luaL_checkinteger(L, 2));
    if (!world) luaL_error(L, "tilemap.create: world is null");

    auto* handle = static_cast<TilemapLuaHandle*>(
        lua_newuserdata(L, sizeof(TilemapLuaHandle)));
    handle->world = world;
    handle->entity = entity;
    luaL_setmetatable(L, kMetatableName);
    return 1;
}

static int L_GC(lua_State* L) {
    auto* handle = ToHandle(L, 1);
    handle->world = nullptr;
    handle->entity = entt::null;
    return 0;
}

// ── 配置 ──────────────────────────────────────────────────────────────────

static int L_SetGrid(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    tm->width = static_cast<int>(luaL_checkinteger(L, 2));
    tm->height = static_cast<int>(luaL_checkinteger(L, 3));
    tm->tile_size = static_cast<float>(luaL_checknumber(L, 4));
    tm->tiles.assign(static_cast<size_t>(tm->width * tm->height), 0);
    tm->dirty = true;
    return 0;
}

static int L_SetTileset(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    // tileset path 不在此设置（需引擎 asset manager 加载），只设置行列
    tm->tileset_cols = static_cast<int>(luaL_checkinteger(L, 2));
    tm->tileset_rows = static_cast<int>(luaL_checkinteger(L, 3));
    tm->dirty = true;
    return 0;
}

// ── 单层模式 ──────────────────────────────────────────────────────────────

static int L_SetTile(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));
    int tile_id = static_cast<int>(luaL_checkinteger(L, 4));
    if (x < 0 || x >= tm->width || y < 0 || y >= tm->height)
        luaL_error(L, "tilemap.set_tile: out of bounds (%d, %d)", x, y);
    tm->tiles[static_cast<size_t>(y * tm->width + x)] = tile_id;
    tm->dirty = true;
    return 0;
}

static int L_GetTile(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));
    if (x < 0 || x >= tm->width || y < 0 || y >= tm->height) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, tm->tiles[static_cast<size_t>(y * tm->width + x)]);
    return 1;
}

static int L_Fill(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int tile_id = static_cast<int>(luaL_checkinteger(L, 2));
    std::fill(tm->tiles.begin(), tm->tiles.end(), tile_id);
    tm->dirty = true;
    return 0;
}

static int L_Clear(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    std::fill(tm->tiles.begin(), tm->tiles.end(), 0);
    tm->dirty = true;
    return 0;
}

static int L_MarkDirty(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    tm->MarkAllDirty();
    return 0;
}

// ── 多层模式 ──────────────────────────────────────────────────────────────

static int L_AddLayer(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    const char* name = luaL_optstring(L, 2, "layer");
    int idx = tm->AddLayer(name);
    lua_pushinteger(L, idx);
    return 1;
}

static int L_SetLayerTile(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int layer_idx = static_cast<int>(luaL_checkinteger(L, 2));
    int x = static_cast<int>(luaL_checkinteger(L, 3));
    int y = static_cast<int>(luaL_checkinteger(L, 4));
    int tile_id = static_cast<int>(luaL_checkinteger(L, 5));
    auto* layer = tm->GetLayerMut(layer_idx);
    if (!layer) luaL_error(L, "tilemap.set_layer_tile: invalid layer %d", layer_idx);
    if (x < 0 || x >= layer->width || y < 0 || y >= layer->height)
        luaL_error(L, "tilemap.set_layer_tile: out of bounds");
    layer->tiles[static_cast<size_t>(y * layer->width + x)] = tile_id;
    layer->dirty = true;
    return 0;
}

static int L_GetLayerTile(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int layer_idx = static_cast<int>(luaL_checkinteger(L, 2));
    int x = static_cast<int>(luaL_checkinteger(L, 3));
    int y = static_cast<int>(luaL_checkinteger(L, 4));
    auto* layer = tm->GetLayerMut(layer_idx);
    if (!layer) { lua_pushinteger(L, 0); return 1; }
    if (x < 0 || x >= layer->width || y < 0 || y >= layer->height) {
        lua_pushinteger(L, 0); return 1;
    }
    lua_pushinteger(L, layer->tiles[static_cast<size_t>(y * layer->width + x)]);
    return 1;
}

static int L_SetLayerVisible(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int layer_idx = static_cast<int>(luaL_checkinteger(L, 2));
    bool visible = lua_toboolean(L, 3) != 0;
    auto* layer = tm->GetLayerMut(layer_idx);
    if (layer) { layer->visible = visible; layer->dirty = true; }
    return 0;
}

static int L_SetLayerOpacity(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int layer_idx = static_cast<int>(luaL_checkinteger(L, 2));
    float opacity = static_cast<float>(luaL_checknumber(L, 3));
    auto* layer = tm->GetLayerMut(layer_idx);
    if (layer) { layer->opacity = opacity; layer->dirty = true; }
    return 0;
}

static int L_LayerCount(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    lua_pushinteger(L, tm->LayerCount());
    return 1;
}

// ── 动画瓦片 ──────────────────────────────────────────────────────────────

static int L_AddAnimation(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int tile_id = static_cast<int>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);
    bool loop = lua_toboolean(L, 4) != 0;

    TileAnimation anim;
    anim.loop = loop;
    size_t len = lua_rawlen(L, 3);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, 3, static_cast<lua_Integer>(i));
        if (!lua_istable(L, -1)) { lua_pop(L, 1); continue; }
        TileAnimationFrame frame;
        lua_getfield(L, -1, "id");
        frame.tile_id = static_cast<int>(luaL_checkinteger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, -1, "dur");
        frame.duration = static_cast<float>(luaL_optnumber(L, -1, 0.2));
        lua_pop(L, 1);
        anim.frames.push_back(frame);
        lua_pop(L, 1);
    }
    tm->AddAnimation(tile_id, anim);
    return 0;
}

// ── 瓦片属性 ──────────────────────────────────────────────────────────────

static int L_SetTileProperty(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int tile_id = static_cast<int>(luaL_checkinteger(L, 2));
    luaL_checktype(L, 3, LUA_TTABLE);

    TileProperties prop;
    lua_getfield(L, 3, "solid");
    prop.solid = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    lua_getfield(L, 3, "collision_type");
    prop.collision_type = static_cast<int>(luaL_optinteger(L, -1, 0));
    lua_pop(L, 1);

    lua_getfield(L, 3, "friction");
    prop.friction = static_cast<float>(luaL_optnumber(L, -1, 0.4));
    lua_pop(L, 1);

    lua_getfield(L, 3, "restitution");
    prop.restitution = static_cast<float>(luaL_optnumber(L, -1, 0.0));
    lua_pop(L, 1);

    lua_getfield(L, 3, "custom");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            const char* key = lua_tostring(L, -2);
            const char* val = lua_tostring(L, -1);
            if (key && val) prop.Set(key, val);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    tm->SetProperties(tile_id, prop);
    return 0;
}

static int L_GetTileProperty(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    int tile_id = static_cast<int>(luaL_checkinteger(L, 2));
    const auto& prop = tm->GetProperties(tile_id);

    lua_newtable(L);
    lua_pushboolean(L, prop.solid); lua_setfield(L, -2, "solid");
    lua_pushinteger(L, prop.collision_type); lua_setfield(L, -2, "collision_type");
    lua_pushnumber(L, prop.friction); lua_setfield(L, -2, "friction");
    lua_pushnumber(L, prop.restitution); lua_setfield(L, -2, "restitution");

    lua_newtable(L);
    for (const auto& [k, v] : prop.custom) {
        lua_pushstring(L, v.c_str());
        lua_setfield(L, -2, k.c_str());
    }
    lua_setfield(L, -2, "custom");
    return 1;
}

// ── 动画时间 ──────────────────────────────────────────────────────────────

static int L_SetAnimationTime(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    tm->animation_time = static_cast<float>(luaL_checknumber(L, 2));
    tm->dirty = true;
    return 0;
}

static int L_GetAnimationTime(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    lua_pushnumber(L, tm->animation_time);
    return 1;
}

// ── 查询 ──────────────────────────────────────────────────────────────────

static int L_GetWidth(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    lua_pushinteger(L, tm->width);
    return 1;
}

static int L_GetHeight(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    lua_pushinteger(L, tm->height);
    return 1;
}

static int L_GetTileSize(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    lua_pushnumber(L, tm->tile_size);
    return 1;
}

// ── 序列化 ────────────────────────────────────────────────────────────────

static int L_Save(lua_State* L) {
    auto* tm = GetTilemap(L, 1);
    const char* tileset_path = luaL_optstring(L, 2, "");
    auto data = TilemapSerializer::Save(*tm, tileset_path);
    lua_pushlstring(L, reinterpret_cast<const char*>(data.data()), data.size());
    return 1;
}

static int L_SaveToFile(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    auto* tm = GetTilemap(L, 2);
    const char* tileset_path = luaL_optstring(L, 3, "");
    bool ok = TilemapSerializer::SaveToFile(filepath, *tm, tileset_path);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int L_Load(lua_State* L) {
    size_t len = 0;
    const char* data = luaL_checklstring(L, 1, &len);

    TilemapComponent tm;
    std::string tileset_path;
    if (!TilemapSerializer::Load(reinterpret_cast<const uint8_t*>(data), len,
                                 tm, tileset_path)) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushinteger(L, tm.width); lua_setfield(L, -2, "width");
    lua_pushinteger(L, tm.height); lua_setfield(L, -2, "height");
    lua_pushnumber(L, tm.tile_size); lua_setfield(L, -2, "tile_size");
    lua_pushinteger(L, tm.tileset_cols); lua_setfield(L, -2, "tileset_cols");
    lua_pushinteger(L, tm.tileset_rows); lua_setfield(L, -2, "tileset_rows");
    lua_pushstring(L, tileset_path.c_str()); lua_setfield(L, -2, "tileset_path");

    // tiles
    lua_newtable(L);
    for (size_t i = 0; i < tm.tiles.size(); ++i) {
        lua_pushinteger(L, tm.tiles[i]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "tiles");

    // layers
    lua_newtable(L);
    for (size_t i = 0; i < tm.layers.size(); ++i) {
        const auto& layer = tm.layers[i];
        lua_newtable(L);
        lua_pushstring(L, layer.name.c_str()); lua_setfield(L, -2, "name");
        lua_pushnumber(L, layer.opacity); lua_setfield(L, -2, "opacity");
        lua_pushboolean(L, layer.visible); lua_setfield(L, -2, "visible");

        lua_newtable(L);
        for (size_t j = 0; j < layer.tiles.size(); ++j) {
            lua_pushinteger(L, layer.tiles[j]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(j + 1));
        }
        lua_setfield(L, -2, "tiles");

        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    lua_setfield(L, -2, "layers");

    return 1;
}

static int L_LoadFromFile(lua_State* L) {
    const char* filepath = luaL_checkstring(L, 1);
    TilemapComponent tm;
    std::string tileset_path;
    if (!TilemapSerializer::LoadFromFile(filepath, tm, tileset_path)) {
        lua_pushnil(L);
        return 1;
    }
    // 返回二进制数据，用户可用 dse.tilemap.load() 解析
    auto data = TilemapSerializer::Save(tm, tileset_path);
    lua_pushlstring(L, reinterpret_cast<const char*>(data.data()), data.size());
    return 1;
}

static const luaL_Reg kMethods[] = {
    {"set_grid",             L_SetGrid},
    {"set_tileset",          L_SetTileset},
    {"set_tile",             L_SetTile},
    {"get_tile",             L_GetTile},
    {"fill",                 L_Fill},
    {"clear",                L_Clear},
    {"mark_dirty",           L_MarkDirty},
    {"add_layer",            L_AddLayer},
    {"set_layer_tile",       L_SetLayerTile},
    {"get_layer_tile",       L_GetLayerTile},
    {"set_layer_visible",    L_SetLayerVisible},
    {"set_layer_opacity",    L_SetLayerOpacity},
    {"layer_count",          L_LayerCount},
    {"add_animation",        L_AddAnimation},
    {"set_tile_property",    L_SetTileProperty},
    {"get_tile_property",    L_GetTileProperty},
    {"set_animation_time",   L_SetAnimationTime},
    {"get_animation_time",   L_GetAnimationTime},
    {"get_width",            L_GetWidth},
    {"get_height",           L_GetHeight},
    {"get_tile_size",        L_GetTileSize},
    {"save",                 L_Save},
    {nullptr, nullptr}
};

} // anonymous namespace

void RegisterTilemapBindings(lua_State* L) {
    // metatable
    luaL_newmetatable(L, kMetatableName);
    lua_newtable(L);
    luaL_setfuncs(L, kMethods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, L_GC);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // dse.tilemap 模块表
    lua_newtable(L);
    lua_pushcfunction(L, L_Create);
    lua_setfield(L, -2, "create");
    lua_pushcfunction(L, L_SaveToFile);
    lua_setfield(L, -2, "save_to_file");
    lua_pushcfunction(L, L_Load);
    lua_setfield(L, -2, "load");
    lua_pushcfunction(L, L_LoadFromFile);
    lua_setfield(L, -2, "load_from_file");
}

} // namespace dse::runtime::lua_binding

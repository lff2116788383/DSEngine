/**
 * @file lua_binding_compat.cpp
 * @brief Lua 绑定兼容层：修正「生成绑定」中过严/命名不一致的参数约定。
 *
 * 背景：UI/音频等绑定由 codegen 生成，形参多为整数，导致
 *   - ui.set_visible(e, true/false) 报 "number expected, got boolean"
 *   - audio.play_sfx/play_bgm 的 loop 传 false/true 报错
 * 本文件在生成绑定注册之后覆盖同名函数，做 bool/number 双兼容（不改生成物、不改 ABI）。
 */
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api_core.h"
#include "engine/scripting/native_api/dse_api_render.h"
#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/world.h"
#include "engine/ecs/tilemap.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/transform.h"
#include "engine/render/rhi/rhi_handle.h"
#include <glm/gtx/quaternion.hpp>

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
#include <algorithm>
#include <cstring>

namespace {

bool ToBoolish(lua_State* L, int idx, bool def) {
    if (lua_isboolean(L, idx)) return lua_toboolean(L, idx) != 0;
    if (lua_isnumber(L, idx)) return lua_tonumber(L, idx) != 0.0;
    if (lua_isnoneornil(L, idx)) return def;
    return lua_toboolean(L, idx) != 0;
}

int L_SetVisibleCompat(lua_State* L) {
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_set_visible(e, ToBoolish(L, 2, true) ? 1 : 0);
    return 0;
}

int L_PlaySfxCompat(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    dse_audio_play_sfx(path, vol, ToBoolish(L, 3, false) ? 1 : 0);
    return 0;
}

int L_PlayBgmCompat(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    const bool loop = ToBoolish(L, 3, true);
    lua_pushboolean(L, dse_audio_play_bgm(path, vol, loop ? 1 : 0) != 0);
    return 1;
}


//  P2：精灵 UV/排序层/着色器变体/混合模式 + 采样器可控的纹理加载 
int L_SpriteSetUvRect(lua_State* L) {
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_sprite_set_uv_rect(e,
        static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)));
    return 0;
}

int L_SpriteSetSortingLayer(lua_State* L) {
    dse_sprite_set_sorting_layer(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                 static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

int L_SpriteSetShaderVariant(lua_State* L) {
    dse_sprite_set_shader_variant(static_cast<uint32_t>(luaL_checkinteger(L, 1)),
                                  luaL_checkstring(L, 2));
    return 0;
}

// 兼容 "add"/"additive"/"alpha" 字符串与整数（0=Alpha, 1=Additive）
int L_SpriteSetBlendMode(lua_State* L) {
    int mode = 0;
    if (lua_isstring(L, 2)) {
        const char* v = lua_tostring(L, 2);
        mode = (std::strcmp(v, "add") == 0 || std::strcmp(v, "additive") == 0) ? 1 : 0;
    } else {
        mode = static_cast<int>(luaL_optinteger(L, 2, 0));
    }
    dse_sprite_set_blend_mode(static_cast<uint32_t>(luaL_checkinteger(L, 1)), mode);
    return 0;
}

// assets.load_texture_ex(path, filter, wrap)；filter: "nearest"|"linear", wrap: "repeat"|"clamp"
int L_LoadTextureEx(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int filter = 1;
    int wrap = 0;
    if (lua_isstring(L, 2)) {
        filter = (std::strcmp(lua_tostring(L, 2), "nearest") == 0) ? 0 : 1;
    } else if (lua_isnumber(L, 2)) {
        filter = static_cast<int>(lua_tointeger(L, 2));
    }
    if (lua_isstring(L, 3)) {
        const char* w = lua_tostring(L, 3);
        wrap = (std::strcmp(w, "clamp") == 0 || std::strcmp(w, "clamp_to_edge") == 0) ? 1 : 0;
    } else if (lua_isnumber(L, 3)) {
        wrap = static_cast<int>(lua_tointeger(L, 3));
    }
    lua_pushinteger(L, static_cast<lua_Integer>(dse_assets_load_texture_ex(path, filter, wrap)));
    return 1;
}


//  P3：瓦片地图可用化 
// ecs.add_tilemap 的生成绑定不设置 tileset_cols/rows（默认 1x1）且 tiles 初值为 -1，
// 导致整张图集被压进每个格子且 set_tile 语义异常。这里提供显式行列版本：
//   ecs.add_tilemap_ex(e, w, h, tile_size, tex_handle, cols, rows)
int L_AddTilemapEx(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    const auto e = dse_api_internal::TE(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    if (!world->registry().valid(e)) return 0;
    auto& tm = world->registry().emplace_or_replace<TilemapComponent>(e);
    tm.width = static_cast<int>(luaL_checkinteger(L, 2));
    tm.height = static_cast<int>(luaL_checkinteger(L, 3));
    tm.tile_size = static_cast<float>(luaL_checknumber(L, 4));
    tm.tileset_handle = dse::render::TextureHandle::from_raw(
        static_cast<uint32_t>(luaL_checkinteger(L, 5)));
    tm.tileset_cols = static_cast<int>(luaL_optinteger(L, 6, 1));
    tm.tileset_rows = static_cast<int>(luaL_optinteger(L, 7, 1));
    tm.tiles.assign(static_cast<size_t>(tm.width) * static_cast<size_t>(tm.height), 0);
    tm.dirty = true;
    return 0;
}

// ecs.tilemap_set_colliders(e, enabled, tile_min)：为 >= tile_min 的格子生成 Box2D 静态碰撞体
int L_TilemapSetColliders(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    auto* tm = world->registry().try_get<TilemapComponent>(
        dse_api_internal::TE(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    if (!tm) return 0;
    tm->generate_colliders = ToBoolish(L, 2, false);
    tm->collider_tile_min = static_cast<int>(luaL_optinteger(L, 3, 1));
    tm->dirty = true;
    return 0;
}


// ecs.get_tile(e, x, y)：生成绑定未导出，补上（配合 add_tilemap_ex 形成完整读写闭环）
int L_GetTile(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) { lua_pushinteger(L, 0); return 1; }
    auto* tm = world->registry().try_get<TilemapComponent>(
        dse_api_internal::TE(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    const int x = static_cast<int>(luaL_checkinteger(L, 2));
    const int y = static_cast<int>(luaL_checkinteger(L, 3));
    int v = 0;
    if (tm && x >= 0 && y >= 0 && x < tm->width && y < tm->height && !tm->tiles.empty()) {
        v = tm->tiles[static_cast<size_t>(y) * tm->width + x];
    }
    lua_pushinteger(L, v);
    return 1;
}

// ============================================================
//  HD-2D M1: Sprite3D Lua compatibility API
//  The component lives in reflect_only_components (reflection + scene codec),
//  while all script-facing creation/setters are implemented here directly on
//  the ECS registry to keep the task's hand-written compatibility-layer rule.
// ============================================================

int BillboardFromName(const char* value) {
    if (!value) return 1;
    if (std::strcmp(value, "none") == 0) return 0;
    if (std::strcmp(value, "yaw") == 0) return 1;
    if (std::strcmp(value, "yaw_pitch") == 0 || std::strcmp(value, "yawpitch") == 0) return 2;
    if (std::strcmp(value, "screen") == 0) return 3;
    return 1;
}

Sprite3DComponent* GetSprite3D(World* world, uint32_t e) {
    if (!world || !world->registry().valid(dse_api_internal::TE(e))) return nullptr;
    return world->registry().try_get<Sprite3DComponent>(dse_api_internal::TE(e));
}

// ecs.add_sprite3d(e, tex, w, h, {billboard="yaw", anchor=0.0, lit=false})
int L_AddSprite3D(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    if (!world->registry().valid(dse_api_internal::TE(e))) return 0;

    auto& c = world->registry().emplace_or_replace<Sprite3DComponent>(dse_api_internal::TE(e));
    const uint32_t tex = static_cast<uint32_t>(luaL_optinteger(L, 2, 0));
    c.texture_handle = dse::render::TextureRef(dse::render::TextureHandle::from_raw(tex));
    c.size_w = static_cast<float>(luaL_checknumber(L, 3));
    c.size_h = static_cast<float>(luaL_checknumber(L, 4));

    if (lua_istable(L, 5)) {
        lua_getfield(L, 5, "billboard");
        if (lua_isstring(L, -1)) {
            c.billboard = BillboardFromName(lua_tostring(L, -1));
        } else if (lua_isnumber(L, -1)) {
            c.billboard = static_cast<int>(lua_tointeger(L, -1));
        }
        lua_pop(L, 1);

        lua_getfield(L, 5, "anchor");
        if (lua_isnumber(L, -1)) c.anchor_y = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 5, "lit");
        if (!lua_isnoneornil(L, -1)) c.lit = ToBoolish(L, -1, false);
        lua_pop(L, 1);

        lua_getfield(L, 5, "receive_shadow");
        if (!lua_isnoneornil(L, -1)) c.receive_shadow = ToBoolish(L, -1, false);
        lua_pop(L, 1);

        lua_getfield(L, 5, "z_offset");
        if (lua_isnumber(L, -1)) c.z_offset = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, 5, "sorting_bias");
        if (lua_isnumber(L, -1)) c.sorting_bias = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return 0;
}

int L_Sprite3DSetUvRect(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->uv_rect = glm::vec4(
            static_cast<float>(luaL_checknumber(L, 2)),
            static_cast<float>(luaL_checknumber(L, 3)),
            static_cast<float>(luaL_checknumber(L, 4)),
            static_cast<float>(luaL_checknumber(L, 5)));
    }
    return 0;
}

int L_Sprite3DSetBillboard(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        if (lua_isstring(L, 2)) {
            c->billboard = BillboardFromName(lua_tostring(L, 2));
        } else {
            c->billboard = static_cast<int>(luaL_optinteger(L, 2, 1));
        }
    }
    return 0;
}

int L_Sprite3DSetLit(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->lit = ToBoolish(L, 2, false);
    }
    return 0;
}
int L_Sprite3DSetReceiveShadow(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->receive_shadow = ToBoolish(L, 2, false);
    }
    return 0;
}

int L_Sprite3DSetNormal(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        const uint32_t tex = static_cast<uint32_t>(luaL_optinteger(L, 2, 0));
        c->normal_handle = dse::render::TextureRef(dse::render::TextureHandle::from_raw(tex));
        c->normal_strength = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    }
    return 0;
}

int L_Sprite3DSetContactShadow(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->contact_shadow = ToBoolish(L, 2, false);
        c->contact_shadow_radius = static_cast<float>(luaL_optnumber(L, 3, 0.5));
        c->contact_shadow_opacity = static_cast<float>(luaL_optnumber(L, 4, 0.45));
    }
    return 0;
}
int L_LoadSpriteAtlas(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const int handle = dse_sprite_atlas_load(path, 0, 1);  // nearest + clamp
    lua_pushinteger(L, handle);
    return 1;
}

int L_Sprite3DSetAtlas(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto* c = GetSprite3D(world, e);
    if (!c) return 0;
    const int atlas = static_cast<int>(luaL_checkinteger(L, 2));
    const char* clip = luaL_checkstring(L, 3);
    const uint32_t texture = dse_sprite_atlas_texture(atlas);
    if (!texture) return 0;

    c->texture_handle = dse::render::TextureRef(dse::render::TextureHandle::from_raw(texture));
    c->atlas_handle = atlas;
    c->clip_name = clip ? clip : "";
    char path_buffer[1024] = {0};
    if (dse_sprite_atlas_path(atlas, path_buffer, static_cast<int>(sizeof(path_buffer)))) {
        c->atlas_path = path_buffer;
    }
    const uint32_t normal_texture = dse_sprite_atlas_normal_texture(atlas);
    c->normal_handle = normal_texture
        ? dse::render::TextureRef(dse::render::TextureHandle::from_raw(normal_texture))
        : dse::render::TextureRef{};
    const uint32_t emissive_texture = dse_sprite_atlas_emissive_texture(atlas);
    c->emissive_handle = emissive_texture
        ? dse::render::TextureRef(dse::render::TextureHandle::from_raw(emissive_texture))
        : dse::render::TextureRef{};

    const int frame_count = dse_sprite_atlas_clip_frame_count(atlas, c->clip_name.c_str());
    c->clip_uvs.clear();
    c->clip_uvs.reserve(static_cast<size_t>(std::max(frame_count, 0)));
    for (int i = 0; i < frame_count; ++i) {
        float uv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        dse_sprite_atlas_clip_frame_uv(atlas, c->clip_name.c_str(), i, uv);
        c->clip_uvs.emplace_back(uv[0], uv[1], uv[2], uv[3]);
    }
    if (!c->clip_uvs.empty()) c->uv_rect = c->clip_uvs.front();
    c->anim_fps = dse_sprite_atlas_clip_fps(atlas, c->clip_name.c_str());
    c->anim_loop = dse_sprite_atlas_clip_loop(atlas, c->clip_name.c_str()) != 0;
    c->anim_time = 0.0f;
    c->anim_frame = 0;
    c->anim_playing = !c->clip_uvs.empty();
    return 0;
}

int L_Sprite3DSetAnim(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto* c = GetSprite3D(world, e);
    if (!c) return 0;
    const char* clip = luaL_checkstring(L, 2);
    if (!clip) return 0;

    if (c->atlas_handle >= 0 && c->clip_name != clip) {
        const int frame_count = dse_sprite_atlas_clip_frame_count(c->atlas_handle, clip);
        if (frame_count <= 0) return 0;
        c->clip_name = clip;
        c->clip_uvs.clear();
        c->clip_uvs.reserve(static_cast<size_t>(frame_count));
        for (int i = 0; i < frame_count; ++i) {
            float uv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
            dse_sprite_atlas_clip_frame_uv(c->atlas_handle, clip, i, uv);
            c->clip_uvs.emplace_back(uv[0], uv[1], uv[2], uv[3]);
        }
        c->uv_rect = c->clip_uvs.front();
        c->anim_fps = dse_sprite_atlas_clip_fps(c->atlas_handle, clip);
        c->anim_loop = dse_sprite_atlas_clip_loop(c->atlas_handle, clip) != 0;
    }
    if (c->clip_uvs.empty()) return 0;

    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "fps");
        if (lua_isnumber(L, -1)) c->anim_fps = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 3, "loop");
        if (!lua_isnoneornil(L, -1)) c->anim_loop = ToBoolish(L, -1, c->anim_loop);
        lua_pop(L, 1);
    }
    c->anim_time = 0.0f;
    c->anim_frame = 0;
    c->anim_playing = c->anim_fps > 0.0f;
    c->uv_rect = c->clip_uvs.front();
    return 0;
}

int L_Sprite3DSetSortingBias(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->sorting_bias = static_cast<float>(luaL_checknumber(L, 2));
    }
    return 0;
}

int L_Sprite3DSetEmissive(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->emissive = glm::vec3(
            static_cast<float>(luaL_checknumber(L, 2)),
            static_cast<float>(luaL_checknumber(L, 3)),
            static_cast<float>(luaL_checknumber(L, 4)));
    }
    return 0;
}

int L_Sprite3DSetSize(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->size_w = static_cast<float>(luaL_checknumber(L, 2));
        c->size_h = static_cast<float>(luaL_checknumber(L, 3));
    }
    return 0;
}

int L_Sprite3DSetColorTint(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        const float r = static_cast<float>(luaL_checknumber(L, 2));
        const float g = static_cast<float>(luaL_checknumber(L, 3));
        const float b = static_cast<float>(luaL_checknumber(L, 4));
        const float a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
        c->color_tint = glm::vec4(r, g, b, a);
    }
    return 0;
}

int L_Sprite3DSetOpacity(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (auto* c = GetSprite3D(world, static_cast<uint32_t>(luaL_checkinteger(L, 1)))) {
        c->opacity = static_cast<float>(luaL_checknumber(L, 2));
    }
    return 0;
}

int L_SetCameraOrtho3D(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto* cam = world->registry().try_get<Camera3DComponent>(static_cast<entt::entity>(dse_api_internal::TE(e)));
    if (!cam) return 0;
    cam->orthographic = true;
    cam->ortho_size = static_cast<float>(luaL_optnumber(L, 2, 5.0));
    if (auto* tf = world->registry().try_get<TransformComponent>(static_cast<entt::entity>(dse_api_internal::TE(e)))) {
        const float pitch = static_cast<float>(luaL_optnumber(L, 3, 0.0));
        const float yaw = static_cast<float>(luaL_optnumber(L, 4, 0.0));
        tf->rotation = glm::quat(glm::vec3(glm::radians(pitch), glm::radians(yaw), 0.0f));
        tf->dirty = true;
    }
    return 0;
}

int L_SetPostProcessTiltShift(lua_State* L) {
    World* world = dse_api_internal::GW();
    if (!world) return 0;
    const uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto* pp = world->registry().try_get<PostProcessComponent>(static_cast<entt::entity>(dse_api_internal::TE(e)));
    if (!pp) return 0;
    pp->dof_enabled = ToBoolish(L, 2, false);
    pp->dof_focus_distance = static_cast<float>(luaL_optnumber(L, 3, pp->dof_focus_distance));
    pp->dof_focus_range = static_cast<float>(luaL_optnumber(L, 4, pp->dof_focus_range));
    pp->dof_bokeh_radius = static_cast<float>(luaL_optnumber(L, 5, pp->dof_bokeh_radius));
    return 0;
}

void Override(lua_State* L, const char* table, const char* name, lua_CFunction fn) {
    lua_getglobal(L, "dse");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return; }
    lua_getfield(L, -1, table);
    if (lua_istable(L, -1)) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    }
    lua_pop(L, 2);
}

}  // namespace

void RegisterCompatBindings(lua_State* L) {
    Override(L, "ui", "set_visible", L_SetVisibleCompat);
    Override(L, "audio", "play_sfx", L_PlaySfxCompat);
    Override(L, "audio", "play_bgm", L_PlayBgmCompat);
    Override(L, "ecs", "set_sprite_uv_rect", L_SpriteSetUvRect);
    Override(L, "ecs", "set_sprite_sorting_layer", L_SpriteSetSortingLayer);
    Override(L, "ecs", "set_sprite_shader_variant", L_SpriteSetShaderVariant);
    Override(L, "ecs", "set_sprite_blend_mode", L_SpriteSetBlendMode);
    Override(L, "assets", "load_texture_ex", L_LoadTextureEx);
    Override(L, "ecs", "add_tilemap_ex", L_AddTilemapEx);
    Override(L, "ecs", "tilemap_set_colliders", L_TilemapSetColliders);
    Override(L, "ecs", "get_tile", L_GetTile);
    // HD-2D M1 Sprite3D API
    Override(L, "ecs", "add_sprite3d", L_AddSprite3D);
    Override(L, "ecs", "set_sprite3d_uv_rect", L_Sprite3DSetUvRect);
    Override(L, "ecs", "set_sprite3d_billboard", L_Sprite3DSetBillboard);
    Override(L, "ecs", "set_sprite3d_lit", L_Sprite3DSetLit);
    Override(L, "ecs", "set_sprite3d_receive_shadow", L_Sprite3DSetReceiveShadow);
    Override(L, "ecs", "set_sprite3d_normal", L_Sprite3DSetNormal);
    Override(L, "ecs", "set_sprite3d_contact_shadow", L_Sprite3DSetContactShadow);
    Override(L, "ecs", "set_sprite3d_atlas", L_Sprite3DSetAtlas);
    Override(L, "ecs", "set_sprite3d_anim", L_Sprite3DSetAnim);
    Override(L, "assets", "load_sprite_atlas", L_LoadSpriteAtlas);
    Override(L, "ecs", "set_camera_ortho_3d", L_SetCameraOrtho3D);
    Override(L, "ecs", "set_post_process_tilt_shift", L_SetPostProcessTiltShift);
    Override(L, "ecs", "set_sprite3d_sorting_bias", L_Sprite3DSetSortingBias);
    Override(L, "ecs", "set_sprite3d_emissive", L_Sprite3DSetEmissive);
    Override(L, "ecs", "set_sprite3d_size", L_Sprite3DSetSize);
    Override(L, "ecs", "set_sprite3d_color_tint", L_Sprite3DSetColorTint);
    Override(L, "ecs", "set_sprite3d_opacity", L_Sprite3DSetOpacity);
}

}  // namespace dse::runtime::lua_binding
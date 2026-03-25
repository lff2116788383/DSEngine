#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/ecs/components_2d.h"
extern "C" {
#include <lauxlib.h>
}

namespace dse::runtime::lua_binding {
namespace {
int L_UiAddRenderer(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    unsigned int tex_handle = static_cast<unsigned int>(luaL_optinteger(L, 2, 0));
    float r = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    int order = static_cast<int>(luaL_optinteger(L, 7, 0));
    auto& ui = world->registry().emplace_or_replace<UIRendererComponent>(e);
    ui.texture_handle = tex_handle;
    ui.color = glm::vec4(r, g, b, a);
    ui.order = order;
    ui.size = glm::vec2(1.0f, 1.0f);
    return 0;
}

int L_UiAddLabel(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    unsigned int font_tex_handle = static_cast<unsigned int>(luaL_optinteger(L, 3, 0));
    float r = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    float glyph_w = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    float glyph_h = static_cast<float>(luaL_optnumber(L, 9, 0.0));
    float spacing = static_cast<float>(luaL_optnumber(L, 10, 0.0));
    int atlas_cols = static_cast<int>(luaL_optinteger(L, 11, 0));
    int atlas_rows = static_cast<int>(luaL_optinteger(L, 12, 0));
    int ascii_start = static_cast<int>(luaL_optinteger(L, 13, 0));
    float offset_x = static_cast<float>(luaL_optnumber(L, 14, 0.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 15, 0.0));
    auto& label = world->registry().emplace_or_replace<UILabelComponent>(e);
    label.text = text;
    label.font_texture_handle = font_tex_handle;
    label.color = glm::vec4(r, g, b, a);
    if (glyph_w > 0.0f && glyph_h > 0.0f) {
        label.glyph_size = glm::vec2(glyph_w, glyph_h);
    }
    if (spacing != 0.0f) {
        label.spacing = spacing;
    }
    if (atlas_cols > 0) {
        label.atlas_cols = atlas_cols;
    }
    if (atlas_rows > 0) {
        label.atlas_rows = atlas_rows;
    }
    if (ascii_start > 0) {
        label.ascii_start = ascii_start;
    }
    if (offset_x != 0.0f || offset_y != 0.0f) {
        label.offset = glm::vec2(offset_x, offset_y);
    }
    label.dirty = true;
    return 0;
}

int L_UiAddPanel(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool blocks_input = lua_toboolean(L, 2);
    auto& panel = world->registry().emplace_or_replace<UIPanelComponent>(e);
    panel.blocks_input = blocks_input;
    return 0;
}

int L_UiAddButton(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    auto& button = world->registry().emplace_or_replace<UIButtonComponent>(e);
    if (lua_gettop(L) >= 5) {
        button.normal_color = glm::vec4(
            static_cast<float>(luaL_optnumber(L, 2, 1.0)),
            static_cast<float>(luaL_optnumber(L, 3, 1.0)),
            static_cast<float>(luaL_optnumber(L, 4, 1.0)),
            static_cast<float>(luaL_optnumber(L, 5, 1.0))
        );
        button.hover_color = button.normal_color * glm::vec4(1.1f, 1.1f, 1.1f, 1.0f);
        button.pressed_color = button.normal_color * glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    }
    if (world->registry().valid(e) && !world->registry().all_of<UIRendererComponent>(e)) {
        world->registry().emplace<UIRendererComponent>(e);
    }
    return 0;
}

int L_UiSetLabelText(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* text = luaL_checkstring(L, 2);
    if (world->registry().valid(e) && world->registry().all_of<UILabelComponent>(e)) {
        auto& label = world->registry().get<UILabelComponent>(e);
        label.numeric_mode = false;
        label.text = text;
        label.dirty = true;
    }
    return 0;
}

int L_UiSetLabelNumber(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    long long number = static_cast<long long>(luaL_checkinteger(L, 2));
    if (world->registry().valid(e) && world->registry().all_of<UILabelComponent>(e)) {
        auto& label = world->registry().get<UILabelComponent>(e);
        label.numeric_mode = true;
        label.number_value = number;
        label.dirty = true;
    }
    return 0;
}

int L_UiSetButtonScale(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float hover_scale = static_cast<float>(luaL_optnumber(L, 2, 1.08));
    float pressed_scale = static_cast<float>(luaL_optnumber(L, 3, 0.94));
    float lerp_speed = static_cast<float>(luaL_optnumber(L, 4, 12.0));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.hover_scale = hover_scale;
        ui.pressed_scale = pressed_scale;
        ui.scale_lerp_speed = lerp_speed;
    }
    return 0;
}
}

void RegisterUiBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("add_renderer", L_UiAddRenderer);
    set_fn("add_label", L_UiAddLabel);
    set_fn("set_label_text", L_UiSetLabelText);
    set_fn("set_label_number", L_UiSetLabelNumber);
    set_fn("add_panel", L_UiAddPanel);
    set_fn("add_button", L_UiAddButton);
    set_fn("set_button_scale", L_UiSetButtonScale);
}

}

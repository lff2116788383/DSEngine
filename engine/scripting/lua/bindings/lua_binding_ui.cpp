/**
 * @file lua_binding_ui.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/ecs/ui.h"
extern "C" {
#include "depends/lua/lauxlib.h"
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
    ui.size = glm::vec2(
        static_cast<float>(luaL_optnumber(L, 8, 640.0)),
        static_cast<float>(luaL_optnumber(L, 9, 160.0)));
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
    auto& ui = world->registry().emplace_or_replace<UIRendererComponent>(e);
    ui.texture_handle = font_tex_handle;
    ui.color = glm::vec4(r, g, b, 0.0f);
    ui.visible = true;
    ui.interactable = false;
    ui.position = glm::vec2(offset_x, offset_y);
    ui.size = glm::vec2(0.0f, 0.0f);

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

/**
 * @brief Lua 绑定：给实体添加 UI 遮罩组件
 * @param L Lua 状态机，参数为 (实体 ID, width, height, offset_x, offset_y, block_outside)
 * @return 无
 * @example ui.add_mask(entity, 200, 200, 0, 0, true)
 */
int L_UiAddMask(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float width = static_cast<float>(luaL_optnumber(L, 2, 0.0));
    float height = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float offset_x = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float offset_y = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    bool block_outside = lua_gettop(L) >= 6 ? lua_toboolean(L, 6) != 0 : true;
    auto& mask = world->registry().emplace_or_replace<UIMaskComponent>(e);
    mask.size = glm::vec2(width, height);
    mask.offset = glm::vec2(offset_x, offset_y);
    mask.block_outside_input = block_outside;
    mask.enabled = true;
    return 0;
}

/**
 * @brief Lua 绑定：给实体添加 UI 富文本组件
 * @param L Lua 状态机，参数为 (实体 ID, text, r, g, b, a, shadow, outline)
 * @return 无
 * @example ui.add_rich_text(entity, "<color=#ff0000>Hello</color>", 1, 1, 1, 1, true, false)
 */
int L_UiAddRichText(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* text = luaL_optstring(L, 2, "");
    float r = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    float g = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float b = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float a = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    bool shadow = lua_gettop(L) >= 7 ? lua_toboolean(L, 7) != 0 : false;
    bool outline = lua_gettop(L) >= 8 ? lua_toboolean(L, 8) != 0 : false;
    auto& rich = world->registry().emplace_or_replace<UIRichTextComponent>(e);
    rich.text = text;
    rich.default_color = glm::vec4(r, g, b, a);
    rich.enable_shadow = shadow;
    rich.enable_outline = outline;
    rich.dirty = true;
    if (!world->registry().all_of<UILabelComponent>(e)) {
        world->registry().emplace<UILabelComponent>(e);
    }
    if (!world->registry().all_of<UIRendererComponent>(e)) {
        world->registry().emplace<UIRendererComponent>(e);
    }
    auto& label = world->registry().get<UILabelComponent>(e);
    label.dirty = true;
    return 0;
}

/**
 * @brief Lua 绑定：修改富文本组件的内容
 * @param L Lua 状态机，参数为 (实体 ID, text)
 * @return 无
 * @example ui.set_rich_text(entity, "New Text")
 */
int L_UiSetRichText(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* text = luaL_optstring(L, 2, "");
    if (world->registry().valid(e) && world->registry().all_of<UIRichTextComponent>(e)) {
        auto& rich = world->registry().get<UIRichTextComponent>(e);
        rich.text = text;
        rich.dirty = true;
        if (world->registry().all_of<UILabelComponent>(e)) {
            world->registry().get<UILabelComponent>(e).dirty = true;
        }
    }
    return 0;
}

/**
 * @brief Lua 绑定：给实体添加 UI 虚拟摇杆组件
 * @param L Lua 状态机，参数为 (实体 ID, max_radius, follow_pointer, reset_on_release)
 * @return 无
 * @example ui.add_joystick(entity, 64.0, true, true)
 */
int L_UiAddJoystick(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float max_radius = static_cast<float>(luaL_optnumber(L, 2, 64.0));
    bool follow_pointer = lua_gettop(L) >= 3 ? lua_toboolean(L, 3) != 0 : true;
    bool reset_on_release = lua_gettop(L) >= 4 ? lua_toboolean(L, 4) != 0 : true;
    auto& joystick = world->registry().emplace_or_replace<UIJoystickComponent>(e);
    joystick.max_radius = max_radius;
    joystick.follow_pointer = follow_pointer;
    joystick.reset_on_release = reset_on_release;
    joystick.direction = glm::vec2(0.0f);
    joystick.is_dragging = false;
    if (!world->registry().all_of<UIRendererComponent>(e)) {
        world->registry().emplace<UIRendererComponent>(e);
    }
    return 0;
}

/**
 * @brief Lua 绑定：获取摇杆的 X 轴输入方向
 * @param L Lua 状态机，参数为 (实体 ID)
 * @return 压入一个数字，表示摇杆 X 轴标准化方向 (-1 到 1)
 * @example local x = ui.get_joystick_x(entity)
 */
int L_UiGetJoystickX(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<UIJoystickComponent>(e)) {
        lua_pushnumber(L, static_cast<lua_Number>(world->registry().get<UIJoystickComponent>(e).direction.x));
    } else {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}

/**
 * @brief Lua 绑定：获取摇杆的 Y 轴输入方向
 * @param L Lua 状态机，参数为 (实体 ID)
 * @return 压入一个数字，表示摇杆 Y 轴标准化方向 (-1 到 1)
 * @example local y = ui.get_joystick_y(entity)
 */
int L_UiGetJoystickY(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<UIJoystickComponent>(e)) {
        lua_pushnumber(L, static_cast<lua_Number>(world->registry().get<UIJoystickComponent>(e).direction.y));
    } else {
        lua_pushnumber(L, 0.0);
    }
    return 1;
}
}

int L_UiSetPosition(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.position = glm::vec2(x, y);
    }
    return 0;
}

int L_UiSetSize(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float w = static_cast<float>(luaL_checknumber(L, 2));
    float h = static_cast<float>(luaL_checknumber(L, 3));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.size = glm::vec2(w, h);
    }
    return 0;
}

int L_UiSetAnchor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float ax = static_cast<float>(luaL_checknumber(L, 2));
    float ay = static_cast<float>(luaL_checknumber(L, 3));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.anchor_min = glm::vec2(ax, ay);
        ui.anchor_max = glm::vec2(ax, ay);
    }
    return 0;
}

int L_UiSetColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.color = glm::vec4(r, g, b, a);
    }
    return 0;
}

int L_UiSetVisible(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool vis = lua_toboolean(L, 2) != 0;
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.visible = vis;
    }
    return 0;
}

int L_UiSetUv(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float u = static_cast<float>(luaL_checknumber(L, 2));
    float v = static_cast<float>(luaL_checknumber(L, 3));
    float w = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    float h = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.uv = glm::vec4(u, v, w, h);
    }
    return 0;
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
    set_fn("add_mask", L_UiAddMask);
    set_fn("add_rich_text", L_UiAddRichText);
    set_fn("set_rich_text", L_UiSetRichText);
    set_fn("add_joystick", L_UiAddJoystick);
    set_fn("get_joystick_x", L_UiGetJoystickX);
    set_fn("get_joystick_y", L_UiGetJoystickY);
    set_fn("set_position", L_UiSetPosition);
    set_fn("set_size", L_UiSetSize);
    set_fn("set_anchor", L_UiSetAnchor);
    set_fn("set_color", L_UiSetColor);
    set_fn("set_visible", L_UiSetVisible);
    set_fn("set_uv", L_UiSetUv);
}

}

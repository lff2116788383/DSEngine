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

int L_UiSetNineSlice(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool enabled = lua_toboolean(L, 2) != 0;
    float left   = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    float bottom = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    float right  = static_cast<float>(luaL_optnumber(L, 5, 0.0));
    float top    = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    float src_w  = static_cast<float>(luaL_optnumber(L, 7, 0.0));
    float src_h  = static_cast<float>(luaL_optnumber(L, 8, 0.0));
    if (world->registry().valid(e) && world->registry().all_of<UIRendererComponent>(e)) {
        auto& ui = world->registry().get<UIRendererComponent>(e);
        ui.nine_slice_enabled  = enabled;
        ui.nine_slice_border   = glm::vec4(left, bottom, right, top);
        ui.nine_slice_src_size = glm::vec2(src_w, src_h);
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

// ============================================================
// UIAnchorComponent 绑定
// ============================================================

// ui.add_anchor(entity, anchor_type, offset_x, offset_y)
int L_UiAddAnchor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e)) return 0;
    auto& anchor = world->registry().emplace_or_replace<UIAnchorComponent>(e);
    anchor.anchor = static_cast<int>(luaL_optinteger(L, 2, 5));
    anchor.offset.x = static_cast<float>(luaL_optnumber(L, 3, 0.0));
    anchor.offset.y = static_cast<float>(luaL_optnumber(L, 4, 0.0));
    return 0;
}

// ui.set_anchor_type(entity, anchor_type)
int L_UiSetAnchorType(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnchorComponent>(e)) return 0;
    auto& anchor = world->registry().get<UIAnchorComponent>(e);
    anchor.anchor = static_cast<int>(luaL_checkinteger(L, 2));
    return 0;
}

// ui.set_anchor_offset(entity, offset_x, offset_y)
int L_UiSetAnchorOffset(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnchorComponent>(e)) return 0;
    auto& anchor = world->registry().get<UIAnchorComponent>(e);
    anchor.offset.x = static_cast<float>(luaL_checknumber(L, 2));
    anchor.offset.y = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

// ============================================================
// UIGridLayoutComponent 绑定
// ============================================================

// ui.add_grid_layout(entity, columns, cell_w, cell_h, spacing_x, spacing_y)
int L_UiAddGridLayout(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e)) return 0;
    auto& grid = world->registry().emplace_or_replace<UIGridLayoutComponent>(e);
    grid.columns = static_cast<int>(luaL_optinteger(L, 2, 1));
    grid.cell_size.x = static_cast<float>(luaL_optnumber(L, 3, 100.0));
    grid.cell_size.y = static_cast<float>(luaL_optnumber(L, 4, 100.0));
    grid.spacing.x = static_cast<float>(luaL_optnumber(L, 5, 10.0));
    grid.spacing.y = static_cast<float>(luaL_optnumber(L, 6, 10.0));
    return 0;
}

// ui.set_grid_layout(entity, columns, rows, cell_w, cell_h, spacing_x, spacing_y, alignment)
int L_UiSetGridLayout(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIGridLayoutComponent>(e)) return 0;
    auto& grid = world->registry().get<UIGridLayoutComponent>(e);
    if (!lua_isnoneornil(L, 2)) grid.columns = static_cast<int>(luaL_checkinteger(L, 2));
    if (!lua_isnoneornil(L, 3)) grid.rows = static_cast<int>(luaL_checkinteger(L, 3));
    if (!lua_isnoneornil(L, 4)) grid.cell_size.x = static_cast<float>(luaL_checknumber(L, 4));
    if (!lua_isnoneornil(L, 5)) grid.cell_size.y = static_cast<float>(luaL_checknumber(L, 5));
    if (!lua_isnoneornil(L, 6)) grid.spacing.x = static_cast<float>(luaL_checknumber(L, 6));
    if (!lua_isnoneornil(L, 7)) grid.spacing.y = static_cast<float>(luaL_checknumber(L, 7));
    if (!lua_isnoneornil(L, 8)) grid.alignment = static_cast<int>(luaL_checkinteger(L, 8));
    return 0;
}

// ============================================================
// UICanvasScalerComponent 绑定
// ============================================================

// ui.add_canvas_scaler(entity, ref_w, ref_h, [match_width_or_height])
int L_UiAddCanvasScaler(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e)) return 0;
    auto& scaler = world->registry().emplace_or_replace<UICanvasScalerComponent>(e);
    scaler.reference_resolution.x = static_cast<float>(luaL_optnumber(L, 2, 1920.0));
    scaler.reference_resolution.y = static_cast<float>(luaL_optnumber(L, 3, 1080.0));
    scaler.match_width_or_height = lua_isnoneornil(L, 4) ? true : (lua_toboolean(L, 4) != 0);
    return 0;
}

// ui.set_canvas_scaler(entity, ref_w, ref_h, scale_factor, match_width_or_height)
int L_UiSetCanvasScaler(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UICanvasScalerComponent>(e)) return 0;
    auto& scaler = world->registry().get<UICanvasScalerComponent>(e);
    if (!lua_isnoneornil(L, 2)) scaler.reference_resolution.x = static_cast<float>(luaL_checknumber(L, 2));
    if (!lua_isnoneornil(L, 3)) scaler.reference_resolution.y = static_cast<float>(luaL_checknumber(L, 3));
    if (!lua_isnoneornil(L, 4)) scaler.scale_factor = static_cast<float>(luaL_checknumber(L, 4));
    if (!lua_isnoneornil(L, 5)) scaler.match_width_or_height = (lua_toboolean(L, 5) != 0);
    return 0;
}

// ============================================================
// UIAnimationComponent 绑定
// ============================================================

// ui.add_ui_animation(entity, duration, easing, [loop, ping_pong, delay])
int L_UiAddAnimation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e)) return 0;
    auto& anim = world->registry().emplace_or_replace<UIAnimationComponent>(e);
    anim.duration = static_cast<float>(luaL_optnumber(L, 2, 0.3));
    anim.easing = static_cast<int>(luaL_optinteger(L, 3, 0));
    anim.loop = lua_isnoneornil(L, 4) ? false : (lua_toboolean(L, 4) != 0);
    anim.ping_pong = lua_isnoneornil(L, 5) ? false : (lua_toboolean(L, 5) != 0);
    anim.delay = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    return 0;
}

// ui.animate_position(entity, target_x, target_y)
int L_UiAnimatePosition(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnimationComponent>(e)) return 0;
    auto& anim = world->registry().get<UIAnimationComponent>(e);
    anim.target_position.x = static_cast<float>(luaL_checknumber(L, 2));
    anim.target_position.y = static_cast<float>(luaL_checknumber(L, 3));
    anim.animate_position = true;
    anim.playing = true;
    anim.elapsed = 0.0f;
    anim.delay_remaining = anim.delay;
    return 0;
}

// ui.animate_scale(entity, target_sx, target_sy)
int L_UiAnimateScale(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnimationComponent>(e)) return 0;
    auto& anim = world->registry().get<UIAnimationComponent>(e);
    anim.target_scale.x = static_cast<float>(luaL_checknumber(L, 2));
    anim.target_scale.y = static_cast<float>(luaL_checknumber(L, 3));
    anim.animate_scale = true;
    anim.playing = true;
    anim.elapsed = 0.0f;
    anim.delay_remaining = anim.delay;
    return 0;
}

// ui.animate_alpha(entity, target_alpha)
int L_UiAnimateAlpha(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnimationComponent>(e)) return 0;
    auto& anim = world->registry().get<UIAnimationComponent>(e);
    anim.target_alpha = static_cast<float>(luaL_checknumber(L, 2));
    anim.animate_alpha = true;
    anim.playing = true;
    anim.elapsed = 0.0f;
    anim.delay_remaining = anim.delay;
    return 0;
}

// ui.animate_color(entity, r, g, b, a)
int L_UiAnimateColor(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnimationComponent>(e)) return 0;
    auto& anim = world->registry().get<UIAnimationComponent>(e);
    anim.target_color = glm::vec4(
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)),
        static_cast<float>(luaL_optnumber(L, 5, 1.0)));
    anim.animate_color = true;
    anim.playing = true;
    anim.elapsed = 0.0f;
    anim.delay_remaining = anim.delay;
    return 0;
}

// ui.stop_ui_animation(entity)
int L_UiStopAnimation(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<UIAnimationComponent>(e)) return 0;
    auto& anim = world->registry().get<UIAnimationComponent>(e);
    anim.playing = false;
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
    set_fn("set_nine_slice", L_UiSetNineSlice);
    // UIAnchorComponent
    set_fn("add_anchor", L_UiAddAnchor);
    set_fn("set_anchor_type", L_UiSetAnchorType);
    set_fn("set_anchor_offset", L_UiSetAnchorOffset);
    // UIGridLayoutComponent
    set_fn("add_grid_layout", L_UiAddGridLayout);
    set_fn("set_grid_layout", L_UiSetGridLayout);
    // UICanvasScalerComponent
    set_fn("add_canvas_scaler", L_UiAddCanvasScaler);
    set_fn("set_canvas_scaler", L_UiSetCanvasScaler);
    // UIAnimationComponent
    set_fn("add_ui_animation", L_UiAddAnimation);
    set_fn("animate_position", L_UiAnimatePosition);
    set_fn("animate_scale", L_UiAnimateScale);
    set_fn("animate_alpha", L_UiAnimateAlpha);
    set_fn("animate_color", L_UiAnimateColor);
    set_fn("stop_ui_animation", L_UiStopAnimation);
}

}

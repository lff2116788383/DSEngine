#include "engine/ecs/ui_serializer.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace dse {

namespace {

glm::vec2 ReadVec2(const rapidjson::Value& v, glm::vec2 fallback = glm::vec2(0.0f)) {
    if (!v.IsArray() || v.Size() < 2) return fallback;
    return glm::vec2(v[0].GetFloat(), v[1].GetFloat());
}

glm::vec4 ReadVec4(const rapidjson::Value& v, glm::vec4 fallback = glm::vec4(1.0f)) {
    if (!v.IsArray() || v.Size() < 4) return fallback;
    return glm::vec4(v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat(), v[3].GetFloat());
}

float ReadFloat(const rapidjson::Value& obj, const char* key, float fallback) {
    if (obj.HasMember(key) && obj[key].IsNumber()) return obj[key].GetFloat();
    return fallback;
}

int ReadInt(const rapidjson::Value& obj, const char* key, int fallback) {
    if (obj.HasMember(key) && obj[key].IsInt()) return obj[key].GetInt();
    return fallback;
}

long long ReadInt64(const rapidjson::Value& obj, const char* key, long long fallback) {
    if (obj.HasMember(key) && obj[key].IsNumber()) return obj[key].GetInt64();
    return fallback;
}

bool ReadBool(const rapidjson::Value& obj, const char* key, bool fallback) {
    if (obj.HasMember(key) && obj[key].IsBool()) return obj[key].GetBool();
    return fallback;
}

std::string ReadString(const rapidjson::Value& obj, const char* key, const std::string& fallback = "") {
    if (obj.HasMember(key) && obj[key].IsString()) return obj[key].GetString();
    return fallback;
}

void ParseUIRenderer(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& ui = reg.emplace_or_replace<UIRendererComponent>(e);
    if (c.HasMember("texture_handle")) {
        ui.texture_handle =
            dse::render::TextureHandle::from_raw(c["texture_handle"].GetUint());
    }
    if (c.HasMember("color")) ui.color = ReadVec4(c["color"]);
    if (c.HasMember("uv")) ui.uv = ReadVec4(c["uv"], glm::vec4(0, 0, 1, 1));
    ui.order = ReadInt(c, "order", 0);
    ui.visible = ReadBool(c, "visible", true);
    ui.interactable = ReadBool(c, "interactable", true);
    if (c.HasMember("position")) ui.position = ReadVec2(c["position"]);
    if (c.HasMember("size")) ui.size = ReadVec2(c["size"], glm::vec2(100.0f));
    if (c.HasMember("anchor_min")) ui.anchor_min = ReadVec2(c["anchor_min"], glm::vec2(0.5f));
    if (c.HasMember("anchor_max")) ui.anchor_max = ReadVec2(c["anchor_max"], glm::vec2(0.5f));
    if (c.HasMember("pivot")) ui.pivot = ReadVec2(c["pivot"], glm::vec2(0.5f));
    ui.nine_slice_enabled = ReadBool(c, "nine_slice_enabled", false);
    if (c.HasMember("nine_slice_border")) ui.nine_slice_border = ReadVec4(c["nine_slice_border"], glm::vec4(0));
    ui.use_sdf_shader = ReadBool(c, "use_sdf_shader", false);
}

void ParseButton(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& btn = reg.emplace_or_replace<UIButtonComponent>(e);
    if (c.HasMember("normal_color")) btn.normal_color = ReadVec4(c["normal_color"]);
    if (c.HasMember("hover_color")) btn.hover_color = ReadVec4(c["hover_color"]);
    if (c.HasMember("pressed_color")) btn.pressed_color = ReadVec4(c["pressed_color"]);
}

void ParseLabel(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& label = reg.emplace_or_replace<UILabelComponent>(e);
    label.text = ReadString(c, "text");
    label.use_localization = ReadBool(c, "use_localization", false);
    label.localization_key = ReadString(c, "localization_key");
    label.fallback_text = ReadString(c, "fallback_text");
    if (c.HasMember("localization_params") && c["localization_params"].IsObject()) {
        label.localization_params.clear();
        for (auto it = c["localization_params"].MemberBegin(); it != c["localization_params"].MemberEnd(); ++it) {
            if (it->value.IsString()) {
                label.localization_params[it->name.GetString()] = it->value.GetString();
            }
        }
    }
    label.number_value = ReadInt64(c, "number_value", 0);
    label.numeric_mode = ReadBool(c, "numeric_mode", false);
    label.font_id = ReadString(c, "font_id");
    label.font_size = ReadFloat(c, "font_size", 32.0f);
    label.use_sdf = ReadBool(c, "use_sdf", true);
    if (c.HasMember("color")) label.color = ReadVec4(c["color"]);
    if (c.HasMember("glyph_size")) label.glyph_size = ReadVec2(c["glyph_size"], glm::vec2(16.0f));
    if (c.HasMember("offset")) label.offset = ReadVec2(c["offset"]);
    label.spacing = ReadFloat(c, "spacing", 0.0f);
    label.atlas_cols = ReadInt(c, "atlas_cols", 16);
    label.atlas_rows = ReadInt(c, "atlas_rows", 6);
    label.ascii_start = ReadInt(c, "ascii_start", 32);
    label.max_width = ReadFloat(c, "max_width", 0.0f);
    label.text_align = ReadInt(c, "text_align", 0);
    label.overflow_mode = ReadInt(c, "overflow_mode", 0);
    label.max_lines = ReadInt(c, "max_lines", 0);
    label.line_spacing_extra = ReadFloat(c, "line_spacing_extra", 0.0f);
    if (c.HasMember("font_texture_handle")) {
        label.font_texture_handle =
            dse::render::TextureHandle::from_raw(c["font_texture_handle"].GetUint());
    }
    label.dirty = true;
}

void ParsePanel(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& panel = reg.emplace_or_replace<UIPanelComponent>(e);
    panel.blocks_input = ReadBool(c, "blocks_input", false);
}

void ParseMask(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& mask = reg.emplace_or_replace<UIMaskComponent>(e);
    mask.enabled = ReadBool(c, "enabled", true);
    if (c.HasMember("size")) mask.size = ReadVec2(c["size"]);
    if (c.HasMember("offset")) mask.offset = ReadVec2(c["offset"]);
    mask.block_outside_input = ReadBool(c, "block_outside_input", true);
}

void ParseGridLayout(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& grid = reg.emplace_or_replace<UIGridLayoutComponent>(e);
    grid.columns = ReadInt(c, "columns", 1);
    grid.rows = ReadInt(c, "rows", 0);
    if (c.HasMember("cell_size")) grid.cell_size = ReadVec2(c["cell_size"], glm::vec2(100.0f));
    if (c.HasMember("spacing")) grid.spacing = ReadVec2(c["spacing"], glm::vec2(10.0f));
    grid.alignment = ReadInt(c, "alignment", 0);
}

void ParseBoxLayout(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& box = reg.emplace_or_replace<UIBoxLayoutComponent>(e);
    box.vertical = ReadBool(c, "vertical", false);
    box.spacing = ReadFloat(c, "spacing", 0.0f);
    if (c.HasMember("padding")) box.padding = ReadVec2(c["padding"]);
    box.align_main = ReadInt(c, "align_main", 0);
    box.align_cross = ReadInt(c, "align_cross", 0);
    box.reverse = ReadBool(c, "reverse", false);
}

void ParseCanvasScaler(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& scaler = reg.emplace_or_replace<UICanvasScalerComponent>(e);
    if (c.HasMember("reference_resolution")) scaler.reference_resolution = ReadVec2(c["reference_resolution"], glm::vec2(1920, 1080));
    scaler.scale_factor = ReadFloat(c, "scale_factor", 1.0f);
    scaler.match_width_or_height = ReadBool(c, "match_width_or_height", true);
    scaler.match = ReadFloat(c, "match", 0.5f);
    scaler.pixel_snap = ReadBool(c, "pixel_snap", false);
}

void ParseScrollView(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& sv = reg.emplace_or_replace<UIScrollViewComponent>(e);
    if (c.HasMember("content_size")) sv.content_size = ReadVec2(c["content_size"]);
    if (c.HasMember("viewport_size")) sv.viewport_size = ReadVec2(c["viewport_size"]);
    if (c.HasMember("scroll_offset")) sv.scroll_offset = ReadVec2(c["scroll_offset"]);
    sv.horizontal = ReadBool(c, "horizontal", false);
    sv.vertical = ReadBool(c, "vertical", true);
    sv.elastic = ReadBool(c, "elastic", true);
    sv.elasticity = ReadFloat(c, "elasticity", 0.1f);
    sv.inertia = ReadBool(c, "inertia", true);
    sv.deceleration_rate = ReadFloat(c, "deceleration_rate", 0.135f);
    sv.show_scrollbar = ReadBool(c, "show_scrollbar", true);
    sv.scrollbar_width = ReadFloat(c, "scrollbar_width", 6.0f);
    if (c.HasMember("scrollbar_color")) sv.scrollbar_color = ReadVec4(c["scrollbar_color"], glm::vec4(0.5f, 0.5f, 0.5f, 0.6f));
}

void ParseSlider(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& slider = reg.emplace_or_replace<UISliderComponent>(e);
    slider.value = ReadFloat(c, "value", 0.0f);
    slider.min_value = ReadFloat(c, "min_value", 0.0f);
    slider.max_value = ReadFloat(c, "max_value", 1.0f);
    slider.whole_numbers = ReadBool(c, "whole_numbers", false);
    slider.vertical = ReadBool(c, "vertical", false);
    slider.handle_size = ReadFloat(c, "handle_size", 20.0f);
    if (c.HasMember("track_color")) slider.track_color = ReadVec4(c["track_color"]);
    if (c.HasMember("fill_color")) slider.fill_color = ReadVec4(c["fill_color"]);
    if (c.HasMember("handle_color")) slider.handle_color = ReadVec4(c["handle_color"]);
}

void ParseToggle(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& toggle = reg.emplace_or_replace<UIToggleComponent>(e);
    toggle.is_on = ReadBool(c, "is_on", false);
    toggle.group = ReadInt(c, "group", -1);
    if (c.HasMember("on_color")) toggle.on_color = ReadVec4(c["on_color"]);
    if (c.HasMember("off_color")) toggle.off_color = ReadVec4(c["off_color"]);
    toggle.transition_duration = ReadFloat(c, "transition_duration", 0.15f);
}

void ParseProgressBar(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& bar = reg.emplace_or_replace<UIProgressBarComponent>(e);
    bar.value = ReadFloat(c, "value", 0.0f);
    bar.max_value = ReadFloat(c, "max_value", 1.0f);
    bar.right_to_left = ReadBool(c, "right_to_left", false);
    bar.vertical = ReadBool(c, "vertical", false);
    if (c.HasMember("background_color")) bar.background_color = ReadVec4(c["background_color"]);
    if (c.HasMember("fill_color")) bar.fill_color = ReadVec4(c["fill_color"]);
}

void ParseTextInput(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& input = reg.emplace_or_replace<UITextInputComponent>(e);
    input.text = ReadString(c, "text");
    input.placeholder = ReadString(c, "placeholder");
    input.cursor_position = ReadInt(c, "cursor_position", 0);
    input.selection_start = ReadInt(c, "selection_start", -1);
    input.selection_end = ReadInt(c, "selection_end", -1);
    input.max_length = ReadInt(c, "max_length", 0);
    input.is_focused = ReadBool(c, "is_focused", false);
    input.is_password = ReadBool(c, "is_password", false);
    input.multiline = ReadBool(c, "multiline", false);
    input.read_only = ReadBool(c, "read_only", false);
    input.submit_on_enter = ReadBool(c, "submit_on_enter", true);
    if (c.HasMember("text_color")) input.text_color = ReadVec4(c["text_color"]);
    if (c.HasMember("placeholder_color")) input.placeholder_color = ReadVec4(c["placeholder_color"]);
    if (c.HasMember("cursor_color")) input.cursor_color = ReadVec4(c["cursor_color"]);
    if (c.HasMember("selection_color")) input.selection_color = ReadVec4(c["selection_color"]);
    input.cursor_blink_rate = ReadFloat(c, "cursor_blink_rate", 0.53f);
    if (c.HasMember("font_texture_handle") && c["font_texture_handle"].IsUint()) {
        input.font_texture_handle =
            dse::render::TextureHandle::from_raw(c["font_texture_handle"].GetUint());
    }
}

void ParseDropdown(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& dd = reg.emplace_or_replace<UIDropdownComponent>(e);
    dd.selected_index = ReadInt(c, "selected_index", -1);
    dd.item_height = ReadFloat(c, "item_height", 40.0f);
    dd.max_visible_items = ReadInt(c, "max_visible_items", 5);
    if (c.HasMember("normal_color")) dd.normal_color = ReadVec4(c["normal_color"]);
    if (c.HasMember("hover_color")) dd.hover_color = ReadVec4(c["hover_color"]);
    if (c.HasMember("selected_color")) dd.selected_color = ReadVec4(c["selected_color"]);
    if (c.HasMember("text_color")) dd.text_color = ReadVec4(c["text_color"]);
    if (c.HasMember("options") && c["options"].IsArray()) {
        for (const auto& opt : c["options"].GetArray()) {
            if (!opt.IsObject()) continue;
            UIDropdownOption o;
            o.text = ReadString(opt, "text");
            o.value = ReadString(opt, "value", o.text);
            dd.options.push_back(std::move(o));
        }
    }
}

void ParseFilledImage(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& fi = reg.emplace_or_replace<UIFilledImageComponent>(e);
    fi.fill_amount = ReadFloat(c, "fill_amount", 1.0f);
    int method = ReadInt(c, "fill_method", 0);
    fi.fill_method = (method >= 0 && method <= 4) ? static_cast<UIFillMethod>(method) : UIFillMethod::Horizontal;
    int origin = ReadInt(c, "fill_origin", 0);
    fi.fill_origin = (origin >= 0 && origin <= 4) ? static_cast<UIFillOrigin>(origin) : UIFillOrigin::Left;
    fi.clockwise = ReadBool(c, "clockwise", true);
}

void ParseFocusNavigable(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& fn = reg.emplace_or_replace<UIFocusNavigableComponent>(e);
    fn.tab_index = ReadInt(c, "tab_index", 0);
    if (c.HasMember("focus_tint")) fn.focus_tint = ReadVec4(c["focus_tint"]);
}

void ParseEventPropagation(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& ep = reg.emplace_or_replace<UIEventPropagationComponent>(e);
    ep.bubbles_click = ReadBool(c, "bubbles_click", true);
    ep.bubbles_hover = ReadBool(c, "bubbles_hover", false);
}

void ParseVisualEffect(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& vfx = reg.emplace_or_replace<UIVisualEffectComponent>(e);
    vfx.corner_radius = ReadFloat(c, "corner_radius", 0.0f);
    if (c.HasMember("gradient_color_start")) vfx.gradient_color_start = ReadVec4(c["gradient_color_start"]);
    if (c.HasMember("gradient_color_end")) vfx.gradient_color_end = ReadVec4(c["gradient_color_end"]);
    int dir = ReadInt(c, "gradient_direction", 1);
    vfx.gradient_direction = (dir >= 0 && dir <= 2) ? static_cast<UIGradientDirection>(dir) : UIGradientDirection::Vertical;
    vfx.blur_radius = ReadFloat(c, "blur_radius", 0.0f);
    vfx.blur_intensity = ReadFloat(c, "blur_intensity", 1.0f);
}

void ParseVirtualScroll(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& vs = reg.emplace_or_replace<UIVirtualScrollComponent>(e);
    vs.total_item_count = ReadInt(c, "total_item_count", 0);
    vs.item_height = ReadFloat(c, "item_height", 50.0f);
}

void ParseAnchor(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& anchor = reg.emplace_or_replace<UIAnchorComponent>(e);
    anchor.anchor = ReadInt(c, "anchor", 5);
    if (c.HasMember("offset")) anchor.offset = ReadVec2(c["offset"]);
}

void ParseRichText(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& rt = reg.emplace_or_replace<UIRichTextComponent>(e);
    rt.text = ReadString(c, "text");
    if (c.HasMember("default_color")) rt.default_color = ReadVec4(c["default_color"]);
    rt.enable_shadow = ReadBool(c, "enable_shadow", false);
    if (c.HasMember("shadow_offset")) rt.shadow_offset = ReadVec2(c["shadow_offset"], glm::vec2(1.0f, -1.0f));
    if (c.HasMember("shadow_color")) rt.shadow_color = ReadVec4(c["shadow_color"], glm::vec4(0, 0, 0, 0.75f));
    rt.enable_outline = ReadBool(c, "enable_outline", false);
    if (c.HasMember("outline_color")) rt.outline_color = ReadVec4(c["outline_color"], glm::vec4(0, 0, 0, 1));
    rt.outline_width = ReadFloat(c, "outline_width", 1.0f);
    rt.dirty = true;
}

void ParseJoystick(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& joy = reg.emplace_or_replace<UIJoystickComponent>(e);
    if (c.HasMember("direction")) joy.direction = ReadVec2(c["direction"]);
    joy.max_radius = ReadFloat(c, "max_radius", 64.0f);
    joy.follow_pointer = ReadBool(c, "follow_pointer", true);
    joy.reset_on_release = ReadBool(c, "reset_on_release", true);
    joy.is_dragging = ReadBool(c, "is_dragging", false);
    if (c.HasMember("drag_anchor")) joy.drag_anchor = ReadVec2(c["drag_anchor"]);
}

void ParseContentSizeFitter(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& fitter = reg.emplace_or_replace<UIContentSizeFitterComponent>(e);
    fitter.fit_width = ReadInt(c, "fit_width", 0);
    fitter.fit_height = ReadInt(c, "fit_height", 0);
    if (c.HasMember("min_size")) fitter.min_size = ReadVec2(c["min_size"]);
    if (c.HasMember("max_size")) fitter.max_size = ReadVec2(c["max_size"]);
}

void ParseAnimation(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& anim = reg.emplace_or_replace<UIAnimationComponent>(e);
    if (c.HasMember("target_position")) anim.target_position = ReadVec2(c["target_position"]);
    if (c.HasMember("target_scale")) anim.target_scale = ReadVec2(c["target_scale"], glm::vec2(1.0f));
    anim.target_alpha = ReadFloat(c, "target_alpha", 1.0f);
    if (c.HasMember("target_color")) anim.target_color = ReadVec4(c["target_color"]);
    anim.animate_position = ReadBool(c, "animate_position", false);
    anim.animate_scale = ReadBool(c, "animate_scale", false);
    anim.animate_alpha = ReadBool(c, "animate_alpha", false);
    anim.animate_color = ReadBool(c, "animate_color", false);
    anim.duration = ReadFloat(c, "duration", 0.3f);
    anim.elapsed = ReadFloat(c, "elapsed", 0.0f);
    anim.delay = ReadFloat(c, "delay", 0.0f);
    anim.loop = ReadBool(c, "loop", false);
    anim.ping_pong = ReadBool(c, "ping_pong", false);
    anim.playing = ReadBool(c, "playing", false);
    anim.reverse = ReadBool(c, "reverse", false);
    anim.easing = ReadInt(c, "easing", 0);
}

void ParseEntityComponents(entt::registry& reg, entt::entity e, const rapidjson::Value& components) {
    if (components.HasMember("UIRenderer")) ParseUIRenderer(reg, e, components["UIRenderer"]);
    if (components.HasMember("UIButton")) ParseButton(reg, e, components["UIButton"]);
    if (components.HasMember("UILabel")) ParseLabel(reg, e, components["UILabel"]);
    if (components.HasMember("UIPanel")) ParsePanel(reg, e, components["UIPanel"]);
    if (components.HasMember("UIMask")) ParseMask(reg, e, components["UIMask"]);
    if (components.HasMember("UIGridLayout")) ParseGridLayout(reg, e, components["UIGridLayout"]);
    if (components.HasMember("UIBoxLayout")) ParseBoxLayout(reg, e, components["UIBoxLayout"]);
    if (components.HasMember("UICanvasScaler")) ParseCanvasScaler(reg, e, components["UICanvasScaler"]);
    if (components.HasMember("UIScrollView")) ParseScrollView(reg, e, components["UIScrollView"]);
    if (components.HasMember("UISlider")) ParseSlider(reg, e, components["UISlider"]);
    if (components.HasMember("UIToggle")) ParseToggle(reg, e, components["UIToggle"]);
    if (components.HasMember("UIProgressBar")) ParseProgressBar(reg, e, components["UIProgressBar"]);
    if (components.HasMember("UITextInput")) ParseTextInput(reg, e, components["UITextInput"]);
    if (components.HasMember("UIDropdown")) ParseDropdown(reg, e, components["UIDropdown"]);
    if (components.HasMember("UIFilledImage")) ParseFilledImage(reg, e, components["UIFilledImage"]);
    if (components.HasMember("UIFocusNavigable")) ParseFocusNavigable(reg, e, components["UIFocusNavigable"]);
    if (components.HasMember("UIEventPropagation")) ParseEventPropagation(reg, e, components["UIEventPropagation"]);
    if (components.HasMember("UIVisualEffect")) ParseVisualEffect(reg, e, components["UIVisualEffect"]);
    if (components.HasMember("UIVirtualScroll")) ParseVirtualScroll(reg, e, components["UIVirtualScroll"]);
    if (components.HasMember("UIAnchor")) ParseAnchor(reg, e, components["UIAnchor"]);
    if (components.HasMember("UIAnimation")) ParseAnimation(reg, e, components["UIAnimation"]);
    if (components.HasMember("UIRichText")) ParseRichText(reg, e, components["UIRichText"]);
    if (components.HasMember("UIJoystick")) ParseJoystick(reg, e, components["UIJoystick"]);
    if (components.HasMember("UIContentSizeFitter")) ParseContentSizeFitter(reg, e, components["UIContentSizeFitter"]);
}

void ParseEntityTree(entt::registry& reg, const rapidjson::Value& node,
                     std::unordered_map<uint32_t, entt::entity>& id_map,
                     std::vector<std::pair<entt::entity, uint32_t>>& pending_parents,
                     std::vector<entt::entity>& out_entities,
                     uint32_t implicit_parent_id = 0, bool has_implicit_parent = false) {
    if (!node.IsObject()) return;

    entt::entity e = reg.create();
    out_entities.push_back(e);

    if (node.HasMember("id") && node["id"].IsUint()) {
        id_map[node["id"].GetUint()] = e;
    }

    if (node.HasMember("parent") && node["parent"].IsUint()) {
        pending_parents.emplace_back(e, node["parent"].GetUint());
    } else if (has_implicit_parent) {
        pending_parents.emplace_back(e, implicit_parent_id);
    }

    if (node.HasMember("components") && node["components"].IsObject()) {
        ParseEntityComponents(reg, e, node["components"]);
    }

    if (node.HasMember("children") && node["children"].IsArray()) {
        uint32_t self_id = 0;
        if (node.HasMember("id") && node["id"].IsUint()) {
            self_id = node["id"].GetUint();
        } else {
            self_id = static_cast<uint32_t>(e);
            id_map[self_id] = e;
        }
        for (const auto& child : node["children"].GetArray()) {
            if (!child.IsObject()) continue;
            ParseEntityTree(reg, child, id_map, pending_parents, out_entities, self_id, true);
        }
    }
}

} // anonymous namespace

std::vector<entt::entity> UISerializer::LoadFromJson(entt::registry& registry, const std::string& json_str) {
    std::vector<entt::entity> entities;

    rapidjson::Document doc;
    if (doc.Parse(json_str.c_str()).HasParseError()) {
        DEBUG_LOG_ERROR("UISerializer::LoadFromJson: JSON parse error");
        return entities;
    }

    if (!doc.IsObject()) {
        DEBUG_LOG_ERROR("UISerializer::LoadFromJson: root is not an object");
        return entities;
    }

    std::unordered_map<uint32_t, entt::entity> id_map;
    std::vector<std::pair<entt::entity, uint32_t>> pending_parents;

    if (doc.HasMember("entities") && doc["entities"].IsArray()) {
        for (const auto& node : doc["entities"].GetArray()) {
            ParseEntityTree(registry, node, id_map, pending_parents, entities);
        }
    } else if (doc.HasMember("components")) {
        ParseEntityTree(registry, doc, id_map, pending_parents, entities);
    }

    for (auto& [entity, parent_id] : pending_parents) {
        auto it = id_map.find(parent_id);
        if (it != id_map.end() && registry.valid(it->second)) {
            registry.emplace_or_replace<ParentComponent>(entity).parent = it->second;
        }
    }

    return entities;
}

std::vector<entt::entity> UISerializer::LoadFromFile(entt::registry& registry, const std::string& file_path) {
    std::ifstream in(file_path);
    if (!in.is_open()) {
        DEBUG_LOG_ERROR("UISerializer::LoadFromFile: cannot open {}", file_path);
        return {};
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return LoadFromJson(registry, buffer.str());
}

// ============================================================
// Save（与 LoadFromJson 对称的序列化）
// ============================================================

namespace {

using JsonAlloc = rapidjson::Document::AllocatorType;

void PutVec2(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, const glm::vec2& v) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(v.x, alloc).PushBack(v.y, alloc);
    obj.AddMember(rapidjson::Value(key, alloc), arr.Move(), alloc);
}

void PutVec4(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, const glm::vec4& v) {
    rapidjson::Value arr(rapidjson::kArrayType);
    arr.PushBack(v.x, alloc).PushBack(v.y, alloc).PushBack(v.z, alloc).PushBack(v.w, alloc);
    obj.AddMember(rapidjson::Value(key, alloc), arr.Move(), alloc);
}

void PutFloat(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, float v) {
    obj.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(v).Move(), alloc);
}

void PutInt(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, int v) {
    obj.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(v).Move(), alloc);
}

void PutInt64(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, long long v) {
    obj.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(v).Move(), alloc);
}

void PutBool(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, bool v) {
    obj.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(v).Move(), alloc);
}

void PutString(rapidjson::Value& obj, JsonAlloc& alloc, const char* key, const std::string& v) {
    obj.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(v.c_str(), alloc).Move(), alloc);
}

void PutTextureHandle(rapidjson::Value& obj, JsonAlloc& alloc, const char* key,
                      const dse::render::TextureRef& handle) {
    if (handle.raw() != 0) {
        obj.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(handle.raw()).Move(), alloc);
    }
}

void WriteUIRenderer(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& ui = reg.get<UIRendererComponent>(e);
    PutVec4(c, alloc, "color", ui.color);
    PutVec4(c, alloc, "uv", ui.uv);
    PutInt(c, alloc, "order", ui.order);
    PutBool(c, alloc, "visible", ui.visible);
    PutBool(c, alloc, "interactable", ui.interactable);
    PutVec2(c, alloc, "position", ui.position);
    PutVec2(c, alloc, "size", ui.size);
    PutVec2(c, alloc, "anchor_min", ui.anchor_min);
    PutVec2(c, alloc, "anchor_max", ui.anchor_max);
    PutVec2(c, alloc, "pivot", ui.pivot);
    PutBool(c, alloc, "nine_slice_enabled", ui.nine_slice_enabled);
    PutVec4(c, alloc, "nine_slice_border", ui.nine_slice_border);
    PutBool(c, alloc, "use_sdf_shader", ui.use_sdf_shader);
}

void WriteButton(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& btn = reg.get<UIButtonComponent>(e);
    PutVec4(c, alloc, "normal_color", btn.normal_color);
    PutVec4(c, alloc, "hover_color", btn.hover_color);
    PutVec4(c, alloc, "pressed_color", btn.pressed_color);
}

void WriteLabel(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& label = reg.get<UILabelComponent>(e);
    PutString(c, alloc, "text", label.text);
    PutBool(c, alloc, "use_localization", label.use_localization);
    PutString(c, alloc, "localization_key", label.localization_key);
    PutString(c, alloc, "fallback_text", label.fallback_text);
    if (!label.localization_params.empty()) {
        rapidjson::Value params(rapidjson::kObjectType);
        for (const auto& kv : label.localization_params) {
            params.AddMember(rapidjson::Value(kv.first.c_str(), alloc),
                             rapidjson::Value(kv.second.c_str(), alloc), alloc);
        }
        c.AddMember(rapidjson::Value("localization_params", alloc), params.Move(), alloc);
    }
    PutInt64(c, alloc, "number_value", label.number_value);
    PutBool(c, alloc, "numeric_mode", label.numeric_mode);
    PutString(c, alloc, "font_id", label.font_id);
    PutFloat(c, alloc, "font_size", label.font_size);
    PutBool(c, alloc, "use_sdf", label.use_sdf);
    PutVec4(c, alloc, "color", label.color);
    PutVec2(c, alloc, "glyph_size", label.glyph_size);
    PutVec2(c, alloc, "offset", label.offset);
    PutFloat(c, alloc, "spacing", label.spacing);
    PutInt(c, alloc, "atlas_cols", label.atlas_cols);
    PutInt(c, alloc, "atlas_rows", label.atlas_rows);
    PutInt(c, alloc, "ascii_start", label.ascii_start);
    PutTextureHandle(c, alloc, "font_texture_handle", label.font_texture_handle);
    PutFloat(c, alloc, "max_width", label.max_width);
    PutInt(c, alloc, "text_align", label.text_align);
    PutInt(c, alloc, "overflow_mode", label.overflow_mode);
    PutInt(c, alloc, "max_lines", label.max_lines);
    PutFloat(c, alloc, "line_spacing_extra", label.line_spacing_extra);
}

void WritePanel(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    PutBool(c, alloc, "blocks_input", reg.get<UIPanelComponent>(e).blocks_input);
}

void WriteMask(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& mask = reg.get<UIMaskComponent>(e);
    PutBool(c, alloc, "enabled", mask.enabled);
    PutVec2(c, alloc, "size", mask.size);
    PutVec2(c, alloc, "offset", mask.offset);
    PutBool(c, alloc, "block_outside_input", mask.block_outside_input);
}

void WriteGridLayout(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& grid = reg.get<UIGridLayoutComponent>(e);
    PutInt(c, alloc, "columns", grid.columns);
    PutInt(c, alloc, "rows", grid.rows);
    PutVec2(c, alloc, "cell_size", grid.cell_size);
    PutVec2(c, alloc, "spacing", grid.spacing);
    PutInt(c, alloc, "alignment", grid.alignment);
}

void WriteBoxLayout(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& box = reg.get<UIBoxLayoutComponent>(e);
    PutBool(c, alloc, "vertical", box.vertical);
    PutFloat(c, alloc, "spacing", box.spacing);
    PutVec2(c, alloc, "padding", box.padding);
    PutInt(c, alloc, "align_main", box.align_main);
    PutInt(c, alloc, "align_cross", box.align_cross);
    PutBool(c, alloc, "reverse", box.reverse);
}

void WriteCanvasScaler(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& scaler = reg.get<UICanvasScalerComponent>(e);
    PutVec2(c, alloc, "reference_resolution", scaler.reference_resolution);
    PutFloat(c, alloc, "scale_factor", scaler.scale_factor);
    PutBool(c, alloc, "match_width_or_height", scaler.match_width_or_height);
    PutFloat(c, alloc, "match", scaler.match);
    PutBool(c, alloc, "pixel_snap", scaler.pixel_snap);
}

void WriteScrollView(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& sv = reg.get<UIScrollViewComponent>(e);
    PutVec2(c, alloc, "content_size", sv.content_size);
    PutVec2(c, alloc, "viewport_size", sv.viewport_size);
    PutVec2(c, alloc, "scroll_offset", sv.scroll_offset);
    PutBool(c, alloc, "horizontal", sv.horizontal);
    PutBool(c, alloc, "vertical", sv.vertical);
    PutBool(c, alloc, "elastic", sv.elastic);
    PutFloat(c, alloc, "elasticity", sv.elasticity);
    PutBool(c, alloc, "inertia", sv.inertia);
    PutFloat(c, alloc, "deceleration_rate", sv.deceleration_rate);
    PutBool(c, alloc, "show_scrollbar", sv.show_scrollbar);
    PutFloat(c, alloc, "scrollbar_width", sv.scrollbar_width);
    PutVec4(c, alloc, "scrollbar_color", sv.scrollbar_color);
}

void WriteSlider(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& slider = reg.get<UISliderComponent>(e);
    PutFloat(c, alloc, "value", slider.value);
    PutFloat(c, alloc, "min_value", slider.min_value);
    PutFloat(c, alloc, "max_value", slider.max_value);
    PutBool(c, alloc, "whole_numbers", slider.whole_numbers);
    PutBool(c, alloc, "vertical", slider.vertical);
    PutVec4(c, alloc, "track_color", slider.track_color);
    PutVec4(c, alloc, "fill_color", slider.fill_color);
    PutVec4(c, alloc, "handle_color", slider.handle_color);
    PutFloat(c, alloc, "handle_size", slider.handle_size);
}

void WriteToggle(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& toggle = reg.get<UIToggleComponent>(e);
    PutBool(c, alloc, "is_on", toggle.is_on);
    PutInt(c, alloc, "group", toggle.group);
    PutVec4(c, alloc, "on_color", toggle.on_color);
    PutVec4(c, alloc, "off_color", toggle.off_color);
    PutFloat(c, alloc, "transition_duration", toggle.transition_duration);
}

void WriteProgressBar(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& bar = reg.get<UIProgressBarComponent>(e);
    PutFloat(c, alloc, "value", bar.value);
    PutFloat(c, alloc, "max_value", bar.max_value);
    PutBool(c, alloc, "right_to_left", bar.right_to_left);
    PutBool(c, alloc, "vertical", bar.vertical);
    PutVec4(c, alloc, "background_color", bar.background_color);
    PutVec4(c, alloc, "fill_color", bar.fill_color);
}

void WriteTextInput(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& input = reg.get<UITextInputComponent>(e);
    PutString(c, alloc, "text", input.text);
    PutString(c, alloc, "placeholder", input.placeholder);
    PutInt(c, alloc, "cursor_position", input.cursor_position);
    PutInt(c, alloc, "selection_start", input.selection_start);
    PutInt(c, alloc, "selection_end", input.selection_end);
    PutInt(c, alloc, "max_length", input.max_length);
    PutBool(c, alloc, "is_focused", input.is_focused);
    PutBool(c, alloc, "is_password", input.is_password);
    PutBool(c, alloc, "multiline", input.multiline);
    PutBool(c, alloc, "read_only", input.read_only);
    PutBool(c, alloc, "submit_on_enter", input.submit_on_enter);
    PutVec4(c, alloc, "text_color", input.text_color);
    PutVec4(c, alloc, "placeholder_color", input.placeholder_color);
    PutVec4(c, alloc, "cursor_color", input.cursor_color);
    PutVec4(c, alloc, "selection_color", input.selection_color);
    PutFloat(c, alloc, "cursor_blink_rate", input.cursor_blink_rate);
    PutTextureHandle(c, alloc, "font_texture_handle", input.font_texture_handle);
}

void WriteDropdown(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& dd = reg.get<UIDropdownComponent>(e);
    PutInt(c, alloc, "selected_index", dd.selected_index);
    PutFloat(c, alloc, "item_height", dd.item_height);
    PutInt(c, alloc, "max_visible_items", dd.max_visible_items);
    PutVec4(c, alloc, "normal_color", dd.normal_color);
    PutVec4(c, alloc, "hover_color", dd.hover_color);
    PutVec4(c, alloc, "selected_color", dd.selected_color);
    PutVec4(c, alloc, "text_color", dd.text_color);
    if (!dd.options.empty()) {
        rapidjson::Value options(rapidjson::kArrayType);
        for (const auto& opt : dd.options) {
            rapidjson::Value o(rapidjson::kObjectType);
            PutString(o, alloc, "text", opt.text);
            PutString(o, alloc, "value", opt.value);
            options.PushBack(o.Move(), alloc);
        }
        c.AddMember(rapidjson::Value("options", alloc), options.Move(), alloc);
    }
}

void WriteFilledImage(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& fi = reg.get<UIFilledImageComponent>(e);
    PutFloat(c, alloc, "fill_amount", fi.fill_amount);
    PutInt(c, alloc, "fill_method", static_cast<int>(fi.fill_method));
    PutInt(c, alloc, "fill_origin", static_cast<int>(fi.fill_origin));
    PutBool(c, alloc, "clockwise", fi.clockwise);
}

void WriteFocusNavigable(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& fn = reg.get<UIFocusNavigableComponent>(e);
    PutInt(c, alloc, "tab_index", fn.tab_index);
    PutVec4(c, alloc, "focus_tint", fn.focus_tint);
}

void WriteEventPropagation(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& ep = reg.get<UIEventPropagationComponent>(e);
    PutBool(c, alloc, "bubbles_click", ep.bubbles_click);
    PutBool(c, alloc, "bubbles_hover", ep.bubbles_hover);
}

void WriteVisualEffect(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& vfx = reg.get<UIVisualEffectComponent>(e);
    PutFloat(c, alloc, "corner_radius", vfx.corner_radius);
    PutVec4(c, alloc, "gradient_color_start", vfx.gradient_color_start);
    PutVec4(c, alloc, "gradient_color_end", vfx.gradient_color_end);
    PutInt(c, alloc, "gradient_direction", static_cast<int>(vfx.gradient_direction));
    PutFloat(c, alloc, "blur_radius", vfx.blur_radius);
    PutFloat(c, alloc, "blur_intensity", vfx.blur_intensity);
}

void WriteVirtualScroll(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& vs = reg.get<UIVirtualScrollComponent>(e);
    PutInt(c, alloc, "total_item_count", vs.total_item_count);
    PutFloat(c, alloc, "item_height", vs.item_height);
}

void WriteAnchor(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& anchor = reg.get<UIAnchorComponent>(e);
    PutInt(c, alloc, "anchor", anchor.anchor);
    PutVec2(c, alloc, "offset", anchor.offset);
}

void WriteAnimation(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& anim = reg.get<UIAnimationComponent>(e);
    PutVec2(c, alloc, "target_position", anim.target_position);
    PutVec2(c, alloc, "target_scale", anim.target_scale);
    PutFloat(c, alloc, "target_alpha", anim.target_alpha);
    PutVec4(c, alloc, "target_color", anim.target_color);
    PutBool(c, alloc, "animate_position", anim.animate_position);
    PutBool(c, alloc, "animate_scale", anim.animate_scale);
    PutBool(c, alloc, "animate_alpha", anim.animate_alpha);
    PutBool(c, alloc, "animate_color", anim.animate_color);
    PutFloat(c, alloc, "duration", anim.duration);
    PutFloat(c, alloc, "elapsed", anim.elapsed);
    PutFloat(c, alloc, "delay", anim.delay);
    PutBool(c, alloc, "loop", anim.loop);
    PutBool(c, alloc, "ping_pong", anim.ping_pong);
    PutBool(c, alloc, "playing", anim.playing);
    PutBool(c, alloc, "reverse", anim.reverse);
    PutInt(c, alloc, "easing", anim.easing);
}

void WriteRichText(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& rt = reg.get<UIRichTextComponent>(e);
    PutString(c, alloc, "text", rt.text);
    PutVec4(c, alloc, "default_color", rt.default_color);
    PutBool(c, alloc, "enable_shadow", rt.enable_shadow);
    PutVec2(c, alloc, "shadow_offset", rt.shadow_offset);
    PutVec4(c, alloc, "shadow_color", rt.shadow_color);
    PutBool(c, alloc, "enable_outline", rt.enable_outline);
    PutVec4(c, alloc, "outline_color", rt.outline_color);
    PutFloat(c, alloc, "outline_width", rt.outline_width);
}

void WriteJoystick(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& joy = reg.get<UIJoystickComponent>(e);
    PutVec2(c, alloc, "direction", joy.direction);
    PutFloat(c, alloc, "max_radius", joy.max_radius);
    PutBool(c, alloc, "follow_pointer", joy.follow_pointer);
    PutBool(c, alloc, "reset_on_release", joy.reset_on_release);
    PutVec2(c, alloc, "drag_anchor", joy.drag_anchor);
}

void WriteContentSizeFitter(entt::registry& reg, entt::entity e, rapidjson::Value& c, JsonAlloc& alloc) {
    const auto& fitter = reg.get<UIContentSizeFitterComponent>(e);
    PutInt(c, alloc, "fit_width", fitter.fit_width);
    PutInt(c, alloc, "fit_height", fitter.fit_height);
    PutVec2(c, alloc, "min_size", fitter.min_size);
    PutVec2(c, alloc, "max_size", fitter.max_size);
}

void WriteEntityComponents(entt::registry& reg, entt::entity e,
                           rapidjson::Value& comps, JsonAlloc& alloc) {
    // rapidjson 的 operator[] 不会自动创建成员：先 AddMember 空对象再填充。
    #define DSE_UI_ADD_WRITER(Tag, Key, Fn)                           \
        if (reg.all_of<Tag>(e)) {                                     \
            comps.AddMember(rapidjson::Value(#Key, alloc),            \
                            rapidjson::Value(rapidjson::kObjectType).Move(), alloc); \
            Fn(reg, e, comps[#Key], alloc);                            \
        }

    DSE_UI_ADD_WRITER(UIRendererComponent, UIRenderer, WriteUIRenderer);
    DSE_UI_ADD_WRITER(UIButtonComponent, UIButton, WriteButton);
    DSE_UI_ADD_WRITER(UILabelComponent, UILabel, WriteLabel);
    DSE_UI_ADD_WRITER(UIPanelComponent, UIPanel, WritePanel);
    DSE_UI_ADD_WRITER(UIMaskComponent, UIMask, WriteMask);
    DSE_UI_ADD_WRITER(UIGridLayoutComponent, UIGridLayout, WriteGridLayout);
    DSE_UI_ADD_WRITER(UIBoxLayoutComponent, UIBoxLayout, WriteBoxLayout);
    DSE_UI_ADD_WRITER(UICanvasScalerComponent, UICanvasScaler, WriteCanvasScaler);
    DSE_UI_ADD_WRITER(UIScrollViewComponent, UIScrollView, WriteScrollView);
    DSE_UI_ADD_WRITER(UISliderComponent, UISlider, WriteSlider);
    DSE_UI_ADD_WRITER(UIToggleComponent, UIToggle, WriteToggle);
    DSE_UI_ADD_WRITER(UIProgressBarComponent, UIProgressBar, WriteProgressBar);
    DSE_UI_ADD_WRITER(UITextInputComponent, UITextInput, WriteTextInput);
    DSE_UI_ADD_WRITER(UIDropdownComponent, UIDropdown, WriteDropdown);
    DSE_UI_ADD_WRITER(UIFilledImageComponent, UIFilledImage, WriteFilledImage);
    DSE_UI_ADD_WRITER(UIFocusNavigableComponent, UIFocusNavigable, WriteFocusNavigable);
    DSE_UI_ADD_WRITER(UIEventPropagationComponent, UIEventPropagation, WriteEventPropagation);
    DSE_UI_ADD_WRITER(UIVisualEffectComponent, UIVisualEffect, WriteVisualEffect);
    DSE_UI_ADD_WRITER(UIVirtualScrollComponent, UIVirtualScroll, WriteVirtualScroll);
    DSE_UI_ADD_WRITER(UIAnchorComponent, UIAnchor, WriteAnchor);
    DSE_UI_ADD_WRITER(UIAnimationComponent, UIAnimation, WriteAnimation);
    DSE_UI_ADD_WRITER(UIRichTextComponent, UIRichText, WriteRichText);
    DSE_UI_ADD_WRITER(UIJoystickComponent, UIJoystick, WriteJoystick);
    DSE_UI_ADD_WRITER(UIContentSizeFitterComponent, UIContentSizeFitter, WriteContentSizeFitter);
    #undef DSE_UI_ADD_WRITER
}

} // anonymous namespace

std::string UISerializer::SaveToJson(entt::registry& registry) const {
    // 收集所有含 UI 组件的实体并分配稳定 id（按注册顺序）。
    std::vector<entt::entity> ui_entities;
    for (auto [e] : registry.storage<entt::entity>().each()) {
        if (registry.any_of<UIRendererComponent, UIButtonComponent, UILabelComponent,
                            UIPanelComponent, UIMaskComponent, UIGridLayoutComponent,
                            UIBoxLayoutComponent, UICanvasScalerComponent, UIScrollViewComponent,
                            UISliderComponent, UIToggleComponent, UIProgressBarComponent,
                            UITextInputComponent, UIDropdownComponent, UIFilledImageComponent,
                            UIFocusNavigableComponent, UIEventPropagationComponent,
                            UIVisualEffectComponent, UIVirtualScrollComponent, UIAnchorComponent,
                            UIAnimationComponent, UIRichTextComponent, UIJoystickComponent,
                            UIContentSizeFitterComponent>(e)) {
            ui_entities.push_back(e);
        }
    }

    if (ui_entities.empty()) return "{}";

    std::unordered_map<entt::entity, uint32_t> id_of;
    for (uint32_t i = 0; i < ui_entities.size(); ++i) id_of[ui_entities[i]] = i;

    rapidjson::Document doc(rapidjson::kObjectType);
    auto& alloc = doc.GetAllocator();
    rapidjson::Value entities(rapidjson::kArrayType);

    // 递归写子树；parent 不在 UI 集合中的实体视为根。
    std::function<void(entt::entity, rapidjson::Value&)> write_node =
        [&](entt::entity e, rapidjson::Value& node) {
            node.AddMember(rapidjson::Value("id", alloc),
                           rapidjson::Value(id_of[e]).Move(), alloc);
            const auto* parent = registry.try_get<ParentComponent>(e);
            if (parent && id_of.count(parent->parent) != 0) {
                node.AddMember(rapidjson::Value("parent", alloc),
                               rapidjson::Value(id_of[parent->parent]).Move(), alloc);
            }
            rapidjson::Value comps(rapidjson::kObjectType);
            WriteEntityComponents(registry, e, comps, alloc);
            node.AddMember(rapidjson::Value("components", alloc), comps.Move(), alloc);

            rapidjson::Value children(rapidjson::kArrayType);
            for (entt::entity child : ui_entities) {
                const auto* p = registry.try_get<ParentComponent>(child);
                if (p && p->parent == e) {
                    rapidjson::Value child_node(rapidjson::kObjectType);
                    write_node(child, child_node);
                    children.PushBack(child_node.Move(), alloc);
                }
            }
            if (!children.Empty()) {
                node.AddMember(rapidjson::Value("children", alloc), children.Move(), alloc);
            }
        };

    for (entt::entity e : ui_entities) {
        const auto* parent = registry.try_get<ParentComponent>(e);
        bool has_ui_parent = parent && id_of.count(parent->parent) != 0;
        if (!has_ui_parent) {
            rapidjson::Value node(rapidjson::kObjectType);
            write_node(e, node);
            entities.PushBack(node.Move(), alloc);
        }
    }

    doc.AddMember(rapidjson::Value("version", alloc), rapidjson::Value(1).Move(), alloc);
    doc.AddMember(rapidjson::Value("entities", alloc), entities.Move(), alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    return buffer.GetString();
}

bool UISerializer::SaveToFile(entt::registry& registry, const std::string& file_path) const {
    std::ofstream out(file_path, std::ios::trunc);
    if (!out.is_open()) {
        DEBUG_LOG_ERROR("UISerializer::SaveToFile: cannot open {}", file_path);
        return false;
    }
    out << SaveToJson(registry);
    return out.good();
}

} // namespace dse

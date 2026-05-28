#include "engine/ecs/ui_serializer.h"
#include "engine/ecs/ui.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <rapidjson/document.h>
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
    if (c.HasMember("texture_handle")) ui.texture_handle = static_cast<unsigned int>(c["texture_handle"].GetUint());
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
    label.font_id = ReadString(c, "font_id");
    label.font_size = ReadFloat(c, "font_size", 32.0f);
    label.use_sdf = ReadBool(c, "use_sdf", true);
    if (c.HasMember("color")) label.color = ReadVec4(c["color"]);
    if (c.HasMember("glyph_size")) label.glyph_size = ReadVec2(c["glyph_size"], glm::vec2(16.0f));
    label.spacing = ReadFloat(c, "spacing", 0.0f);
    label.atlas_cols = ReadInt(c, "atlas_cols", 16);
    label.atlas_rows = ReadInt(c, "atlas_rows", 6);
    label.ascii_start = ReadInt(c, "ascii_start", 32);
    label.max_width = ReadFloat(c, "max_width", 0.0f);
    label.text_align = ReadInt(c, "text_align", 0);
    label.overflow_mode = ReadInt(c, "overflow_mode", 0);
    label.max_lines = ReadInt(c, "max_lines", 0);
    if (c.HasMember("font_texture_handle")) label.font_texture_handle = static_cast<unsigned int>(c["font_texture_handle"].GetUint());
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
}

void ParseScrollView(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& sv = reg.emplace_or_replace<UIScrollViewComponent>(e);
    if (c.HasMember("content_size")) sv.content_size = ReadVec2(c["content_size"]);
    if (c.HasMember("viewport_size")) sv.viewport_size = ReadVec2(c["viewport_size"]);
    sv.horizontal = ReadBool(c, "horizontal", false);
    sv.vertical = ReadBool(c, "vertical", true);
    sv.elastic = ReadBool(c, "elastic", true);
    sv.inertia = ReadBool(c, "inertia", true);
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
    input.placeholder = ReadString(c, "placeholder");
    input.max_length = ReadInt(c, "max_length", 0);
    input.is_password = ReadBool(c, "is_password", false);
    input.multiline = ReadBool(c, "multiline", false);
    input.read_only = ReadBool(c, "read_only", false);
    if (c.HasMember("text_color")) input.text_color = ReadVec4(c["text_color"]);
    if (c.HasMember("placeholder_color")) input.placeholder_color = ReadVec4(c["placeholder_color"]);
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

void ParseAnchor(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& anchor = reg.emplace_or_replace<UIAnchorComponent>(e);
    anchor.anchor = ReadInt(c, "anchor", 5);
    if (c.HasMember("offset")) anchor.offset = ReadVec2(c["offset"]);
}

void ParseAnimation(entt::registry& reg, entt::entity e, const rapidjson::Value& c) {
    auto& anim = reg.emplace_or_replace<UIAnimationComponent>(e);
    anim.duration = ReadFloat(c, "duration", 0.3f);
    anim.easing = ReadInt(c, "easing", 0);
    anim.loop = ReadBool(c, "loop", false);
    anim.ping_pong = ReadBool(c, "ping_pong", false);
    anim.delay = ReadFloat(c, "delay", 0.0f);
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
    if (components.HasMember("UIAnchor")) ParseAnchor(reg, e, components["UIAnchor"]);
    if (components.HasMember("UIAnimation")) ParseAnimation(reg, e, components["UIAnimation"]);
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

} // namespace dse

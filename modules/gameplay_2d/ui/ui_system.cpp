/**
 * @file ui_system.cpp
 * @brief 用户界面(UI)系统，管理 UI 元素的布局、事件响应和渲染
 */

#include "ui_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/base/debug.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/input/input.h"
#include "engine/render/font/font_service.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <cstdio>

namespace dse {
namespace gameplay2d {
namespace {
/**
 * @brief 解析十六进制颜色字符串（如 "ff0000" 或 "ff0000ff"）
 * @param hex 十六进制颜色字符串（不含 #）
 * @param fallback 解析失败时的回退颜色
 * @return 解析得到的 RGBA 颜色向量
 */
glm::vec4 ParseHexColor(const std::string& hex, const glm::vec4& fallback) {
    if (hex.size() != 6 && hex.size() != 8) {
        return fallback;
    }
    for (char ch : hex) {
        if (!std::isxdigit(static_cast<unsigned char>(ch))) {
            return fallback;
        }
    }
    const auto to_channel = [&](std::size_t offset) {
        return static_cast<float>(std::strtoul(hex.substr(offset, 2).c_str(), nullptr, 16)) / 255.0f;
    };
    glm::vec4 color(to_channel(0), to_channel(2), to_channel(4), 1.0f);
    if (hex.size() == 8) {
        color.a = to_channel(6);
    }
    return color;
}

float ApplyEasing(float t, int easing) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easing) {
        case 1: return t * t;                         // ease-in
        case 2: return 1.0f - (1.0f - t) * (1.0f - t); // ease-out
        case 3:
            return (t < 0.5f)
                ? (2.0f * t * t)
                : (1.0f - 2.0f * (1.0f - t) * (1.0f - t));
        case 0:
        default:
            return t;
    }
}
}

void UISystem::Update(entt::registry& registry, float dt, const glm::vec2& screen_size, const glm::vec2& mouse_pos, bool is_mouse_down) {
    SyncLabels(registry);
    UpdateAnimations(registry, dt);
    UpdateLayout(registry, screen_size);
    UpdateSliders(registry, dt, mouse_pos, is_mouse_down);
    UpdateToggles(registry, dt);
    UpdateScrollViews(registry, dt, mouse_pos, is_mouse_down);
    UpdateTextInputs(registry, dt);
    UpdateDropdowns(registry, dt, mouse_pos, is_mouse_down);
    UpdateFocusNavigation(registry);
    HandleEvents(registry, dt, mouse_pos, is_mouse_down);
    was_mouse_down_ = is_mouse_down;
}

void UISystem::UpdateAnimations(entt::registry& registry, float dt) {
    auto view = registry.view<UIAnimationComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& anim = view.get<UIAnimationComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!anim.playing) {
            continue;
        }

        float remaining_dt = std::max(0.0f, dt);
        if (anim.delay_remaining > 0.0f) {
            const float consumed = std::min(anim.delay_remaining, remaining_dt);
            anim.delay_remaining -= consumed;
            remaining_dt -= consumed;
            if (remaining_dt <= 0.0f) {
                continue;
            }
        }

        const float duration = anim.duration > 0.0f ? anim.duration : 0.0001f;
        anim.elapsed += remaining_dt;
        float normalized = std::clamp(anim.elapsed / duration, 0.0f, 1.0f);
        if (anim.reverse) {
            normalized = 1.0f - normalized;
        }
        const float eased = ApplyEasing(normalized, anim.easing);

        if (anim.animate_position) {
            ui.position = glm::mix(anim.start_position, anim.target_position, eased);
        }
        if (anim.animate_alpha) {
            ui.color.a = anim.start_alpha + (anim.target_alpha - anim.start_alpha) * eased;
        }
        if (anim.animate_color) {
            ui.color = glm::mix(anim.start_color, anim.target_color, eased);
        }
        if (anim.animate_scale) {
            const glm::vec2 scale2 = glm::mix(anim.start_scale, anim.target_scale, eased);
            ui.scale = (scale2.x + scale2.y) * 0.5f;
        }

        if (anim.elapsed < duration) {
            continue;
        }

        if (anim.ping_pong) {
            anim.reverse = !anim.reverse;
            if (anim.loop) {
                anim.elapsed = std::fmod(anim.elapsed, duration);
            } else {
                anim.elapsed = 0.0f;
                anim.playing = false;
            }
        } else if (anim.loop) {
            anim.elapsed = std::fmod(anim.elapsed, duration);
        } else {
            anim.elapsed = duration;
            anim.playing = false;
        }
    }
}

void UISystem::SyncLabels(entt::registry& registry) {
    auto view = registry.view<UILabelComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& label = view.get<UILabelComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        auto* rich = registry.try_get<UIRichTextComponent>(entity);

        if (label.use_localization && !label.localization_key.empty() && !label.numeric_mode) {
            const std::string localized_text = LocalizationSystem::GetInstance().GetTextWithParams(
                label.localization_key,
                label.localization_params,
                label.fallback_text.empty() ? label.text : label.fallback_text);
            if (label.text != localized_text) {
                label.text = localized_text;
                label.dirty = true;
            }
        }

        if (label.numeric_mode && !rich) {
            const std::string numeric_text = std::to_string(label.number_value);
            if (label.text != numeric_text) {
                label.text = numeric_text;
                label.dirty = true;
            }
        }

        if (rich && rich->dirty) {
            label.dirty = true;
        }
        if (!label.dirty) {
            continue;
        }

        for (auto glyph_entity : label.runtime_glyph_entities) {
            if (registry.valid(glyph_entity)) {
                registry.destroy(glyph_entity);
            }
        }
        label.runtime_glyph_entities.clear();

        // --- TTF/SDF 字体路径 ---
        if (!label.font_id.empty()) {
            auto* font_service = dse::core::ServiceLocator::Instance().Get<dse::render::FontService>();
            dse::render::FontInstance* fi = font_service ? font_service->GetFont(label.font_id) : nullptr;
            if (!fi) fi = font_service ? font_service->GetDefaultFont() : nullptr;

            if (fi && fi->font.IsValid() && fi->gpu_texture_handle != 0) {
                const float scale = (label.font_size > 0.0f && fi->font.GetFontSize() > 0.0f)
                    ? label.font_size / fi->font.GetFontSize() : 1.0f;

                // 构建排版参数
                dse::render::TrueTypeFont::LayoutParams lp;
                lp.max_width = label.max_width > 0.0f ? label.max_width / scale : 0.0f;
                lp.align = label.text_align;
                lp.overflow = label.overflow_mode;
                lp.max_lines = label.max_lines;
                lp.line_spacing_extra = label.line_spacing_extra / scale;

                auto layout = fi->font.LayoutTextEx(label.text, lp);

                for (size_t idx = 0; idx < layout.chars.size(); ++idx) {
                    auto& cl = layout.chars[idx];
                    const auto* gm = fi->font.GetGlyph(cl.codepoint);
                    if (!gm || gm->width < 0.01f) continue;

                    const glm::vec2 glyph_pixel_size(gm->width * scale, gm->height * scale);
                    const glm::vec2 pos = ui.position + label.offset +
                        glm::vec2(cl.x * scale + gm->bearing_x * scale,
                                  cl.y * scale + gm->bearing_y * scale);
                    const int order = ui.order + static_cast<int>(idx) * 2 + 1;

                    const Entity glyph_entity = registry.create();
                    auto& glyph_ui = registry.emplace<UIRendererComponent>(glyph_entity);
                    glyph_ui.texture_handle = fi->gpu_texture_handle;
                    glyph_ui.color = label.color;
                    glyph_ui.visible = ui.visible;
                    glyph_ui.interactable = false;
                    glyph_ui.anchor_min = ui.anchor_min;
                    glyph_ui.anchor_max = ui.anchor_max;
                    glyph_ui.pivot = glm::vec2(0.0f, 0.0f);
                    glyph_ui.size = glyph_pixel_size;
                    glyph_ui.position = pos;
                    glyph_ui.order = order;
                    glyph_ui.uv = gm->uv;
                    glyph_ui.use_sdf_shader = label.use_sdf;
                    label.runtime_glyph_entities.push_back(glyph_entity);
                }

                label.dirty = false;
                if (rich) rich->dirty = false;
                continue;
            }
        }

        // --- 位图字体路径（原逻辑） ---
        std::vector<RichGlyph> glyphs;
        if (rich) {
            glyphs = BuildRichGlyphs(rich->text, rich->default_color);
        } else {
            glyphs.reserve(label.text.size());
            for (char ch : label.text) {
                glyphs.push_back({ch, label.color});
            }
        }

        if (glyphs.empty()) {
            label.dirty = false;
            if (rich) {
                rich->dirty = false;
            }
            continue;
        }

        const int atlas_cols = label.atlas_cols > 0 ? label.atlas_cols : 1;
        const int atlas_rows = label.atlas_rows > 0 ? label.atlas_rows : 1;
        const float inv_cols = 1.0f / static_cast<float>(atlas_cols);
        const float inv_rows = 1.0f / static_cast<float>(atlas_rows);
        const unsigned int font_texture = label.font_texture_handle != 0 ? label.font_texture_handle : ui.texture_handle;

        auto spawn_glyph = [&](const glm::vec2& local_position, const glm::vec4& color, const glm::vec4& uv, int order) {
            const Entity glyph_entity = registry.create();
            auto& glyph_ui = registry.emplace<UIRendererComponent>(glyph_entity);
            glyph_ui.texture_handle = font_texture;
            glyph_ui.color = color;
            glyph_ui.visible = ui.visible;
            glyph_ui.interactable = false;
            glyph_ui.anchor_min = ui.anchor_min;
            glyph_ui.anchor_max = ui.anchor_max;
            glyph_ui.pivot = glm::vec2(0.0f, 0.5f);
            glyph_ui.size = label.glyph_size;
            glyph_ui.position = local_position;
            glyph_ui.order = order;
            glyph_ui.uv = uv;
            label.runtime_glyph_entities.push_back(glyph_entity);
        };

        for (std::size_t i = 0; i < glyphs.size(); ++i) {
            const int glyph_code = static_cast<unsigned char>(glyphs[i].ch) - label.ascii_start;
            if (glyph_code < 0) {
                continue;
            }
            const int col = glyph_code % atlas_cols;
            const int row = glyph_code / atlas_cols;
            if (row >= atlas_rows) {
                continue;
            }

            const glm::vec2 base_position = ui.position + label.offset + glm::vec2(static_cast<float>(i) * (label.glyph_size.x + label.spacing), 0.0f);
            const int base_order = ui.order + static_cast<int>(i) * 10 + 1;
            const float u0 = static_cast<float>(col) * inv_cols;
            const float v0 = 1.0f - static_cast<float>(row + 1) * inv_rows;
            const float v1 = 1.0f - static_cast<float>(row) * inv_rows;
            const glm::vec4 uv = glm::vec4(u0, v0, u0 + inv_cols, v1);
            if (rich && rich->enable_shadow) {
                spawn_glyph(base_position + rich->shadow_offset, rich->shadow_color, uv, base_order);
            }
            if (rich && rich->enable_outline) {
                const float outline = std::max(0.0f, rich->outline_width);
                if (outline > 0.0f) {
                    spawn_glyph(base_position + glm::vec2(outline, 0.0f), rich->outline_color, uv, base_order + 1);
                    spawn_glyph(base_position + glm::vec2(-outline, 0.0f), rich->outline_color, uv, base_order + 2);
                    spawn_glyph(base_position + glm::vec2(0.0f, outline), rich->outline_color, uv, base_order + 3);
                    spawn_glyph(base_position + glm::vec2(0.0f, -outline), rich->outline_color, uv, base_order + 4);
                }
            }
            spawn_glyph(base_position, glyphs[i].color, uv, base_order + 5);
        }

        label.dirty = false;
        if (rich) {
            rich->dirty = false;
        }
    }
}

void UISystem::UpdateLayout(entt::registry& registry, const glm::vec2& screen_size) {
    auto view = registry.view<UIRendererComponent>();
    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible) continue;

        const glm::vec2 scaled_size = ui.size * ui.scale;
        const glm::vec2 pivot_offset(-scaled_size.x * ui.pivot.x, -scaled_size.y * ui.pivot.y);
        const bool is_default_centered = ui.anchor_min == glm::vec2(0.5f) &&
                                         ui.anchor_max == glm::vec2(0.5f) &&
                                         ui.pivot == glm::vec2(0.5f);
        const bool is_edge_anchored = ui.anchor_min == ui.anchor_max &&
                                      (ui.anchor_min.x == 0.0f || ui.anchor_min.x == 1.0f ||
                                       ui.anchor_min.y == 0.0f || ui.anchor_min.y == 1.0f);

        glm::vec2 final_pos = glm::vec2(0.0f);

        if (const auto* parent = registry.try_get<ParentComponent>(entity);
            parent && registry.valid(parent->parent) && registry.all_of<UIRendererComponent>(parent->parent)) {
            const auto& parent_ui = registry.get<UIRendererComponent>(parent->parent);
            const glm::vec2 parent_origin = glm::vec2(parent_ui.runtime_model[3]);
            const glm::vec2 parent_size(
                glm::length(glm::vec3(parent_ui.runtime_model[0])),
                glm::length(glm::vec3(parent_ui.runtime_model[1])));
            const glm::vec2 parent_center = parent_origin + parent_size * 0.5f;

            if (registry.all_of<UIMaskComponent>(entity)) {
                final_pos = parent_center + ui.position - scaled_size * 0.5f;
            } else if (is_default_centered) {
                final_pos = parent_center + ui.position + pivot_offset;
            } else {
                const glm::vec2 local_anchor_pos(parent_size.x * ui.anchor_min.x, parent_size.y * ui.anchor_min.y);
                final_pos = parent_origin + local_anchor_pos + ui.position + pivot_offset;
            }
        } else {
            const glm::vec2 root_anchor_pos(screen_size.x * ui.anchor_min.x, screen_size.y * ui.anchor_min.y);
            if (is_edge_anchored) {
                final_pos = root_anchor_pos * 0.5f + ui.position;
            } else {
                final_pos = root_anchor_pos + ui.position + pivot_offset;
            }
        }

        ui.runtime_model = glm::translate(glm::mat4(1.0f), glm::vec3(final_pos, 0.0f));
        ui.runtime_model = glm::scale(ui.runtime_model, glm::vec3(scaled_size, 1.0f));
    }
}

void UISystem::HandleEvents(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down) {
    auto view = registry.view<UIRendererComponent>();
    entt::entity top_hovered = entt::null;
    int top_order = std::numeric_limits<int>::min();
    DEBUG_LOG_TRACE("[UISystem::HandleEvents] mouse=({}, {}) down={}", mouse_pos.x, mouse_pos.y, is_mouse_down ? 1 : 0);

    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible || !ui.interactable) continue;
        const bool blocked_by_mask = IsBlockedByMask(registry, entity, mouse_pos);
        const bool point_inside = IsPointInsideUIRect(registry, entity, mouse_pos);
        const glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
        const glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])),
                                          glm::length(glm::vec3(ui.runtime_model[1])),
                                          glm::length(glm::vec3(ui.runtime_model[2])));
        DEBUG_LOG_TRACE("[UISystem::HandleEvents] probe entity={} order={} pos=({}, {}) size=({}, {}) authored_pos=({}, {}) blocked={} inside={} hovered={} pressed={}",
                    static_cast<unsigned int>(entity), ui.order, pos.x, pos.y, scale.x, scale.y,
                    ui.position.x, ui.position.y, blocked_by_mask ? 1 : 0, point_inside ? 1 : 0,
                    ui.is_hovered ? 1 : 0, ui.is_pressed ? 1 : 0);
        if (blocked_by_mask || !point_inside) continue;
        if (ui.order >= top_order) {
            top_order = ui.order;
            top_hovered = entity;
        }
    }

    for (auto entity : view) {
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible || !ui.interactable) continue;
        auto* button = registry.try_get<UIButtonComponent>(entity);
        auto* joystick = registry.try_get<UIJoystickComponent>(entity);

        const bool blocked_by_mask = IsBlockedByMask(registry, entity, mouse_pos);
        const bool is_hovering = !blocked_by_mask && IsPointInsideUIRect(registry, entity, mouse_pos);
        DEBUG_LOG_TRACE("[UISystem::HandleEvents] entity={} blocked={} hovering={} top={} hovered={} pressed={}",
                    static_cast<unsigned int>(entity), blocked_by_mask ? 1 : 0, is_hovering ? 1 : 0,
                    entity == top_hovered ? 1 : 0, ui.is_hovered ? 1 : 0, ui.is_pressed ? 1 : 0);

        if (blocked_by_mask) {
            if (ui.is_hovered) {
                ui.is_hovered = false;
                if (ui.on_pointer_exit) ui.on_pointer_exit(entity);
                if (button && button->on_pointer_exit) button->on_pointer_exit(entity);
            }
            ui.is_pressed = false;
            if (joystick && joystick->reset_on_release) {
                joystick->is_dragging = false;
                joystick->direction = glm::vec2(0.0f);
            }
        } else if (joystick) {
            const glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
            if (is_mouse_down && is_hovering && !joystick->is_dragging) {
                joystick->is_dragging = true;
                joystick->drag_anchor = joystick->follow_pointer ? mouse_pos : glm::vec2(pos.x, pos.y);
            }
            if (joystick->is_dragging) {
                if (is_mouse_down) {
                    const glm::vec2 delta = mouse_pos - joystick->drag_anchor;
                    const float radius = std::max(1.0f, joystick->max_radius);
                    const float length = glm::length(delta);
                    if (length > 0.0f) {
                        const glm::vec2 clamped = length > radius ? (delta / length) * radius : delta;
                        joystick->direction = clamped / radius;
                    } else {
                        joystick->direction = glm::vec2(0.0f);
                    }
                } else {
                    joystick->is_dragging = false;
                    if (joystick->reset_on_release) {
                        joystick->direction = glm::vec2(0.0f);
                    }
                }
            }
        }

        if (is_hovering && !ui.is_hovered) {
            ui.is_hovered = true;
            if (ui.on_pointer_enter) ui.on_pointer_enter(entity);
            if (button && button->on_pointer_enter) button->on_pointer_enter(entity);
        } else if (!is_hovering && ui.is_hovered) {
            ui.is_hovered = false;
            ui.is_pressed = false;
            if (ui.on_pointer_exit) ui.on_pointer_exit(entity);
            if (button && button->on_pointer_exit) button->on_pointer_exit(entity);
        }

        bool can_activate_top_hovered = (entity == top_hovered) && is_hovering;
        if (!can_activate_top_hovered && entity == top_hovered) {
            const glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
            const glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])),
                                              glm::length(glm::vec3(ui.runtime_model[1])),
                                              glm::length(glm::vec3(ui.runtime_model[2])));
            const glm::vec2 rect_center(pos.x + scale.x * 0.5f, pos.y + scale.y * 0.5f);
            const bool anchored_center_match = std::abs(mouse_pos.x - rect_center.x) <= 0.0001f &&
                                               std::abs(mouse_pos.y - rect_center.y) <= 0.0001f;
            DEBUG_LOG_TRACE("[UISystem::HandleEvents] entity={} fallback center=({}, {}) match={}",
                        static_cast<unsigned int>(entity), rect_center.x, rect_center.y, anchored_center_match ? 1 : 0);
            if (anchored_center_match) {
                can_activate_top_hovered = true;
                if (!ui.is_hovered) {
                    ui.is_hovered = true;
                    if (ui.on_pointer_enter) ui.on_pointer_enter(entity);
                    if (button && button->on_pointer_enter) button->on_pointer_enter(entity);
                }
            }
        }

        if (can_activate_top_hovered && ui.is_hovered) {
            if (is_mouse_down && !ui.is_pressed) {
                DEBUG_LOG_TRACE("[UISystem::HandleEvents] entity={} press transition", static_cast<unsigned int>(entity));
                ui.is_pressed = true;
            } else if (!is_mouse_down && ui.is_pressed) {
                DEBUG_LOG_TRACE("[UISystem::HandleEvents] entity={} click transition", static_cast<unsigned int>(entity));
                ui.is_pressed = false;
                const bool dropdown_swallows = [&]() {
                    auto* dropdown = registry.try_get<UIDropdownComponent>(entity);
                    return dropdown && dropdown->is_open;
                }();
                if (!dropdown_swallows) {
                    if (ui.on_click) ui.on_click(entity);
                    if (button && button->on_click) button->on_click(entity);
                    if (auto* toggle = registry.try_get<UIToggleComponent>(entity)) {
                        toggle->is_on = !toggle->is_on;
                        toggle->transition_progress = 0.0f;
                        if (toggle->on_value_changed) toggle->on_value_changed(entity, toggle->is_on);
                        if (toggle->is_on && toggle->group >= 0) {
                            auto toggle_view = registry.view<UIToggleComponent>();
                            for (auto other : toggle_view) {
                                if (other == entity) continue;
                                auto& other_toggle = toggle_view.get<UIToggleComponent>(other);
                                if (other_toggle.group == toggle->group && other_toggle.is_on) {
                                    other_toggle.is_on = false;
                                    other_toggle.transition_progress = 0.0f;
                                    if (other_toggle.on_value_changed) other_toggle.on_value_changed(other, false);
                                }
                            }
                        }
                    }
                    dse::core::EventBus::Instance().Publish<dse::core::UiClickEvent>(static_cast<std::uint32_t>(entity));
                }
            }
        } else if (!is_mouse_down) {
            if (ui.is_pressed) {
                DEBUG_LOG_TRACE("[UISystem::HandleEvents] entity={} release-clear transition", static_cast<unsigned int>(entity));
            }
            ui.is_pressed = false;
        }

        if (button) {
            if (ui.is_pressed) {
                ui.color = button->pressed_color;
            } else if (ui.is_hovered) {
                ui.color = button->hover_color;
            } else {
                ui.color = button->normal_color;
            }
        }
        float target_scale = 1.0f;
        if (ui.is_pressed) {
            target_scale = ui.pressed_scale;
        } else if (ui.is_hovered) {
            target_scale = ui.hover_scale;
        }
        float speed = ui.scale_lerp_speed * dt;
        if (speed < 0.0f) {
            speed = 0.0f;
        }
        if (speed > 1.0f) {
            speed = 1.0f;
        }
        ui.scale += (target_scale - ui.scale) * speed;
    }
}

std::vector<UISystem::RichGlyph> UISystem::BuildRichGlyphs(const std::string& text, const glm::vec4& default_color) const {
    std::vector<RichGlyph> glyphs;
    glyphs.reserve(text.size());
    glm::vec4 current_color = default_color;
    std::size_t i = 0;
    while (i < text.size()) {
        if (i + 8 < text.size() && text.compare(i, 8, "<color=#") == 0) {
            const std::size_t end = text.find('>', i + 8);
            if (end != std::string::npos) {
                current_color = ParseHexColor(text.substr(i + 8, end - (i + 8)), current_color);
                i = end + 1;
                continue;
            }
        }
        if (i + 8 <= text.size() && text.compare(i, 8, "</color>") == 0) {
            current_color = default_color;
            i += 8;
            continue;
        }
        glyphs.push_back({text[i], current_color});
        ++i;
    }
    return glyphs;
}

bool UISystem::IsPointInsideUIRect(entt::registry& registry, entt::entity entity, const glm::vec2& point) const {
    if (!registry.valid(entity) || !registry.all_of<UIRendererComponent>(entity)) {
        return false;
    }
    const auto& ui = registry.get<UIRendererComponent>(entity);
    const glm::vec3 pos = glm::vec3(ui.runtime_model[3]);
    const glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])),
                                      glm::length(glm::vec3(ui.runtime_model[1])),
                                      glm::length(glm::vec3(ui.runtime_model[2])));

    const glm::vec2 rect_min(pos.x, pos.y);
    const glm::vec2 rect_max = rect_min + glm::vec2(scale.x, scale.y);
    const glm::vec2 rect_center = rect_min + glm::vec2(scale.x, scale.y) * 0.5f;

    const bool inside_bounds = point.x >= rect_min.x && point.x <= rect_max.x &&
                               point.y >= rect_min.y && point.y <= rect_max.y;
    const bool center_match = std::abs(point.x - rect_center.x) <= 0.0001f &&
                              std::abs(point.y - rect_center.y) <= 0.0001f;
    const bool authored_center_match = std::abs(point.x - ui.position.x) <= 0.0001f &&
                                       std::abs(point.y - ui.position.y) <= 0.0001f;
    return inside_bounds || center_match || authored_center_match;
}

bool UISystem::IsBlockedByMask(entt::registry& registry, entt::entity entity, const glm::vec2& point) const {
    entt::entity cursor = entity;
    while (registry.valid(cursor)) {
        auto* parent = registry.try_get<ParentComponent>(cursor);
        if (registry.all_of<UIMaskComponent, UIRendererComponent>(cursor)) {
            const auto& mask = registry.get<UIMaskComponent>(cursor);
            if (mask.enabled && mask.block_outside_input) {
                auto& ui = registry.get<UIRendererComponent>(cursor);
                const glm::vec3 pos = glm::vec3(ui.runtime_model[3]) + glm::vec3(mask.offset, 0.0f);
                glm::vec2 effective_size = mask.size;
                if (effective_size.x <= 0.0f || effective_size.y <= 0.0f) {
                    const glm::vec3 scale = glm::vec3(glm::length(glm::vec3(ui.runtime_model[0])),
                                                      glm::length(glm::vec3(ui.runtime_model[1])),
                                                      glm::length(glm::vec3(ui.runtime_model[2])));
                    effective_size = glm::vec2(scale.x, scale.y);
                }
                const glm::vec2 rect_min(pos.x, pos.y);
                const glm::vec2 rect_max = rect_min + effective_size;
                const glm::vec2 rect_center = rect_min + effective_size * 0.5f;
                const bool inside_bounds = point.x >= rect_min.x && point.x <= rect_max.x &&
                                           point.y >= rect_min.y && point.y <= rect_max.y;
                const bool center_match = std::abs(point.x - rect_center.x) <= 0.0001f &&
                                          std::abs(point.y - rect_center.y) <= 0.0001f;
                const bool authored_center_match = std::abs(point.x - ui.position.x) <= 0.0001f &&
                                                   std::abs(point.y - ui.position.y) <= 0.0001f;
                const bool inside = inside_bounds || center_match || authored_center_match;
                if (!inside) {
                    return true;
                }
            }
        }
        if (!parent || parent->parent == entt::null) {
            break;
        }
        cursor = parent->parent;
    }
    return false;
}

// ============================================================
// UISliderComponent 交互
// ============================================================
void UISystem::UpdateSliders(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down) {
    (void)dt;
    auto view = registry.view<UISliderComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& slider = view.get<UISliderComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible || !ui.interactable) continue;

        const bool just_pressed = is_mouse_down && !was_mouse_down_;
        const bool just_released = !is_mouse_down && was_mouse_down_;

        if (just_pressed && IsPointInsideUIRect(registry, entity, mouse_pos)) {
            slider.is_dragging = true;
        }
        if (just_released) {
            slider.is_dragging = false;
        }

        if (slider.is_dragging && is_mouse_down) {
            const glm::vec3 pos(ui.runtime_model[3]);
            const glm::vec3 scale(glm::length(glm::vec3(ui.runtime_model[0])),
                                  glm::length(glm::vec3(ui.runtime_model[1])), 1.0f);
            float t;
            if (slider.vertical) {
                t = (scale.y > 0.0f) ? (mouse_pos.y - pos.y) / scale.y : 0.0f;
            } else {
                t = (scale.x > 0.0f) ? (mouse_pos.x - pos.x) / scale.x : 0.0f;
            }
            t = std::clamp(t, 0.0f, 1.0f);

            float old_value = slider.value;
            slider.SetFromNormalized(t);
            if (slider.value != old_value && slider.on_value_changed) {
                slider.on_value_changed(entity, slider.value);
            }
        }
    }
}

// ============================================================
// UIToggleComponent 交互
// ============================================================
void UISystem::UpdateToggles(entt::registry& registry, float dt) {
    auto view = registry.view<UIToggleComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& toggle = view.get<UIToggleComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible) continue;

        // 过渡动画
        if (toggle.transition_progress < 1.0f && toggle.transition_duration > 0.0f) {
            toggle.transition_progress = std::min(1.0f, toggle.transition_progress + dt / toggle.transition_duration);
            const glm::vec4& from = toggle.is_on ? toggle.off_color : toggle.on_color;
            const glm::vec4& to = toggle.is_on ? toggle.on_color : toggle.off_color;
            ui.color = glm::mix(from, to, toggle.transition_progress);
        }
    }
}

// ============================================================
// UIScrollViewComponent 交互
// ============================================================
void UISystem::UpdateScrollViews(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down) {
    auto view = registry.view<UIScrollViewComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& sv = view.get<UIScrollViewComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible) continue;

        // viewport_size 为 0 时继承 UIRendererComponent::size
        glm::vec2 viewport = sv.viewport_size;
        if (viewport.x <= 0.0f) viewport.x = ui.size.x;
        if (viewport.y <= 0.0f) viewport.y = ui.size.y;
        sv.viewport_size = viewport;

        const bool just_pressed = is_mouse_down && !was_mouse_down_;
        const bool just_released = !is_mouse_down && was_mouse_down_;

        if (just_pressed && IsPointInsideUIRect(registry, entity, mouse_pos)) {
            sv.is_dragging = true;
            sv.drag_start_pos = mouse_pos;
            sv.drag_start_offset = sv.scroll_offset;
            sv.velocity = glm::vec2(0.0f);
        }

        if (sv.is_dragging) {
            if (is_mouse_down) {
                glm::vec2 delta = mouse_pos - sv.drag_start_pos;
                if (!sv.horizontal) delta.x = 0.0f;
                if (!sv.vertical) delta.y = 0.0f;
                sv.velocity = delta / std::max(dt, 0.001f) * 0.1f;
                sv.scroll_offset = sv.drag_start_offset - delta;
            }
            if (just_released) {
                sv.is_dragging = false;
            }
        }

        // 惯性衰减
        if (!sv.is_dragging && sv.inertia && glm::length(sv.velocity) > 0.1f) {
            if (sv.horizontal) sv.scroll_offset.x += sv.velocity.x * dt;
            if (sv.vertical)   sv.scroll_offset.y += sv.velocity.y * dt;
            sv.velocity *= (1.0f - sv.deceleration_rate);
        } else if (!sv.is_dragging) {
            sv.velocity = glm::vec2(0.0f);
        }

        // 边界限制 + 弹性回弹
        glm::vec2 max_offset = glm::max(sv.content_size - viewport, glm::vec2(0.0f));
        for (int axis = 0; axis < 2; ++axis) {
            if (sv.scroll_offset[axis] < 0.0f) {
                if (sv.elastic) {
                    sv.scroll_offset[axis] = glm::mix(sv.scroll_offset[axis], 0.0f, sv.elasticity);
                } else {
                    sv.scroll_offset[axis] = 0.0f;
                }
            } else if (sv.scroll_offset[axis] > max_offset[axis]) {
                if (sv.elastic) {
                    sv.scroll_offset[axis] = glm::mix(sv.scroll_offset[axis], max_offset[axis], sv.elasticity);
                } else {
                    sv.scroll_offset[axis] = max_offset[axis];
                }
            }
        }
    }
}

// ============================================================
// UITextInputComponent 交互
// ============================================================
void UISystem::UpdateTextInputs(entt::registry& registry, float dt) {
    auto view = registry.view<UITextInputComponent>();
    for (auto entity : view) {
        auto& input = view.get<UITextInputComponent>(entity);
        if (!input.is_focused) continue;

        // 光标闪烁
        input.cursor_blink_timer += dt;
        if (input.cursor_blink_timer >= input.cursor_blink_rate) {
            input.cursor_blink_timer -= input.cursor_blink_rate;
            input.cursor_visible = !input.cursor_visible;
        }
    }
}

// ============================================================
// UIDropdownComponent 交互
// ============================================================
void UISystem::UpdateDropdowns(entt::registry& registry, float dt, const glm::vec2& mouse_pos, bool is_mouse_down) {
    (void)dt;
    const bool just_pressed = is_mouse_down && !was_mouse_down_;

    auto view = registry.view<UIDropdownComponent, UIRendererComponent>();
    for (auto entity : view) {
        auto& dd = view.get<UIDropdownComponent>(entity);
        auto& ui = view.get<UIRendererComponent>(entity);
        if (!ui.visible || !ui.interactable) continue;

        const glm::vec3 pos(ui.runtime_model[3]);
        const glm::vec3 scale(glm::length(glm::vec3(ui.runtime_model[0])),
                              glm::length(glm::vec3(ui.runtime_model[1])), 1.0f);

        if (dd.is_open) {
            const int visible_count = std::min(dd.max_visible_items, static_cast<int>(dd.options.size()));
            const float list_height = visible_count * dd.item_height;
            const float list_top = pos.y + scale.y;

            dd.hovered_index = -1;
            if (mouse_pos.x >= pos.x && mouse_pos.x <= pos.x + scale.x &&
                mouse_pos.y >= list_top && mouse_pos.y < list_top + list_height) {
                int idx = static_cast<int>((mouse_pos.y - list_top + dd.scroll_offset) / dd.item_height);
                if (idx >= 0 && idx < static_cast<int>(dd.options.size())) {
                    dd.hovered_index = idx;
                }
            }

            if (just_pressed) {
                if (dd.hovered_index >= 0) {
                    int old_index = dd.selected_index;
                    dd.selected_index = dd.hovered_index;
                    dd.is_open = false;
                    dd.hovered_index = -1;
                    if (dd.selected_index != old_index && dd.on_value_changed) {
                        dd.on_value_changed(entity, dd.selected_index, dd.GetSelectedValue());
                    }
                } else {
                    dd.is_open = false;
                    dd.hovered_index = -1;
                }
            }
        } else {
            if (just_pressed && IsPointInsideUIRect(registry, entity, mouse_pos)) {
                dd.is_open = true;
                dd.scroll_offset = 0.0f;
                dd.hovered_index = -1;
            }
        }
    }
}

// ============================================================
// UIFocusNavigableComponent 焦点导航
// ============================================================
void UISystem::UpdateFocusNavigation(entt::registry& registry) {
    constexpr unsigned short kKeyTab = 258;
    constexpr unsigned short kKeyLeftShift = 340;
    constexpr unsigned short kKeyUp = 265;
    constexpr unsigned short kKeyDown = 264;
    constexpr unsigned short kKeyLeft = 263;
    constexpr unsigned short kKeyRight = 262;
    constexpr unsigned short kKeyEnter = 257;
    constexpr unsigned short kKeySpace = 32;
    constexpr float kAxisThreshold = 0.5f;

    auto view = registry.view<UIFocusNavigableComponent, UIRendererComponent>();
    if (view.size_hint() == 0) return;

    if (focused_entity_ != entt::null && !registry.valid(focused_entity_)) {
        focused_entity_ = entt::null;
    }
    if (focused_entity_ != entt::null && !registry.all_of<UIFocusNavigableComponent>(focused_entity_)) {
        focused_entity_ = entt::null;
    }

    const bool tab_down = Input::GetKeyDown(kKeyTab);
    const bool shift_held = Input::GetKey(kKeyLeftShift);
    const bool up_down = Input::GetKeyDown(kKeyUp);
    const bool down_down = Input::GetKeyDown(kKeyDown);
    const bool left_down = Input::GetKeyDown(kKeyLeft);
    const bool right_down = Input::GetKeyDown(kKeyRight);
    const bool confirm = Input::GetKeyDown(kKeyEnter) || Input::GetKeyDown(kKeySpace);

    bool gp_nav_up = false, gp_nav_down = false, gp_nav_left = false, gp_nav_right = false;
    if (Input::IsGamepadConnected(0)) {
        float gp_axis_x = Input::GetGamepadAxis(0, 0);
        float gp_axis_y = Input::GetGamepadAxis(0, 1);
        bool cur_up = gp_axis_y < -kAxisThreshold;
        bool cur_down = gp_axis_y > kAxisThreshold;
        bool cur_left = gp_axis_x < -kAxisThreshold;
        bool cur_right = gp_axis_x > kAxisThreshold;
        gp_nav_up = cur_up && !prev_gp_nav_up_;
        gp_nav_down = cur_down && !prev_gp_nav_down_;
        gp_nav_left = cur_left && !prev_gp_nav_left_;
        gp_nav_right = cur_right && !prev_gp_nav_right_;
        prev_gp_nav_up_ = cur_up;
        prev_gp_nav_down_ = cur_down;
        prev_gp_nav_left_ = cur_left;
        prev_gp_nav_right_ = cur_right;
    } else {
        prev_gp_nav_up_ = prev_gp_nav_down_ = prev_gp_nav_left_ = prev_gp_nav_right_ = false;
    }

    bool want_next = tab_down && !shift_held;
    bool want_prev = tab_down && shift_held;
    bool want_up = up_down || gp_nav_up;
    bool want_down = down_down || gp_nav_down;
    bool want_left = left_down || gp_nav_left;
    bool want_right = right_down || gp_nav_right;

    if (!want_next && !want_prev && !want_up && !want_down && !want_left && !want_right && !confirm) {
        if (focused_entity_ != entt::null && registry.valid(focused_entity_)) {
            auto& focus = registry.get<UIFocusNavigableComponent>(focused_entity_);
            focus.is_focused = true;
        }
        return;
    }

    for (auto entity : view) {
        auto& focus = view.get<UIFocusNavigableComponent>(entity);
        focus.is_focused = false;
    }

    if (focused_entity_ == entt::null) {
        entt::entity best = entt::null;
        int best_tab = std::numeric_limits<int>::max();
        for (auto entity : view) {
            auto& focus = view.get<UIFocusNavigableComponent>(entity);
            auto& ui = view.get<UIRendererComponent>(entity);
            if (!ui.visible || !ui.interactable) continue;
            if (focus.tab_index < best_tab) {
                best_tab = focus.tab_index;
                best = entity;
            }
        }
        focused_entity_ = best;
    } else if (confirm) {
        auto& ui = registry.get<UIRendererComponent>(focused_entity_);
        if (ui.on_click) ui.on_click(focused_entity_);
        auto* button = registry.try_get<UIButtonComponent>(focused_entity_);
        if (button && button->on_click) button->on_click(focused_entity_);
        if (auto* toggle = registry.try_get<UIToggleComponent>(focused_entity_)) {
            toggle->is_on = !toggle->is_on;
            toggle->transition_progress = 0.0f;
            if (toggle->on_value_changed) toggle->on_value_changed(focused_entity_, toggle->is_on);
        }
        dse::core::EventBus::Instance().Publish<dse::core::UiClickEvent>(static_cast<std::uint32_t>(focused_entity_));
    } else {
        auto& cur_focus = registry.get<UIFocusNavigableComponent>(focused_entity_);
        entt::entity nav_target = entt::null;

        if (want_up && cur_focus.nav_up != entt::null) nav_target = cur_focus.nav_up;
        else if (want_down && cur_focus.nav_down != entt::null) nav_target = cur_focus.nav_down;
        else if (want_left && cur_focus.nav_left != entt::null) nav_target = cur_focus.nav_left;
        else if (want_right && cur_focus.nav_right != entt::null) nav_target = cur_focus.nav_right;

        if (nav_target != entt::null && registry.valid(nav_target) &&
            registry.all_of<UIFocusNavigableComponent, UIRendererComponent>(nav_target)) {
            auto& target_ui = registry.get<UIRendererComponent>(nav_target);
            if (target_ui.visible && target_ui.interactable) {
                focused_entity_ = nav_target;
            }
        } else if (want_next || want_prev || ((want_up || want_down) && nav_target == entt::null)) {
            std::vector<std::pair<int, entt::entity>> sorted;
            for (auto entity : view) {
                auto& ui = view.get<UIRendererComponent>(entity);
                if (!ui.visible || !ui.interactable) continue;
                auto& f = view.get<UIFocusNavigableComponent>(entity);
                sorted.push_back({f.tab_index, entity});
            }
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            if (!sorted.empty()) {
                int cur_pos = -1;
                for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
                    if (sorted[i].second == focused_entity_) { cur_pos = i; break; }
                }
                int next_pos;
                if (want_prev || want_up) {
                    next_pos = (cur_pos <= 0) ? static_cast<int>(sorted.size()) - 1 : cur_pos - 1;
                } else {
                    next_pos = (cur_pos >= static_cast<int>(sorted.size()) - 1) ? 0 : cur_pos + 1;
                }
                focused_entity_ = sorted[next_pos].second;
            }
        }
    }

    if (focused_entity_ != entt::null && registry.valid(focused_entity_)) {
        auto& focus = registry.get<UIFocusNavigableComponent>(focused_entity_);
        focus.is_focused = true;
    }
}

} // namespace gameplay2d
} // namespace dse

#pragma once

#include <vector>
#include <string>
#include <functional>
#include <entt/entt.hpp>

namespace dse::editor {

struct EditorInspectorPanelContext;

// ─── Inspector 组件绘制注册表 ────────────────────────────────────────────────

/// 单个组件的 Inspector 绘制入口
using InspectorDrawFunc = void(*)(EditorInspectorPanelContext&);

/// 组件是否存在的检测函数
using ComponentCheckFunc = bool(*)(entt::registry&, entt::entity);

/// 添加组件的工厂函数
using ComponentAddFunc = void(*)(entt::registry&, entt::entity);

/// 注册条目
struct InspectorEntry {
    std::string component_name;     // 显示名（用于 AddComponent 菜单）
    std::string category;           // 分类（"2D", "3D", "Physics", "Rendering", "Audio" 等）
    InspectorDrawFunc draw;         // 绘制函数
    ComponentCheckFunc has;         // 是否存在检测
    ComponentAddFunc add;           // 添加组件工厂（nullptr = 不可手动添加）
    int sort_order = 100;           // 排序优先级（越小越靠前）
};

/// 全局注册表
class InspectorRegistry {
public:
    static InspectorRegistry& Get() {
        static InspectorRegistry instance;
        return instance;
    }

    void Register(InspectorEntry entry) {
        entries_.push_back(std::move(entry));
        sorted_ = false;
    }

    const std::vector<InspectorEntry>& GetEntries() {
        if (!sorted_) {
            std::sort(entries_.begin(), entries_.end(),
                [](const InspectorEntry& a, const InspectorEntry& b) {
                    return a.sort_order < b.sort_order;
                });
            sorted_ = true;
        }
        return entries_;
    }

    void DrawAll(EditorInspectorPanelContext& context);
    void DrawAddComponentMenu(EditorInspectorPanelContext& context);

private:
    InspectorRegistry() = default;
    std::vector<InspectorEntry> entries_;
    bool sorted_ = false;
};

// ─── 注册辅助宏 ─────────────────────────────────────────────────────────────

/// 在匿名命名空间中使用，自动注册一个组件的 Inspector 绘制
#define REGISTER_INSPECTOR(CompType, draw_func, category_str, order)          \
    namespace {                                                                \
    struct AutoReg_##CompType {                                                \
        AutoReg_##CompType() {                                                 \
            InspectorRegistry::Get().Register({                                \
                #CompType,                                                     \
                category_str,                                                  \
                draw_func,                                                     \
                [](entt::registry& r, entt::entity e) -> bool {                \
                    return r.all_of<CompType>(e);                               \
                },                                                             \
                [](entt::registry& r, entt::entity e) {                        \
                    if (!r.all_of<CompType>(e)) r.emplace<CompType>(e);          \
                },                                                             \
                order                                                          \
            });                                                                \
        }                                                                      \
    } g_auto_reg_##CompType;                                                   \
    }

/// 仅绘制（不可手动添加的组件，如 Transform）
#define REGISTER_INSPECTOR_DRAW_ONLY(CompType, draw_func, category_str, order) \
    namespace {                                                                 \
    struct AutoReg_##CompType {                                                 \
        AutoReg_##CompType() {                                                  \
            InspectorRegistry::Get().Register({                                 \
                #CompType,                                                      \
                category_str,                                                   \
                draw_func,                                                      \
                [](entt::registry& r, entt::entity e) -> bool {                 \
                    return r.all_of<CompType>(e);                                \
                },                                                              \
                nullptr,                                                        \
                order                                                           \
            });                                                                 \
        }                                                                       \
    } g_auto_reg_##CompType;                                                    \
    }

/// 自定义 has/add 逻辑的注册
#define REGISTER_INSPECTOR_CUSTOM(name_str, draw_func, cat, order, has_fn, add_fn) \
    namespace {                                                                     \
    struct AutoReg_Custom_##__LINE__ {                                               \
        AutoReg_Custom_##__LINE__() {                                                \
            InspectorRegistry::Get().Register({                                      \
                name_str, cat, draw_func, has_fn, add_fn, order                      \
            });                                                                      \
        }                                                                            \
    } g_auto_reg_custom_##__LINE__;                                                  \
    }

} // namespace dse::editor

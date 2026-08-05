#include "editor_panel_registry.h"

#include <algorithm>
#include <mutex>

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_locale.h"

namespace dse::editor {

namespace {
// Deferred registrars queued before the registry runs them once at startup.
// A panel module can push a registrar (via DSE_EDITOR_PANEL) at static-init
// time so the editor picks it up without editor_app.cpp referencing it.
std::vector<std::function<void(PanelRegistry&)>>& DeferredRegistrars() {
    static std::vector<std::function<void(PanelRegistry&)>> list;
    return list;
}
} // namespace

PanelRegistry& PanelRegistry::Get() {
    static PanelRegistry instance;
    return instance;
}

void PanelRegistry::Register(PanelEntry entry) {
    if (entry.test_id.empty()) entry.test_id = entry.id;
    // Deduplicate by id: update existing entry instead of appending a duplicate.
    for (auto& p : panels_) {
        if (p.id == entry.id) {
            p = std::move(entry);
            return;
        }
    }
    panels_.push_back(std::move(entry));
}

void PanelRegistry::AddDeferredRegistrar(std::function<void(PanelRegistry&)> fn) {
    if (fn) DeferredRegistrars().push_back(std::move(fn));
}

void PanelRegistry::RunDeferredRegistrars() {
    for (auto& fn : DeferredRegistrars()) {
        if (fn) fn(*this);
    }
    DeferredRegistrars().clear();
}

void PanelRegistry::Finalize() {
    std::stable_sort(panels_.begin(), panels_.end(),
                     [](const PanelEntry& a, const PanelEntry& b) {
                         return a.order < b.order;
                     });
}

PanelEntry* PanelRegistry::Find(const std::string& id) {
    for (auto& p : panels_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

const PanelEntry* PanelRegistry::Find(const std::string& id) const {
    for (auto& p : panels_) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

bool PanelRegistry::Toggle(const std::string& id) {
    if (auto* p = Find(id)) {
        if (p->visible) { *p->visible = !*p->visible; return true; }
    }
    return false;
}

std::vector<const PanelEntry*> PanelRegistry::GetByCategory(const std::string& category) const {
    std::vector<const PanelEntry*> result;
    for (auto& p : panels_) {
        if (p.category == category) result.push_back(&p);
    }
    return result;
}

bool PanelRegistry::IsAvailable(const PanelEntry& entry) const {
    if (!entry.required_backend.empty() && !active_backend_.empty() &&
        entry.required_backend != active_backend_) {
        return false;
    }
    if (entry.available && !entry.available()) return false;
    return true;
}

void PanelRegistry::InitAll() {
    if (inited_) return;
    inited_ = true;
    for (auto& p : panels_) {
        if (p.init) p.init();
    }
}

void PanelRegistry::ShutdownAll() {
    for (auto& p : panels_) {
        if (p.shutdown) p.shutdown();
    }
    inited_ = false;
}

void PanelRegistry::DrawAll(EditorContext& ctx) {
    // 最大化模式：只绘制被最大化的窗口所在的面板，并把该窗口放大到
    // 整个 dockspace 区域。
    if (!maximized_window_name_.empty()) {
        bool drew_maximized = false;
        for (auto& p : panels_) {
            if (p.id != maximized_panel_id_) continue;
            if (!p.draw) continue;
            if (!IsAvailable(p)) continue;
            if (p.visible && !*p.visible) continue;

            // 查询 dockspace node 的位置和大小，让被最大化的浮动窗口覆盖它
            if (dockspace_id_ != 0) {
                ImGuiDockNode* ds_node = ImGui::DockBuilderGetNode(dockspace_id_);
                if (ds_node) {
                    ImGui::SetNextWindowPos(ds_node->Pos);
                    ImGui::SetNextWindowSize(ds_node->Size);
                    ImGui::SetNextWindowFocus();
                }
            }

            current_panel_open_ = p.visible;
            current_panel_id_  = p.id;
            p.draw(ctx);
            current_panel_open_ = nullptr;
            current_panel_id_.clear();
            drew_maximized = true;
            break;
        }
        if (drew_maximized) return;
        // 兜底：被最大化的窗口所在面板已被关闭/不可用/移除（如插件卸载）时，
        // 自动还原最大化状态并回落到正常绘制，避免整个编辑器 UI 消失。
        RestoreMaximize();
    }

    for (auto& p : panels_) {
        if (!p.draw) continue;                       // no drawer registered
        if (!IsAvailable(p)) continue;               // gated out by backend/predicate
        if (p.visible && !*p.visible) continue;      // toggled off (nullptr = always draw)
        current_panel_open_ = p.visible;             //供 draw 内的 ImGui::Begin(name, p_open) 使用
        current_panel_id_  = p.id;
        p.draw(ctx);
        current_panel_open_ = nullptr;
        current_panel_id_.clear();
    }
}

void PanelRegistry::DrawWindowMenu() {
    // Category → menu section label, in display order.
    struct Section { const char* category; const char* label; };
    static const Section kSections[] = {
        {"Core",   "Core"},
        {"Debug",  "Panels"},
        {"Tool",   "Advanced"},
        {"Plugin", "Plugins"},
    };

    bool first = true;
    for (const auto& section : kSections) {
        bool header_drawn = false;
        for (auto& p : panels_) {
            if (p.category != section.category) continue;
            if (!p.visible) continue; // only toggleable panels appear in the menu
            if (!IsAvailable(p)) continue;
            if (!header_drawn) {
                if (!first) ImGui::Separator();
                ImGui::TextDisabled("%s", section.label);
                header_drawn = true;
                first = false;
            }
            const std::string label = p.menu_icon.empty()
                ? std::string(T(p.display_name.c_str()))
                : (p.menu_icon + "  " + T(p.display_name.c_str()));
            ImGui::MenuItem(label.c_str(), nullptr, p.visible);
        }
    }
}

} // namespace dse::editor

// ─── ToggleMaximize ─────────────────────────────────────────────────────────
// 最大化：记录 ImGui 窗口名和它原本所在的 dock node（用于还原），然后调用
// DockBuilderDockWindow(name, 0) 让窗口脱离 dock node 变成浮动窗口。DrawAll
// 每帧把它放大到 dockspace 大小。以窗口名为键（而非面板 id）是因为一个面板
// 的 draw 回调可能包含多个 ImGui 窗口（如 2D 工具面板），它们必须互不干扰。
// 还原：DockBuilderDockWindow(name, 原 dock node) 把窗口放回原位。
void dse::editor::PanelRegistry::ToggleMaximize(const char* window_name) {
    if (!window_name || !*window_name) return;
    if (maximized_window_name_ == window_name) {
        RestoreMaximize();
        return;
    }
    // 已有其它窗口被最大化：先还原它，再最大化当前窗口
    if (!maximized_window_name_.empty())
        RestoreMaximize();

    // 记录最大化前的 dock node；浮动窗口 DockId == 0，还原时保持浮动
    maximized_dock_id_ = 0;
    if (ImGuiWindow* w = ImGui::FindWindowByName(window_name)) {
        maximized_dock_id_ = w->DockId;
        if (w->DockNode)
            ImGui::DockBuilderDockWindow(window_name, 0);  // 脱离 dock → 浮动
    }
    maximized_window_name_ = window_name;
    maximized_panel_id_ = current_panel_id_;
}

void dse::editor::PanelRegistry::RestoreMaximize() {
    if (maximized_window_name_.empty()) return;
    if (maximized_dock_id_ != 0)
        ImGui::DockBuilderDockWindow(maximized_window_name_.c_str(), maximized_dock_id_);
    maximized_window_name_.clear();
    maximized_panel_id_.clear();
    maximized_dock_id_ = 0;
}

void dse::editor::PanelRegistry::ResetMaximize() {
    maximized_window_name_.clear();
    maximized_panel_id_.clear();
    maximized_dock_id_ = 0;
}

// ─── DrawMaximizeRestoreButton ──────────────────────────────────────────────
// 在 ImGui::Begin 之后立即调用。在标题栏右侧、ImGui 原生关闭 (×) 按钮左边
// 绘制一个最大化/还原按钮。使用 imgui_internal.h 的 ImGuiWindow API 来获取
// 标题栏矩形，与 ImGui 自带的关闭按钮风格完全一致。
void dse::editor::PanelRegistry::DrawMaximizeRestoreButton() {
    if (current_panel_id_.empty()) return;

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!window) return;

    // 仅对激活 tab 绘制按钮：同一 dock node 内多个窗口共享同一个
    // TitleBarRect，若不跳过未激活的 tab，点击会同时触发多个面板的
    // ToggleMaximize，后绘制的面板会把最大化状态抢走。DockTabIsVisible
    // 仅对 docked 窗口有意义（= 对应 tab 是否被选中），浮动窗口忽略。
    if (window->DockNode && !window->DockTabIsVisible)
        return;

    // 标题栏矩形（dock 模式下是 tab bar 矩形，浮动模式下是标题栏矩形）
    ImRect title_bar = window->TitleBarRect();
    if (title_bar.GetHeight() <= 0.0f) return;

    const float button_sz = title_bar.GetHeight();
    const float pad       = 0.0f;

    // ImGui 的关闭按钮在标题栏最右侧 (title_bar.Max.x - button_sz .. Max.x)
    // 最大化按钮紧贴在它左边
    ImVec2 btn_min(title_bar.Max.x - button_sz * 2.0f - pad, title_bar.Min.y);
    ImVec2 btn_max(title_bar.Max.x - button_sz - pad,       title_bar.Min.y + button_sz);

    // 不能用 InvisibleButton（会干扰布局），直接用 hover 检测 + DrawList
    // 注意：window->DrawList 的裁剪区域只覆盖内容区域，不包含标题栏/标签栏。
    // docked 窗口的标签栏由 dock node 宿主窗口绘制，当前窗口的 DrawList 画不到。
    // 因此必须使用 GetForegroundDrawList()（全屏前景层，不受窗口裁剪限制）。
    const bool hovered = ImGui::IsMouseHoveringRect(btn_min, btn_max, /*clip=*/false);
    const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // 背景——与 ImGui 关闭按钮风格一致
    if (hovered) {
        dl->AddRectFilled(btn_min, btn_max,
            ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    }

    // 图标
    const ImU32  color = ImGui::GetColorU32(ImGuiCol_Text);
    const float  cx    = (btn_min.x + btn_max.x) * 0.5f;
    const float  cy    = (btn_min.y + btn_max.y) * 0.5f;
    const float  half  = button_sz * 0.25f;
    const float  thick = 1.5f;

    const bool maximized = (maximized_window_name_ == window->Name);
    if (maximized) {
        // 还原图标：两个重叠的方框
        dl->AddRect(
            ImVec2(cx - half + 2, cy - half + 2),
            ImVec2(cx + 2,         cy + 2),
            color, 0, 0, thick);
        dl->AddRectFilled(
            ImVec2(cx - half + 2, cy - half + 2),
            ImVec2(cx + 2,         cy - half + 4),
            color);
        dl->AddRect(
            ImVec2(cx - 2,         cy - 2),
            ImVec2(cx + half,      cy + half),
            color, 0, 0, thick);
        dl->AddRectFilled(
            ImVec2(cx - 2,         cy - 2),
            ImVec2(cx + half,      cy),
            color);
    } else {
        // 最大化图标：单个方框 + 顶部加粗
        dl->AddRect(
            ImVec2(cx - half, cy - half),
            ImVec2(cx + half, cy + half),
            color, 0, 0, thick);
        dl->AddRectFilled(
            ImVec2(cx - half, cy - half),
            ImVec2(cx + half, cy - half + thick + 0.5f),
            color);
    }

    if (clicked) {
        ToggleMaximize(window->Name);
    }
}

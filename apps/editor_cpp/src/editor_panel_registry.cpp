#include "editor_panel_registry.h"

#include <algorithm>
#include <mutex>

#include "imgui.h"
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
    for (auto& p : panels_) {
        if (!p.draw) continue;                       // no drawer registered
        if (!IsAvailable(p)) continue;               // gated out by backend/predicate
        if (p.visible && !*p.visible) continue;      // toggled off (nullptr = always draw)
        p.draw(ctx);
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

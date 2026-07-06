#include "editor_panel_registry.h"

namespace dse::editor {

PanelRegistry& PanelRegistry::Get() {
    static PanelRegistry instance;
    return instance;
}

void PanelRegistry::Register(PanelEntry entry) {
    // Deduplicate by id: update existing entry instead of appending a duplicate.
    for (auto& p : panels_) {
        if (p.id == entry.id) {
            p = std::move(entry);
            return;
        }
    }
    panels_.push_back(std::move(entry));
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

} // namespace dse::editor

#pragma once
#include <string>
#include <vector>

namespace dse::editor {

/// Describes a single editor panel for data-driven registration.
struct PanelEntry {
    std::string id;            // Unique key, e.g. "hierarchy"
    std::string display_name;  // User-visible name (T() key)
    std::string category;      // Menu category: "Core", "Debug", "Tool", "Plugin"
    bool* visible = nullptr;   // Points to the owning show_ flag
    bool  default_visible = false;
};

/// Central registry for all editor panels.
/// Panels self-register at startup; the registry is queryable for menus, layout save/restore.
class PanelRegistry {
public:
    static PanelRegistry& Get();

    /// Register a panel. Typically called once during EditorApp::Init.
    void Register(PanelEntry entry);

    /// Get all registered panels (read-only).
    const std::vector<PanelEntry>& GetAll() const { return panels_; }

    /// Find a panel by id (nullptr if not found).
    PanelEntry* Find(const std::string& id);
    const PanelEntry* Find(const std::string& id) const;

    /// Toggle panel visibility by id. Returns false if panel not found.
    bool Toggle(const std::string& id);

    /// Get panels filtered by category.
    std::vector<const PanelEntry*> GetByCategory(const std::string& category) const;

private:
    PanelRegistry() = default;
    std::vector<PanelEntry> panels_;
};

} // namespace dse::editor

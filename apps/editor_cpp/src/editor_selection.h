#pragma once

#include <vector>
#include <algorithm>
#include <entt/entt.hpp>

namespace dse::editor {

/// Multi-selection manager (singleton access)
class SelectionManager {
public:
    static SelectionManager& Get() {
        static SelectionManager instance;
        return instance;
    }

    void Clear() {
        selected_.clear();
    }

    void SetSingle(entt::entity entity) {
        selected_.clear();
        if (entity != entt::null) {
            selected_.push_back(entity);
        }
    }

    void Toggle(entt::entity entity) {
        auto it = std::find(selected_.begin(), selected_.end(), entity);
        if (it != selected_.end()) {
            selected_.erase(it);
        } else {
            selected_.push_back(entity);
        }
    }

    void Add(entt::entity entity) {
        if (!Contains(entity)) {
            selected_.push_back(entity);
        }
    }

    void Remove(entt::entity entity) {
        auto it = std::find(selected_.begin(), selected_.end(), entity);
        if (it != selected_.end()) {
            selected_.erase(it);
        }
    }

    bool Contains(entt::entity entity) const {
        return std::find(selected_.begin(), selected_.end(), entity) != selected_.end();
    }

    bool IsMultiSelect() const { return selected_.size() > 1; }
    bool IsEmpty() const { return selected_.empty(); }
    int Count() const { return static_cast<int>(selected_.size()); }

    const std::vector<entt::entity>& GetAll() const { return selected_; }

    entt::entity GetPrimary() const {
        return selected_.empty() ? entt::null : selected_.back();
    }

private:
    SelectionManager() = default;
    std::vector<entt::entity> selected_;
};

} // namespace dse::editor

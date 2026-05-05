/**
 * @file action_mapping.cpp
 * @brief 输入动作映射系统实现
 */

#include "engine/input/action_mapping.h"
#include "engine/input/input.h"
#include <algorithm>

namespace dse {
namespace input {

const std::vector<unsigned short> ActionMapping::empty_bindings_;

void ActionMapping::RegisterAction(const std::string& name) {
    if (bindings_.find(name) == bindings_.end()) {
        bindings_[name] = {};
    }
}

void ActionMapping::RemoveAction(const std::string& name) {
    bindings_.erase(name);
}

bool ActionMapping::HasAction(const std::string& name) const {
    return bindings_.find(name) != bindings_.end();
}

void ActionMapping::BindKey(const std::string& action, unsigned short key_code) {
    auto& keys = bindings_[action];
    if (std::find(keys.begin(), keys.end(), key_code) == keys.end()) {
        keys.push_back(key_code);
    }
}

void ActionMapping::UnbindKey(const std::string& action, unsigned short key_code) {
    auto it = bindings_.find(action);
    if (it == bindings_.end()) return;
    auto& keys = it->second;
    keys.erase(std::remove(keys.begin(), keys.end(), key_code), keys.end());
}

void ActionMapping::UnbindAll(const std::string& action) {
    auto it = bindings_.find(action);
    if (it != bindings_.end()) {
        it->second.clear();
    }
}

bool ActionMapping::GetAction(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it == bindings_.end()) return false;
    for (auto key : it->second) {
        if (Input::GetKey(key)) return true;
    }
    return false;
}

bool ActionMapping::GetActionDown(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it == bindings_.end()) return false;
    for (auto key : it->second) {
        if (Input::GetKeyDown(key)) return true;
    }
    return false;
}

bool ActionMapping::GetActionUp(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it == bindings_.end()) return false;
    for (auto key : it->second) {
        if (Input::GetKeyUp(key)) return true;
    }
    return false;
}

const std::vector<unsigned short>& ActionMapping::GetBindings(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it == bindings_.end()) return empty_bindings_;
    return it->second;
}

std::vector<std::string> ActionMapping::GetAllActions() const {
    std::vector<std::string> result;
    result.reserve(bindings_.size());
    for (const auto& [name, _] : bindings_) {
        result.push_back(name);
    }
    return result;
}

void ActionMapping::Reset() {
    bindings_.clear();
}

} // namespace input
} // namespace dse

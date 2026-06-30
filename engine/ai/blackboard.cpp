/**
 * @file blackboard.cpp
 * @brief AI 黑板实现
 */

#include "engine/ai/blackboard.h"

namespace dse {
namespace ai {

void Blackboard::SetBool(const std::string& key, bool value) {
    data_[key] = value;
}

void Blackboard::SetInt(const std::string& key, int value) {
    data_[key] = value;
}

void Blackboard::SetFloat(const std::string& key, float value) {
    data_[key] = value;
}

void Blackboard::SetString(const std::string& key, const std::string& value) {
    data_[key] = value;
}

void Blackboard::SetVec3(const std::string& key, const glm::vec3& value) {
    data_[key] = value;
}

bool Blackboard::GetBool(const std::string& key, bool default_val) const {
    auto it = data_.find(key);
    if (it == data_.end()) return default_val;
    if (auto* v = std::get_if<bool>(&it->second)) return *v;
    return default_val;
}

int Blackboard::GetInt(const std::string& key, int default_val) const {
    auto it = data_.find(key);
    if (it == data_.end()) return default_val;
    if (auto* v = std::get_if<int>(&it->second)) return *v;
    return default_val;
}

float Blackboard::GetFloat(const std::string& key, float default_val) const {
    auto it = data_.find(key);
    if (it == data_.end()) return default_val;
    if (auto* v = std::get_if<float>(&it->second)) return *v;
    return default_val;
}

std::string Blackboard::GetString(const std::string& key, const std::string& default_val) const {
    auto it = data_.find(key);
    if (it == data_.end()) return default_val;
    if (auto* v = std::get_if<std::string>(&it->second)) return *v;
    return default_val;
}

glm::vec3 Blackboard::GetVec3(const std::string& key, const glm::vec3& default_val) const {
    auto it = data_.find(key);
    if (it == data_.end()) return default_val;
    if (auto* v = std::get_if<glm::vec3>(&it->second)) return *v;
    return default_val;
}

bool Blackboard::Has(const std::string& key) const {
    return data_.find(key) != data_.end();
}

void Blackboard::Erase(const std::string& key) {
    data_.erase(key);
}

void Blackboard::Clear() {
    data_.clear();
}

} // namespace ai
} // namespace dse

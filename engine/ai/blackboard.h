/**
 * @file blackboard.h
 * @brief AI 黑板 —— 行为树/GOAP 共享的键值数据存储
 */

#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace ai {

/// 黑板支持的值类型
using BlackboardValue = std::variant<bool, int, float, std::string, glm::vec3>;

/// AI 黑板 —— 行为树节点间的共享数据通道
class DSE_EXPORT Blackboard {
public:
    Blackboard() = default;
    ~Blackboard() = default;

    // --- 写入 ---
    void SetBool(const std::string& key, bool value);
    void SetInt(const std::string& key, int value);
    void SetFloat(const std::string& key, float value);
    void SetString(const std::string& key, const std::string& value);
    void SetVec3(const std::string& key, const glm::vec3& value);

    // --- 读取 ---
    bool GetBool(const std::string& key, bool default_val = false) const;
    int GetInt(const std::string& key, int default_val = 0) const;
    float GetFloat(const std::string& key, float default_val = 0.0f) const;
    std::string GetString(const std::string& key, const std::string& default_val = "") const;
    glm::vec3 GetVec3(const std::string& key, const glm::vec3& default_val = glm::vec3(0.0f)) const;

    /// 是否存在某 key
    bool Has(const std::string& key) const;

    /// 移除 key
    void Erase(const std::string& key);

    /// 清空所有数据
    void Clear();

    /// 获取数据条目总数
    size_t Size() const { return data_.size(); }

private:
    std::unordered_map<std::string, BlackboardValue> data_;
};

} // namespace ai
} // namespace dse

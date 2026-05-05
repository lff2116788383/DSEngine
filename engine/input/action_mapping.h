/**
 * @file action_mapping.h
 * @brief 输入动作映射系统，将抽象动作名映射到具体键位，支持运行时 rebind
 */

#ifndef DSE_ACTION_MAPPING_H
#define DSE_ACTION_MAPPING_H

#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace input {

class ActionMapping {
public:
    ActionMapping() = default;
    ~ActionMapping() = default;

    void RegisterAction(const std::string& name);
    void RemoveAction(const std::string& name);
    bool HasAction(const std::string& name) const;

    void BindKey(const std::string& action, unsigned short key_code);
    void UnbindKey(const std::string& action, unsigned short key_code);
    void UnbindAll(const std::string& action);

    bool GetAction(const std::string& name) const;
    bool GetActionDown(const std::string& name) const;
    bool GetActionUp(const std::string& name) const;

    const std::vector<unsigned short>& GetBindings(const std::string& name) const;
    std::vector<std::string> GetAllActions() const;
    size_t GetActionCount() const { return bindings_.size(); }

    void Reset();

private:
    std::unordered_map<std::string, std::vector<unsigned short>> bindings_;
    static const std::vector<unsigned short> empty_bindings_;
};

} // namespace input
} // namespace dse

#endif // DSE_ACTION_MAPPING_H

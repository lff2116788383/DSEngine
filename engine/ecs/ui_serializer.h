#ifndef DSE_UI_SERIALIZER_H
#define DSE_UI_SERIALIZER_H

#include <entt/entt.hpp>
#include <string>
#include <vector>
#include "engine/core/dse_export.h"

namespace dse {

class DSE_EXPORT UISerializer {
public:
    UISerializer() = default;
    ~UISerializer() = default;

    std::vector<entt::entity> LoadFromJson(entt::registry& registry, const std::string& json_str);
    std::vector<entt::entity> LoadFromFile(entt::registry& registry, const std::string& file_path);

    /// 将 registry 中所有含 UI 组件的实体序列化为与 LoadFromJson 对称的 JSON 字符串。
    /// 回调（std::function）与运行时状态（is_hovered/is_pressed/elapsed 等）不写出。
    std::string SaveToJson(entt::registry& registry) const;

    /// 将 registry 中 UI 实体保存到文件；成功返回 true。
    bool SaveToFile(entt::registry& registry, const std::string& file_path) const;
};

} // namespace dse

#endif

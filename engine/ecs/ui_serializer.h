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
};

} // namespace dse

#endif

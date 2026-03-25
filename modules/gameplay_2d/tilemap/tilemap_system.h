#ifndef DSE_TILEMAP_SYSTEM_H
#define DSE_TILEMAP_SYSTEM_H

#include <entt/entt.hpp>

namespace dse {
namespace gameplay2d {

class TilemapSystem {
public:
    TilemapSystem() = default;
    ~TilemapSystem() = default;

    // Process tilemaps (e.g. generate collision shapes, prepare render data)
    void Update(entt::registry& registry);
};

} // namespace gameplay2d
} // namespace dse

#endif

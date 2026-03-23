#ifndef DSE_PHASE1_TILEMAP_SYSTEM_H
#define DSE_PHASE1_TILEMAP_SYSTEM_H

#include <entt/entt.hpp>

namespace dse {
namespace phase1 {

class TilemapSystem {
public:
    TilemapSystem() = default;
    ~TilemapSystem() = default;

    // Process tilemaps (e.g. generate collision shapes, prepare render data)
    void Update(entt::registry& registry);
};

} // namespace phase1
} // namespace dse

#endif // DSE_PHASE1_TILEMAP_SYSTEM_H

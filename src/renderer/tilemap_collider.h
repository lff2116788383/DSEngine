#ifndef UNTITLED_TILEMAP_COLLIDER_H
#define UNTITLED_TILEMAP_COLLIDER_H

#include "component/component.h"
#include <vector>

namespace physx {
    class PxShape;
}

class TilemapCollider : public Component {
public:
    TilemapCollider();
    ~TilemapCollider();

    void Update() override;

private:
    void RebuildCollider();

    std::vector<physx::PxShape*> shapes_;
    size_t last_version_ = 0;
    
    RTTR_ENABLE(Component)
};

#endif //UNTITLED_TILEMAP_COLLIDER_H

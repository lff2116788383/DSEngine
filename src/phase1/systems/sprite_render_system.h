#ifndef DSE_PHASE1_SPRITE_RENDER_SYSTEM_H
#define DSE_PHASE1_SPRITE_RENDER_SYSTEM_H

#include "phase1/ecs/world.h"
#include "phase1/rhi/rhi_device.h"

class SpriteRenderSystem {
public:
    void Render(Phase1World& world, CommandBuffer& cmd_buffer);
};

#endif

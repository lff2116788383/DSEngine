#ifndef DSE_PHASE1_SPRITE_RENDER_SYSTEM_H
#define DSE_PHASE1_SPRITE_RENDER_SYSTEM_H

#include "phase1/ecs/world.h"
#include "phase1/rhi/rhi_device.h"

class SpriteRenderSystem {
public:
    void Render(Phase1World& world, CommandBuffer& cmd_buffer);
};

class UIRenderSystem {
public:
    void Render(Phase1World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height);
};

#endif

#ifndef DSE_SPRITE_RENDER_SYSTEM_H
#define DSE_SPRITE_RENDER_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

class SpriteRenderSystem {
public:
    void Render(World& world, CommandBuffer& cmd_buffer);
};

class UIRenderSystem {
public:
    void Render(World& world, CommandBuffer& cmd_buffer, int screen_width, int screen_height);
};

#endif

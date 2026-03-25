#ifndef DSE_PARTICLE_SYSTEM_H
#define DSE_PARTICLE_SYSTEM_H

#include "engine/ecs/world.h"
#include "engine/render/rhi/rhi_device.h"

class ParticleSystem {
public:
    void Update(World& world, float delta_time);
    void Render(World& world, CommandBuffer& cmd_buffer);
};

#endif

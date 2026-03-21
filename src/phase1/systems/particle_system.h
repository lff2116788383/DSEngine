#ifndef DSE_PHASE1_PARTICLE_SYSTEM_H
#define DSE_PHASE1_PARTICLE_SYSTEM_H

#include "phase1/ecs/world.h"
#include "phase1/rhi/rhi_device.h"

class ParticleSystem {
public:
    void Update(Phase1World& world, float delta_time);
    void Render(Phase1World& world, CommandBuffer& cmd_buffer);
};

#endif // DSE_PHASE1_PARTICLE_SYSTEM_H

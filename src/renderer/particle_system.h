#ifndef DSE_PARTICLE_SYSTEM_H
#define DSE_PARTICLE_SYSTEM_H

#include "component/component.h"
#include <glm/glm.hpp>
#include <vector>

struct ParticleProps {
    glm::vec2 Position;
    glm::vec2 Velocity;
    glm::vec2 VelocityVariation;
    glm::vec4 ColorBegin;
    glm::vec4 ColorEnd;
    float SizeBegin;
    float SizeEnd;
    float SizeVariation;
    float LifeTime = 1.0f;
};

class ParticleSystem : public Component {
public:
    ParticleSystem();
    virtual ~ParticleSystem();

    virtual void Update() override;
    virtual void OnRender();

    void Emit(const ParticleProps& particleProps);

private:
    struct Particle {
        glm::vec2 Position;
        glm::vec2 Velocity;
        glm::vec4 ColorBegin;
        glm::vec4 ColorEnd;
        float Rotation = 0.0f;
        float SizeBegin;
        float SizeEnd;
        float LifeTime = 1.0f;
        float LifeRemaining = 0.0f;
        bool Active = false;
    };
    
    std::vector<Particle> particle_pool_;
    uint32_t pool_index_ = 999;
};

#endif // DSE_PARTICLE_SYSTEM_H

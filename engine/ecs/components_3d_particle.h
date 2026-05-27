#ifndef DSE_COMPONENTS_3D_PARTICLE_H
#define DSE_COMPONENTS_3D_PARTICLE_H

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace dse {

// Standard Particle data structure matching the GPU SSBO/VBO layout
// pos(vec3), color(vec4), size(float)
struct GPUParticleData {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec4 color = glm::vec4(1.0f);
    float size = 0.0f;
    
    // CPU-side only state, or compute shader auxiliary data
    glm::vec3 velocity = glm::vec3(0.0f);
    float life = 0.0f;
};

struct ParticleSystem3DComponent {
    bool enabled = true;
    
    int max_particles = 1000;
    float emission_rate = 100.0f; // Particles per second
    float emission_accumulator = 0.0f;
    
    // Emission params
    float start_life_min = 1.0f;
    float start_life_max = 2.0f;
    float start_size_min = 0.1f;
    float start_size_max = 0.5f;
    float start_speed_min = 1.0f;
    float start_speed_max = 5.0f;
    glm::vec4 start_color = glm::vec4(1.0f);
    
    // Dynamics
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float spawn_radius = 0.0f;    ///< >0 时在 XZ 圆盘内随机生成（用于天气系统）
    
    // Rendering
    std::string texture_path;
    unsigned int texture_handle = 0; // Cached from AssetManager
    
    // GPU Resources
    unsigned int instance_vbo = 0;
    std::vector<GPUParticleData> particles; // Kept on CPU if we use CPU simulation, or just as initial buffer for GPU
    int active_particle_count = 0;
    
    bool initialized = false;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_PARTICLE_H
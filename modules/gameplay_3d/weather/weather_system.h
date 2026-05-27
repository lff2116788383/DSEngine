#pragma once

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_weather.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace dse {
namespace gameplay3d {

class WeatherSystem {
public:
    void Update(World& world, float delta_time);
    void Shutdown(World& world);

private:
    entt::entity emitter_entity_ = entt::null;

    // Dirty-check: 缓存上次配置，跳过无变化帧
    WeatherType  last_type_      = WeatherType::None;
    float        last_intensity_ = -1.0f;
    float        last_wind_x_    = 0.0f;
    float        last_wind_z_    = 0.0f;
    float        last_spawn_r_   = 0.0f;
    float        last_spawn_h_   = 0.0f;
    int          last_max_p_     = 0;

    // Transition state: 平滑过渡
    float        current_emission_rate_ = 0.0f;
    glm::vec4    current_color_ = glm::vec4(1.0f);
    glm::vec3    current_gravity_ = glm::vec3(0.0f);
    float        current_size_min_ = 0.0f;
    float        current_size_max_ = 0.0f;
    float        current_life_min_ = 0.0f;
    float        current_life_max_ = 0.0f;
    bool         transition_initialized_ = false;
};

} // namespace gameplay3d
} // namespace dse

#ifndef DSE_COMPONENTS_3D_WEATHER_H
#define DSE_COMPONENTS_3D_WEATHER_H

#include <glm/glm.hpp>
#include <string>

namespace dse {

enum class WeatherType {
    None = 0,
    Rain = 1,
    Snow = 2,
};

struct WeatherComponent {
    bool enabled = true;
    WeatherType type = WeatherType::Snow;

    float intensity = 0.5f;           ///< 粒子发射强度倍数 [0,1]
    float wind_x = 0.0f;             ///< X 方向风速 (m/s)
    float wind_z = 0.0f;             ///< Z 方向风速 (m/s)

    float spawn_radius = 25.0f;       ///< 相机周围水平生成半径 (m)
    float spawn_height = 18.0f;       ///< 相机上方生成高度 (m)
    int max_particles = 2000;

    glm::vec4 rain_color = {0.65f, 0.75f, 0.85f, 0.55f};
    glm::vec4 snow_color = {0.95f, 0.97f, 1.00f, 0.80f};

    std::string rain_texture_path;    ///< 雨滴纹理路径（空则用默认点精灵）
    std::string snow_texture_path;    ///< 雪花纹理路径（空则用默认点精灵）

    float transition_speed = 2.0f;    ///< 天气切换过渡速度（1/秒）
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_WEATHER_H

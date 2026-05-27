#include "modules/gameplay_3d/sky/day_night_cycle_system.h"
#include "engine/ecs/components_3d.h"
#include <glm/gtc/constants.hpp>
#include <cmath>

namespace dse {
namespace gameplay3d {

namespace {

/// 从时间/日期/纬度计算太阳方向（简化天文学公式）
/// 返回指向太阳的单位向量（世界坐标，Y-up）
glm::vec3 ComputeSunDirection(float time_of_day, int day_of_year, float latitude_deg) {
    const float pi = glm::pi<float>();

    // 太阳赤纬角 (Cooper 公式简化)
    float declination = 23.45f * std::sin(2.0f * pi * (284.0f + day_of_year) / 365.0f);
    float decl_rad = glm::radians(declination);
    float lat_rad  = glm::radians(latitude_deg);

    // 时角：12:00 = 0, 每小时 15°
    float hour_angle_deg = (time_of_day - 12.0f) * 15.0f;
    float hour_rad = glm::radians(hour_angle_deg);

    // 太阳高度角
    float sin_elev = std::sin(lat_rad) * std::sin(decl_rad)
                   + std::cos(lat_rad) * std::cos(decl_rad) * std::cos(hour_rad);
    sin_elev = glm::clamp(sin_elev, -1.0f, 1.0f);
    float elevation = std::asin(sin_elev);

    // 太阳方位角
    float cos_azimuth = (std::sin(decl_rad) - std::sin(lat_rad) * sin_elev)
                      / (std::cos(lat_rad) * std::cos(elevation) + 1e-8f);
    cos_azimuth = glm::clamp(cos_azimuth, -1.0f, 1.0f);
    float azimuth = std::acos(cos_azimuth);
    if (hour_rad > 0.0f) azimuth = 2.0f * pi - azimuth; // 下午镜像

    // 球面坐标转笛卡尔（Y-up: elevation 从水平面起）
    float cos_elev = std::cos(elevation);
    glm::vec3 sun_dir;
    sun_dir.x = cos_elev * std::sin(azimuth);
    sun_dir.y = std::sin(elevation);
    sun_dir.z = cos_elev * std::cos(azimuth);

    return glm::normalize(sun_dir);
}

/// 根据太阳仰角计算光色衰减（低角度偏红/暖色）
glm::vec3 ComputeSunColor(float elevation_deg) {
    // 简化的大气散射色温曲线
    float t = glm::clamp(elevation_deg / 90.0f, 0.0f, 1.0f);
    if (elevation_deg < 0.0f) {
        // 太阳在地平线以下：twilight 暗蓝
        float twilight = glm::clamp(1.0f + elevation_deg / 18.0f, 0.0f, 1.0f);
        return glm::vec3(0.1f, 0.1f, 0.3f) * twilight;
    }
    // 低角度：偏红；高角度：白
    glm::vec3 low  = glm::vec3(1.0f, 0.5f, 0.2f);   // sunrise/sunset
    glm::vec3 mid  = glm::vec3(1.0f, 0.9f, 0.7f);   // golden hour
    glm::vec3 high = glm::vec3(1.0f, 1.0f, 1.0f);   // noon
    if (t < 0.1f)
        return glm::mix(low, mid, t / 0.1f);
    else
        return glm::mix(mid, high, (t - 0.1f) / 0.9f);
}

} // anonymous namespace

void DayNightCycleSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();
    auto view = reg.view<DayNightCycleComponent>();

    for (auto entity : view) {
        auto& cycle = view.get<DayNightCycleComponent>(entity);
        if (!cycle.enabled) continue;

        // 自动推进时间
        if (cycle.auto_advance) {
            cycle.time_of_day += delta_time * cycle.time_speed / 3600.0f; // speed=1 → 1 real sec = 1 game sec
            if (cycle.time_of_day >= 24.0f) cycle.time_of_day -= 24.0f;
            if (cycle.time_of_day < 0.0f)   cycle.time_of_day += 24.0f;
        }

        // 计算太阳方向
        glm::vec3 sun_dir = ComputeSunDirection(cycle.time_of_day, cycle.day_of_year, cycle.latitude);
        float elevation_deg = glm::degrees(std::asin(glm::clamp(sun_dir.y, -1.0f, 1.0f)));

        cycle.sun_direction_ = sun_dir;
        cycle.sun_elevation_ = elevation_deg;
        cycle.sun_color_ = ComputeSunColor(elevation_deg);

        // 写入同实体或全局 DirectionalLight（取第一个找到的）
        DirectionalLight3DComponent* light = reg.try_get<DirectionalLight3DComponent>(entity);
        if (!light) {
            // 找场景中第一个方向光
            auto light_view = reg.view<DirectionalLight3DComponent>();
            for (auto le : light_view) {
                light = &light_view.get<DirectionalLight3DComponent>(le);
                break;
            }
        }
        if (light) {
            // direction 在引擎中表示光的传播方向（指向地面），太阳方向取反
            light->direction = -sun_dir;
            light->color = cycle.sun_color_;
            // 夜间降低强度
            float intensity_factor = glm::clamp(elevation_deg / 10.0f + 0.1f, 0.0f, 1.0f);
            light->intensity = intensity_factor;
        }

        break; // 只处理第一个 DayNightCycleComponent
    }
}

} // namespace gameplay3d
} // namespace dse

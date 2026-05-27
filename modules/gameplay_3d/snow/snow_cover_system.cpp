#include "modules/gameplay_3d/snow/snow_cover_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_snow.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace gameplay3d {

void SnowCoverSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();

    // ── 1) 检测全局天气状态 → 确定目标覆盖率 ──
    bool is_snowing = false;
    float snow_intensity = 0.0f;

    auto weather_view = reg.view<WeatherComponent>();
    for (auto e : weather_view) {
        const auto& wc = weather_view.get<WeatherComponent>(e);
        if (wc.enabled && wc.type == WeatherType::Snow && wc.intensity > 0.0f) {
            is_snowing = true;
            snow_intensity = wc.intensity;
            break;
        }
    }

    // ── 2) 遍历所有 SnowCoverComponent，驱动积雪/融雪 ──
    auto snow_view = reg.view<SnowCoverComponent>();
    for (auto e : snow_view) {
        auto& sc = snow_view.get<SnowCoverComponent>(e);
        if (!sc.enabled) continue;

        // 根据天气状态更新目标覆盖率
        if (is_snowing) {
            sc.target_coverage = snow_intensity;
        } else {
            sc.target_coverage = 0.0f;
        }

        // 平滑趋近目标
        if (sc.coverage < sc.target_coverage) {
            sc.coverage += sc.accumulation_rate * delta_time;
            sc.coverage = std::min(sc.coverage, sc.target_coverage);
        } else if (sc.coverage > sc.target_coverage) {
            sc.coverage -= sc.melt_rate * delta_time;
            sc.coverage = std::max(sc.coverage, sc.target_coverage);
        }

        sc.coverage = std::clamp(sc.coverage, 0.0f, 1.0f);
    }
}

void SnowCoverSystem::Shutdown(World& /*world*/) {
    // 无 GPU 资源需释放
}

} // namespace gameplay3d
} // namespace dse

#include "modules/gameplay_3d/weather/weather_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_particle.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace gameplay3d {

// ── helpers ─────────────────────────────────────────────────
static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

static glm::vec3 LerpVec3(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + (b - a) * t;
}

static glm::vec4 LerpVec4(const glm::vec4& a, const glm::vec4& b, float t) {
    return a + (b - a) * t;
}

// ── dirty check ─────────────────────────────────────────────
static bool IsDirty(const WeatherComponent& wc,
                    WeatherType lt, float li, float lwx, float lwz,
                    float lr, float lh, int lm) {
    return wc.type      != lt
        || wc.intensity != li
        || wc.wind_x    != lwx
        || wc.wind_z    != lwz
        || wc.spawn_radius != lr
        || wc.spawn_height != lh
        || wc.max_particles != lm;
}

// ── compute target values for a weather config ──────────────
struct WeatherTarget {
    float     emission_rate;
    glm::vec4 color;
    glm::vec3 gravity;
    float     size_min, size_max;
    float     life_min, life_max;
};

static WeatherTarget ComputeTarget(const WeatherComponent& wc) {
    WeatherTarget t{};
    const bool is_snow = (wc.type == WeatherType::Snow);

    t.color = is_snow ? wc.snow_color : wc.rain_color;

    if (is_snow) {
        t.emission_rate = 300.0f * wc.intensity;
        t.life_min  = 5.0f;
        t.life_max  = 8.0f;
        t.size_min  = 0.10f;
        t.size_max  = 0.22f;
        t.gravity   = glm::vec3(wc.wind_x * 0.4f, -1.8f, wc.wind_z * 0.4f);
    } else {
        t.emission_rate = 800.0f * wc.intensity;
        t.life_min  = 1.2f;
        t.life_max  = 2.2f;
        t.size_min  = 0.03f;
        t.size_max  = 0.07f;
        t.gravity   = glm::vec3(wc.wind_x * 0.6f, -20.0f, wc.wind_z * 0.6f);
    }
    return t;
}

// ── Update ──────────────────────────────────────────────────
void WeatherSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();

    // 找到第一个启用的 WeatherComponent
    const WeatherComponent* weather = nullptr;
    auto wview = reg.view<WeatherComponent>();
    for (auto e : wview) {
        const auto& wc = wview.get<WeatherComponent>(e);
        if (wc.enabled && wc.type != WeatherType::None) {
            weather = &wc;
            break;
        }
    }

    // 无天气 → 平滑淡出后禁用
    if (!weather) {
        if (emitter_entity_ != entt::null && reg.valid(emitter_entity_)) {
            auto* ps = reg.try_get<ParticleSystem3DComponent>(emitter_entity_);
            if (ps) {
                // 渐出 emission_rate
                current_emission_rate_ = Lerp(current_emission_rate_, 0.0f,
                    std::min(1.0f, 2.0f * delta_time));
                ps->emission_rate = current_emission_rate_;
                if (current_emission_rate_ < 0.5f) {
                    ps->enabled = false;
                    current_emission_rate_ = 0.0f;
                    transition_initialized_ = false;
                }
            }
        }
        // 重置 dirty 缓存
        last_type_ = WeatherType::None;
        last_intensity_ = -1.0f;
        return;
    }

    // 找相机位置
    glm::vec3 cam_pos{0.0f};
    auto cam_view = reg.view<Camera3DComponent, TransformComponent>();
    for (auto e : cam_view) {
        if (cam_view.get<Camera3DComponent>(e).enabled) {
            cam_pos = cam_view.get<TransformComponent>(e).position;
            break;
        }
    }

    // 创建发射器实体（仅一次）
    if (emitter_entity_ == entt::null || !reg.valid(emitter_entity_)) {
        emitter_entity_ = reg.create();
        reg.emplace<TransformComponent>(emitter_entity_);
        reg.emplace<ParticleSystem3DComponent>(emitter_entity_);
    }

    // 每帧跟随相机
    auto& tf = reg.get<TransformComponent>(emitter_entity_);
    tf.position = cam_pos + glm::vec3(0.0f, weather->spawn_height, 0.0f);

    auto& ps = reg.get<ParticleSystem3DComponent>(emitter_entity_);
    ps.enabled       = true;
    ps.max_particles = weather->max_particles;
    ps.spawn_radius  = weather->spawn_radius;
    ps.start_speed_min = 0.0f;
    ps.start_speed_max = 0.3f;

    // 纹理
    const bool is_snow = (weather->type == WeatherType::Snow);
    const std::string& tex = is_snow ? weather->snow_texture_path
                                     : weather->rain_texture_path;
    if (ps.texture_path != tex) {
        ps.texture_path   = tex;
        ps.texture_handle = 0; // 强制粒子系统重新加载
    }

    // Dirty check — 仅在参数变化时计算新目标
    bool dirty = IsDirty(*weather, last_type_, last_intensity_,
                         last_wind_x_, last_wind_z_,
                         last_spawn_r_, last_spawn_h_, last_max_p_);
    if (dirty) {
        last_type_      = weather->type;
        last_intensity_ = weather->intensity;
        last_wind_x_    = weather->wind_x;
        last_wind_z_    = weather->wind_z;
        last_spawn_r_   = weather->spawn_radius;
        last_spawn_h_   = weather->spawn_height;
        last_max_p_     = weather->max_particles;
    }

    // 计算目标值
    WeatherTarget target = ComputeTarget(*weather);

    // 首次初始化 → 直接跳到目标，不过渡
    if (!transition_initialized_) {
        current_emission_rate_ = target.emission_rate;
        current_color_         = target.color;
        current_gravity_       = target.gravity;
        current_size_min_      = target.size_min;
        current_size_max_      = target.size_max;
        current_life_min_      = target.life_min;
        current_life_max_      = target.life_max;
        transition_initialized_ = true;
    } else {
        // 平滑过渡
        float t = std::min(1.0f, weather->transition_speed * delta_time);
        current_emission_rate_ = Lerp(current_emission_rate_, target.emission_rate, t);
        current_color_         = LerpVec4(current_color_,     target.color,         t);
        current_gravity_       = LerpVec3(current_gravity_,   target.gravity,       t);
        current_size_min_      = Lerp(current_size_min_,      target.size_min,      t);
        current_size_max_      = Lerp(current_size_max_,      target.size_max,      t);
        current_life_min_      = Lerp(current_life_min_,      target.life_min,      t);
        current_life_max_      = Lerp(current_life_max_,      target.life_max,      t);
    }

    // 写入粒子系统
    ps.emission_rate   = current_emission_rate_;
    ps.start_color     = current_color_;
    ps.gravity         = current_gravity_;
    ps.start_size_min  = current_size_min_;
    ps.start_size_max  = current_size_max_;
    ps.start_life_min  = current_life_min_;
    ps.start_life_max  = current_life_max_;
}

void WeatherSystem::Shutdown(World& world) {
    if (emitter_entity_ != entt::null && world.registry().valid(emitter_entity_)) {
        world.registry().destroy(emitter_entity_);
        emitter_entity_ = entt::null;
    }
    transition_initialized_ = false;
}

} // namespace gameplay3d
} // namespace dse

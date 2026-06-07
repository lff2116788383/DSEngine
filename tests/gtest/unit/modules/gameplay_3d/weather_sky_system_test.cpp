/**
 * @file weather_sky_system_test.cpp
 * @brief WeatherSystem / SnowCoverSystem / DayNightCycleSystem 无 GPU 单元测试
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_snow.h"
#include "engine/ecs/components_3d_particle.h"
#include "modules/gameplay_3d/weather/weather_system.h"
#include "modules/gameplay_3d/snow/snow_cover_system.h"
#include "modules/gameplay_3d/sky/day_night_cycle_system.h"

using namespace dse;
using namespace dse::gameplay3d;

namespace {
template<typename... Ts>
size_t CountView(entt::registry& reg) {
    size_t n = 0;
    for (auto _ : reg.view<Ts...>()) { (void)_; ++n; }
    return n;
}
} // namespace

// ============================================================
//  WeatherSystem
// ============================================================

TEST(WeatherSystemTest, 无天气组件不创建发射器) {
    WeatherSystem sys;
    World world;
    sys.Update(world, 0.016f);
    EXPECT_EQ(CountView<ParticleSystem3DComponent>(world.registry()), 0u);
}

TEST(WeatherSystemTest, 类型None不创建发射器) {
    WeatherSystem sys;
    World world;
    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::None;
    wc.enabled = true;

    sys.Update(world, 0.016f);

    EXPECT_EQ(CountView<ParticleSystem3DComponent>(world.registry()), 0u);
}

TEST(WeatherSystemTest, 雪天气创建发射器并设置参数) {
    WeatherSystem sys;
    World world;

    auto cam = world.registry().create();
    world.registry().emplace<Camera3DComponent>(cam).enabled = true;
    world.registry().emplace<TransformComponent>(cam).position = glm::vec3(0.0f);

    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    wc.enabled = true;

    sys.Update(world, 0.016f);

    EXPECT_GE(CountView<ParticleSystem3DComponent>(world.registry()), 1u);

    bool found = false;
    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto pe : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(pe);
        if (!ps.enabled) continue;
        found = true;
        // Snow: emission_rate = 300 * intensity
        EXPECT_NEAR(ps.emission_rate, 300.0f * wc.intensity, 1.0f);
        // Snow: gravity.y ≈ -1.8
        EXPECT_NEAR(ps.gravity.y, -1.8f, 0.01f);
        // Snow: 较大粒子
        EXPECT_GE(ps.start_size_max, 0.10f);
        break;
    }
    EXPECT_TRUE(found);
}

TEST(WeatherSystemTest, 雨天气参数与雪不同) {
    WeatherSystem sys;
    World world;

    auto cam = world.registry().create();
    world.registry().emplace<Camera3DComponent>(cam).enabled = true;
    world.registry().emplace<TransformComponent>(cam);

    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::Rain;
    wc.intensity = 1.0f;
    wc.enabled = true;

    sys.Update(world, 0.016f);

    bool found = false;
    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto pe : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(pe);
        if (!ps.enabled) continue;
        found = true;
        // Rain: emission_rate = 800 * intensity
        EXPECT_NEAR(ps.emission_rate, 800.0f * wc.intensity, 1.0f);
        // Rain: gravity.y ≈ -20
        EXPECT_NEAR(ps.gravity.y, -20.0f, 0.01f);
        // Rain: 较小粒子
        EXPECT_LE(ps.start_size_max, 0.10f);
        break;
    }
    EXPECT_TRUE(found);
}

TEST(WeatherSystemTest, 禁用天气后发射率衰减) {
    WeatherSystem sys;
    World world;

    auto cam = world.registry().create();
    world.registry().emplace<Camera3DComponent>(cam).enabled = true;
    world.registry().emplace<TransformComponent>(cam);

    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    wc.enabled = true;

    sys.Update(world, 0.016f);

    // 禁用天气
    wc.enabled = false;

    // 大量帧持续衰减（lerp 收敛慢，需要足够多帧）
    for (int i = 0; i < 600; ++i)
        sys.Update(world, 0.016f);

    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto pe : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(pe);
        EXPECT_LT(ps.emission_rate, 5.0f);
    }
}

TEST(WeatherSystemTest, 过渡平滑多帧趋近) {
    WeatherSystem sys;
    World world;

    auto cam = world.registry().create();
    world.registry().emplace<Camera3DComponent>(cam).enabled = true;
    world.registry().emplace<TransformComponent>(cam);

    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::Rain;
    wc.intensity = 1.0f;
    wc.enabled = true;
    wc.transition_speed = 2.0f;

    // 首帧直接跳到目标
    sys.Update(world, 0.016f);

    // 切换到雪
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    const float snow_target_rate = 300.0f;

    // 第1帧不应立即到达目标（从 800 过渡到 300）
    sys.Update(world, 0.016f);

    auto view = world.registry().view<ParticleSystem3DComponent>();
    for (auto pe : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(pe);
        if (!ps.enabled) continue;
        // 应处于 800→300 的中间态
        EXPECT_GT(ps.emission_rate, snow_target_rate + 1.0f);
        break;
    }

    // 多帧后应趋近
    for (int i = 0; i < 200; ++i)
        sys.Update(world, 0.016f);

    for (auto pe : view) {
        auto& ps = view.get<ParticleSystem3DComponent>(pe);
        if (!ps.enabled) continue;
        EXPECT_NEAR(ps.emission_rate, snow_target_rate, 5.0f);
        break;
    }
}

TEST(WeatherSystemTest, Shutdown销毁发射器) {
    WeatherSystem sys;
    World world;

    auto cam = world.registry().create();
    world.registry().emplace<Camera3DComponent>(cam).enabled = true;
    world.registry().emplace<TransformComponent>(cam);

    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    wc.enabled = true;

    sys.Update(world, 0.016f);

    // 确认发射器存在
    EXPECT_GE(CountView<ParticleSystem3DComponent>(world.registry()), 1u);

    sys.Shutdown(world);

    // 发射器实体被销毁后不再持有 ParticleSystem3DComponent
    EXPECT_EQ(CountView<ParticleSystem3DComponent>(world.registry()), 0u);
}

TEST(WeatherSystemTest, 发射器跟随相机位置) {
    WeatherSystem sys;
    World world;

    auto cam = world.registry().create();
    world.registry().emplace<Camera3DComponent>(cam).enabled = true;
    auto& cam_tf = world.registry().emplace<TransformComponent>(cam);
    cam_tf.position = glm::vec3(100.0f, 5.0f, -50.0f);

    auto e = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(e);
    wc.type = WeatherType::Rain;
    wc.intensity = 0.5f;
    wc.enabled = true;
    wc.spawn_height = 20.0f;

    sys.Update(world, 0.016f);

    auto view = world.registry().view<ParticleSystem3DComponent, TransformComponent>();
    for (auto pe : view) {
        auto& tf = view.get<TransformComponent>(pe);
        EXPECT_FLOAT_EQ(tf.position.x, 100.0f);
        EXPECT_FLOAT_EQ(tf.position.y, 5.0f + 20.0f);
        EXPECT_FLOAT_EQ(tf.position.z, -50.0f);
        break;
    }
}

// ============================================================
//  SnowCoverSystem
// ============================================================

TEST(SnowCoverSystemTest, 无天气时雪覆盖融化趋零) {
    SnowCoverSystem sys;
    World world;

    auto e = world.registry().create();
    auto& sc = world.registry().emplace<SnowCoverComponent>(e);
    sc.coverage = 0.5f;
    sc.melt_rate = 0.02f;

    // 无 WeatherComponent → target_coverage = 0, melt_rate=0.02/s
    // 0.5 / 0.02 = 25s → 需要至少 250 帧 * 0.1s
    for (int i = 0; i < 300; ++i)
        sys.Update(world, 0.1f);

    EXPECT_NEAR(sc.coverage, 0.0f, 0.001f);
}

TEST(SnowCoverSystemTest, 雪天气积雪累积) {
    SnowCoverSystem sys;
    World world;

    auto we = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(we);
    wc.type = WeatherType::Snow;
    wc.intensity = 0.8f;
    wc.enabled = true;

    auto e = world.registry().create();
    auto& sc = world.registry().emplace<SnowCoverComponent>(e);
    sc.coverage = 0.0f;
    sc.accumulation_rate = 0.08f;

    // 更新若干帧
    for (int i = 0; i < 50; ++i)
        sys.Update(world, 0.1f);

    EXPECT_GT(sc.coverage, 0.0f);
    // target_coverage 应等于 intensity
    EXPECT_FLOAT_EQ(sc.target_coverage, 0.8f);
}

TEST(SnowCoverSystemTest, 覆盖率钳制到零一区间) {
    SnowCoverSystem sys;
    World world;

    auto we = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(we);
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    wc.enabled = true;

    auto e = world.registry().create();
    auto& sc = world.registry().emplace<SnowCoverComponent>(e);
    sc.coverage = 0.0f;
    sc.accumulation_rate = 10.0f; // 极高速率

    sys.Update(world, 1.0f);

    EXPECT_LE(sc.coverage, 1.0f);
    EXPECT_GE(sc.coverage, 0.0f);
}

TEST(SnowCoverSystemTest, 禁用组件被跳过) {
    SnowCoverSystem sys;
    World world;

    auto we = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(we);
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    wc.enabled = true;

    auto e = world.registry().create();
    auto& sc = world.registry().emplace<SnowCoverComponent>(e);
    sc.enabled = false;
    sc.coverage = 0.0f;

    sys.Update(world, 1.0f);

    EXPECT_FLOAT_EQ(sc.coverage, 0.0f);
}

TEST(SnowCoverSystemTest, 停雪后融化速率正确) {
    SnowCoverSystem sys;
    World world;

    auto we = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(we);
    wc.type = WeatherType::Snow;
    wc.intensity = 1.0f;
    wc.enabled = true;

    auto e = world.registry().create();
    auto& sc = world.registry().emplace<SnowCoverComponent>(e);
    sc.coverage = 0.0f;
    sc.accumulation_rate = 1.0f; // 快速积雪到满

    // 积雪至满
    for (int i = 0; i < 20; ++i)
        sys.Update(world, 0.1f);
    EXPECT_NEAR(sc.coverage, 1.0f, 0.01f);

    // 停止下雪
    wc.type = WeatherType::None;
    sc.melt_rate = 0.1f;

    float before = sc.coverage;
    sys.Update(world, 1.0f);
    float after = sc.coverage;

    // 融化量 ≈ melt_rate * dt = 0.1
    EXPECT_NEAR(before - after, 0.1f, 0.02f);
}

TEST(SnowCoverSystemTest, 雨天不积雪) {
    SnowCoverSystem sys;
    World world;

    auto we = world.registry().create();
    auto& wc = world.registry().emplace<WeatherComponent>(we);
    wc.type = WeatherType::Rain;
    wc.intensity = 1.0f;
    wc.enabled = true;

    auto e = world.registry().create();
    auto& sc = world.registry().emplace<SnowCoverComponent>(e);
    sc.coverage = 0.3f;
    sc.melt_rate = 0.02f;

    // Rain 不算 snowing → target=0 → 应融化
    for (int i = 0; i < 50; ++i)
        sys.Update(world, 0.1f);

    EXPECT_LT(sc.coverage, 0.3f);
}

// ============================================================
//  DayNightCycleSystem
// ============================================================

TEST(DayNightCycleSystemTest, 正午太阳大致朝上) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 12.0f;
    cycle.latitude = 30.0f;
    cycle.day_of_year = 172; // 夏至附近
    cycle.auto_advance = false;

    auto le = world.registry().create();
    world.registry().emplace<DirectionalLight3DComponent>(le);

    sys.Update(world, 0.0f);

    EXPECT_GT(cycle.sun_direction_.y, 0.5f);
    EXPECT_GT(cycle.sun_elevation_, 50.0f);
}

TEST(DayNightCycleSystemTest, 午夜太阳在地平线下) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 0.0f; // 午夜
    cycle.latitude = 30.0f;
    cycle.day_of_year = 172;
    cycle.auto_advance = false;

    auto le = world.registry().create();
    world.registry().emplace<DirectionalLight3DComponent>(le);

    sys.Update(world, 0.0f);

    EXPECT_LT(cycle.sun_elevation_, 0.0f);
    EXPECT_LT(cycle.sun_direction_.y, 0.0f);
}

TEST(DayNightCycleSystemTest, 自动推进时间) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 6.0f;
    cycle.time_speed = 3600.0f; // 1秒 = 1小时
    cycle.auto_advance = true;

    auto le = world.registry().create();
    world.registry().emplace<DirectionalLight3DComponent>(le);

    float before = cycle.time_of_day;
    sys.Update(world, 1.0f); // dt=1s → +1h
    float after = cycle.time_of_day;

    EXPECT_NEAR(after - before, 1.0f, 0.01f);
}

TEST(DayNightCycleSystemTest, 禁用组件被跳过) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = false;
    cycle.time_of_day = 6.0f;
    cycle.auto_advance = true;
    cycle.time_speed = 3600.0f;

    auto le = world.registry().create();
    world.registry().emplace<DirectionalLight3DComponent>(le);

    sys.Update(world, 1.0f);

    // 时间不应前进
    EXPECT_FLOAT_EQ(cycle.time_of_day, 6.0f);
}

TEST(DayNightCycleSystemTest, 方向光接收太阳方向和颜色) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 12.0f;
    cycle.auto_advance = false;

    auto le = world.registry().create();
    auto& light = world.registry().emplace<DirectionalLight3DComponent>(le);
    light.direction = glm::vec3(0.0f);

    sys.Update(world, 0.0f);

    // direction 应为 -sun_direction_
    glm::vec3 expected_dir = -cycle.sun_direction_;
    EXPECT_NEAR(light.direction.x, expected_dir.x, 0.001f);
    EXPECT_NEAR(light.direction.y, expected_dir.y, 0.001f);
    EXPECT_NEAR(light.direction.z, expected_dir.z, 0.001f);

    // 正午色温接近白色
    EXPECT_GT(light.color.r, 0.8f);
    EXPECT_GT(light.color.g, 0.8f);
    EXPECT_GT(light.color.b, 0.7f);
}

TEST(DayNightCycleSystemTest, 时间自动回绕24小时) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 23.5f;
    cycle.time_speed = 3600.0f;
    cycle.auto_advance = true;

    auto le = world.registry().create();
    world.registry().emplace<DirectionalLight3DComponent>(le);

    sys.Update(world, 2.0f); // +2h → 23.5+2=25.5 → wraps to 1.5

    EXPECT_GE(cycle.time_of_day, 0.0f);
    EXPECT_LT(cycle.time_of_day, 24.0f);
    EXPECT_NEAR(cycle.time_of_day, 1.5f, 0.01f);
}

TEST(DayNightCycleSystemTest, 夜间光照强度降低) {
    DayNightCycleSystem sys;
    World world;

    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 0.0f; // 午夜
    cycle.auto_advance = false;

    auto le = world.registry().create();
    auto& light = world.registry().emplace<DirectionalLight3DComponent>(le);

    sys.Update(world, 0.0f);

    // 夜间 intensity 应接近 0
    EXPECT_LT(light.intensity, 0.2f);
}

TEST(DayNightCycleSystemTest, 同实体上方向光优先使用) {
    DayNightCycleSystem sys;
    World world;

    // 在同一实体上同时挂 DayNightCycleComponent 和 DirectionalLight3DComponent
    auto e = world.registry().create();
    auto& cycle = world.registry().emplace<DayNightCycleComponent>(e);
    cycle.enabled = true;
    cycle.time_of_day = 12.0f;
    cycle.auto_advance = false;
    auto& light = world.registry().emplace<DirectionalLight3DComponent>(e);

    // 另一个独立方向光
    auto other = world.registry().create();
    auto& other_light = world.registry().emplace<DirectionalLight3DComponent>(other);
    glm::vec3 sentinel(99.0f, 99.0f, 99.0f);
    other_light.direction = sentinel;

    sys.Update(world, 0.0f);

    // 同实体的 light 被修改
    EXPECT_NE(light.direction.x, 0.0f);
    // 另一个方向光不应被改动
    EXPECT_FLOAT_EQ(other_light.direction.x, sentinel.x);
}

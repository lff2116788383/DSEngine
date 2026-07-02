/**
 * @file light_buffer_test.cpp
 * @brief LightBuffer CPU 侧 CollectLightsFromView + GPU 结构体布局测试（无 GPU 依赖）
 */

#include <gtest/gtest.h>
#include "engine/render/light_buffer.h"
#include "engine/render/render_scene_view.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;
using namespace dse::render;

class LightBufferCollectTest : public ::testing::Test {
protected:
    void SetUp() override {
        world_ = std::make_unique<World>();
        buf_.Init(nullptr);
    }

    void TearDown() override {
        buf_.Shutdown();
        world_.reset();
    }

    void Collect() {
        view_.Clear();
        ExtractRenderSceneView(*world_, view_);
        buf_.CollectLightsFromView(view_);
    }

    std::unique_ptr<World> world_;
    RenderSceneView view_;
    LightBuffer buf_;
};

// 验证 GPU 结构体布局
// 测试 灯光缓冲区收集：GPU点灯光布局
TEST_F(LightBufferCollectTest, GPUPointLightLayout) {
    static_assert(sizeof(GPUPointLight) == 48, "GPUPointLight must be 48 bytes");
}

// 测试 灯光缓冲区收集：GPU聚光灯光布局
TEST_F(LightBufferCollectTest, GPUSpotLightLayout) {
    static_assert(sizeof(GPUSpotLight) == 64, "GPUSpotLight must be 64 bytes");
}

// 测试 灯光缓冲区收集：灯光缓冲区头布局
TEST_F(LightBufferCollectTest, LightBufferHeaderLayout) {
    static_assert(sizeof(LightBufferHeader) == 16, "LightBufferHeader must be 16 bytes");
}

// 验证空场景收集无光源
// 测试 灯光缓冲区收集：收集空世界
TEST_F(LightBufferCollectTest, CollectEmptyWorld) {
    Collect();
    EXPECT_EQ(buf_.point_light_count(), 0);
    EXPECT_EQ(buf_.spot_light_count(), 0);
}

// 验证收集点光源
// 测试 灯光缓冲区收集：收集点灯光
TEST_F(LightBufferCollectTest, CollectPointLights) {
    auto& reg = world_->registry();

    for (int i = 0; i < 3; ++i) {
        auto entity = reg.create();
        auto& transform = reg.emplace<TransformComponent>(entity);
        transform.position = glm::vec3(static_cast<float>(i) * 10.0f, 0.0f, 0.0f);
        auto& light = reg.emplace<PointLightComponent>(entity);
        light.color = glm::vec3(1.0f);
        light.intensity = 2.0f;
        light.radius = 50.0f;
        light.enabled = true;
        light.cast_shadow = (i == 0);
    }

    Collect();

    EXPECT_EQ(buf_.point_light_count(), 3);
    EXPECT_EQ(buf_.spot_light_count(), 0);

    const auto& lights = buf_.point_lights();
    EXPECT_FLOAT_EQ(lights[0].intensity, 2.0f);
    EXPECT_FLOAT_EQ(lights[0].color.r, 1.0f);
}

// 验证收集聚光灯
// 测试 灯光缓冲区收集：收集聚光灯光
TEST_F(LightBufferCollectTest, CollectSpotLights) {
    auto& reg = world_->registry();

    auto entity = reg.create();
    auto& transform = reg.emplace<TransformComponent>(entity);
    transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    auto& light = reg.emplace<SpotLightComponent>(entity);
    light.color = glm::vec3(0.0f, 1.0f, 0.0f);
    light.intensity = 5.0f;
    light.radius = 30.0f;
    light.inner_cone_angle = 0.9f;
    light.outer_cone_angle = 0.8f;
    light.enabled = true;

    Collect();

    EXPECT_EQ(buf_.point_light_count(), 0);
    EXPECT_EQ(buf_.spot_light_count(), 1);

    const auto& spots = buf_.spot_lights();
    EXPECT_FLOAT_EQ(spots[0].intensity, 5.0f);
    EXPECT_FLOAT_EQ(spots[0].inner_cone, 0.9f);
    EXPECT_FLOAT_EQ(spots[0].outer_cone, 0.8f);
}

// 验证 disabled 光源被跳过
// 测试 灯光缓冲区收集：跳过禁用灯光
TEST_F(LightBufferCollectTest, SkipDisabledLights) {
    auto& reg = world_->registry();

    auto e1 = reg.create();
    reg.emplace<TransformComponent>(e1);
    auto& l1 = reg.emplace<PointLightComponent>(e1);
    l1.enabled = false;

    auto e2 = reg.create();
    reg.emplace<TransformComponent>(e2);
    auto& l2 = reg.emplace<PointLightComponent>(e2);
    l2.enabled = true;

    Collect();

    EXPECT_EQ(buf_.point_light_count(), 1);
}

// 验证阴影索引分配（最多 4 个）
// 测试 灯光缓冲区收集：阴影索引Limit
TEST_F(LightBufferCollectTest, ShadowIndexLimit) {
    auto& reg = world_->registry();

    for (int i = 0; i < 6; ++i) {
        auto entity = reg.create();
        reg.emplace<TransformComponent>(entity);
        auto& light = reg.emplace<PointLightComponent>(entity);
        light.enabled = true;
        light.cast_shadow = true;
        light.radius = 10.0f;
    }

    Collect();

    EXPECT_EQ(buf_.point_light_count(), 6);
    int shadow_count = 0;
    for (const auto& l : buf_.point_lights()) {
        if (l.cast_shadow) shadow_count++;
    }
    EXPECT_EQ(shadow_count, 4);
}

// CollectLightsFromView 每帧清空重新收集
// 测试 灯光缓冲区收集：收集清空先前帧
TEST_F(LightBufferCollectTest, CollectClearsPreviousFrame) {
    auto& reg = world_->registry();

    auto entity = reg.create();
    reg.emplace<TransformComponent>(entity);
    reg.emplace<PointLightComponent>(entity).enabled = true;

    Collect();
    EXPECT_EQ(buf_.point_light_count(), 1);

    reg.destroy(entity);
    Collect();
    EXPECT_EQ(buf_.point_light_count(), 0);
}

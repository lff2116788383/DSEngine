/**
 * @file frustum_culling_system_test.cpp
 * @brief FrustumCullingSystem 视锥体剔除系统的单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 无 MeshRenderer 的实体不影响
 * - BoundingBoxComponent center/extents 计算
 * - 带实体但无 BoundingBox 不崩溃
 * - Camera3DComponent 默认值
 * - DirectionalLight3DComponent 默认值
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/frustum_culling_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"

using namespace dse;
using namespace gameplay3d;

class FrustumCullingSystemTest : public ::testing::Test {
protected:
    World world;
    FrustumCullingSystem sys;
};

// 测试 视锥剔除系统：空世界调用更新不崩溃
TEST_F(FrustumCullingSystemTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world));
}

// 测试 视锥剔除系统：带有实体世界调用更新不崩溃
TEST_F(FrustumCullingSystemTest, BringEntityWorldCallsUpdateDoesNotCrash) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    EXPECT_NO_THROW(sys.Update(world));
}

// 测试 视锥剔除系统：带有网格渲染器实体更新不崩溃
TEST_F(FrustumCullingSystemTest, BringMeshRendererEntityUpdateDoesNotCrash) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<MeshRendererComponent>(e);
    EXPECT_NO_THROW(sys.Update(world));
}

// 测试 视锥剔除系统：包围盒Componentcentral Computing
TEST_F(FrustumCullingSystemTest, BoundingBoxComponentcentralComputing) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-1.0f, -2.0f, -3.0f);
    bb.max_extents = glm::vec3(1.0f, 2.0f, 3.0f);

    glm::vec3 center = bb.center();
    EXPECT_FLOAT_EQ(center.x, 0.0f);
    EXPECT_FLOAT_EQ(center.y, 0.0f);
    EXPECT_FLOAT_EQ(center.z, 0.0f);
}

// 测试 视锥剔除系统：包围盒Componentasymmetric中心
TEST_F(FrustumCullingSystemTest, BoundingBoxComponentasymmetricCenter) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(0.0f, 0.0f, 0.0f);
    bb.max_extents = glm::vec3(2.0f, 4.0f, 6.0f);

    glm::vec3 center = bb.center();
    EXPECT_FLOAT_EQ(center.x, 1.0f);
    EXPECT_FLOAT_EQ(center.y, 2.0f);
    EXPECT_FLOAT_EQ(center.z, 3.0f);
}

// 测试 视锥剔除系统：包围盒Componenthalf范围Calculation
TEST_F(FrustumCullingSystemTest, BoundingBoxComponenthalfRangeCalculation) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-2.0f, -4.0f, -6.0f);
    bb.max_extents = glm::vec3(2.0f, 4.0f, 6.0f);

    glm::vec3 extents = bb.extents();
    EXPECT_FLOAT_EQ(extents.x, 2.0f);
    EXPECT_FLOAT_EQ(extents.y, 4.0f);
    EXPECT_FLOAT_EQ(extents.z, 6.0f);
}

// 测试 视锥剔除系统：相机3D组件默认值
TEST_F(FrustumCullingSystemTest, Camera3DComponentDefaultValues) {
    Camera3DComponent cam;
    EXPECT_TRUE(cam.enabled);
    EXPECT_EQ(cam.priority, 0);
    EXPECT_FLOAT_EQ(cam.fov, 60.0f);
    EXPECT_FLOAT_EQ(cam.aspect_ratio, 1.333f);
    EXPECT_FLOAT_EQ(cam.near_clip, 0.1f);
    EXPECT_FLOAT_EQ(cam.far_clip, 1000.0f);
}

// 测试 视锥剔除系统：方向光灯光3D组件默认值
TEST_F(FrustumCullingSystemTest, DirectionalLight3DComponentDefaultValues) {
    DirectionalLight3DComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_TRUE(light.cast_shadow);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_FLOAT_EQ(light.ambient_intensity, 0.2f);
    EXPECT_FLOAT_EQ(light.shadow_strength, 0.35f);
}

// 测试 视锥剔除系统：多次数更新不崩溃
TEST_F(FrustumCullingSystemTest, MultiTimesUpdateDoesNotCrash) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<MeshRendererComponent>(e);
    world.registry().emplace<BoundingBoxComponent>(e);

    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(sys.Update(world));
    }
}

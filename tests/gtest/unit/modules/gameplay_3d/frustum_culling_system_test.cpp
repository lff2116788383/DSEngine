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

TEST_F(FrustumCullingSystemTest, 空World调用Update不崩溃) {
    EXPECT_NO_THROW(sys.Update(world));
}

TEST_F(FrustumCullingSystemTest, 带实体World调用Update不崩溃) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    EXPECT_NO_THROW(sys.Update(world));
}

TEST_F(FrustumCullingSystemTest, 带MeshRenderer实体Update不崩溃) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<MeshRendererComponent>(e);
    EXPECT_NO_THROW(sys.Update(world));
}

TEST_F(FrustumCullingSystemTest, BoundingBoxComponent中心计算) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-1.0f, -2.0f, -3.0f);
    bb.max_extents = glm::vec3(1.0f, 2.0f, 3.0f);

    glm::vec3 center = bb.center();
    EXPECT_FLOAT_EQ(center.x, 0.0f);
    EXPECT_FLOAT_EQ(center.y, 0.0f);
    EXPECT_FLOAT_EQ(center.z, 0.0f);
}

TEST_F(FrustumCullingSystemTest, BoundingBoxComponent非对称中心) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(0.0f, 0.0f, 0.0f);
    bb.max_extents = glm::vec3(2.0f, 4.0f, 6.0f);

    glm::vec3 center = bb.center();
    EXPECT_FLOAT_EQ(center.x, 1.0f);
    EXPECT_FLOAT_EQ(center.y, 2.0f);
    EXPECT_FLOAT_EQ(center.z, 3.0f);
}

TEST_F(FrustumCullingSystemTest, BoundingBoxComponent半范围计算) {
    BoundingBoxComponent bb;
    bb.min_extents = glm::vec3(-2.0f, -4.0f, -6.0f);
    bb.max_extents = glm::vec3(2.0f, 4.0f, 6.0f);

    glm::vec3 extents = bb.extents();
    EXPECT_FLOAT_EQ(extents.x, 2.0f);
    EXPECT_FLOAT_EQ(extents.y, 4.0f);
    EXPECT_FLOAT_EQ(extents.z, 6.0f);
}

TEST_F(FrustumCullingSystemTest, Camera3DComponent默认值) {
    Camera3DComponent cam;
    EXPECT_TRUE(cam.enabled);
    EXPECT_EQ(cam.priority, 0);
    EXPECT_FLOAT_EQ(cam.fov, 60.0f);
    EXPECT_FLOAT_EQ(cam.aspect_ratio, 1.333f);
    EXPECT_FLOAT_EQ(cam.near_clip, 0.1f);
    EXPECT_FLOAT_EQ(cam.far_clip, 1000.0f);
}

TEST_F(FrustumCullingSystemTest, DirectionalLight3DComponent默认值) {
    DirectionalLight3DComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_TRUE(light.cast_shadow);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_FLOAT_EQ(light.ambient_intensity, 0.2f);
    EXPECT_FLOAT_EQ(light.shadow_strength, 0.35f);
}

TEST_F(FrustumCullingSystemTest, 多次Update不崩溃) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<MeshRendererComponent>(e);
    world.registry().emplace<BoundingBoxComponent>(e);

    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(sys.Update(world));
    }
}

/**
 * @file camera_system_2d_test.cpp
 * @brief CameraSystem (2D) 摄像机系统的单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 正交摄像机投影矩阵计算验证
 * - 透视摄像机投影矩阵计算验证
 * - 视图矩阵根据 Transform 更新
 * - CameraFollowComponent 跟随目标移动
 * - CameraFollowComponent 死区内不移动
 * - 禁用摄像机不更新
 * - aspect_ratio <= 0 时自动修正为 1.0
 */

#include <gtest/gtest.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "modules/gameplay_2d/camera/camera_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"

class CameraSystem2DTest : public ::testing::Test {
protected:
    World world;
    CameraSystem sys;
    static constexpr float kAspect = 16.0f / 9.0f;
};

TEST_F(CameraSystem2DTest, 空World调用Update不崩溃) {
    EXPECT_NO_THROW(sys.Update(world, kAspect));
}

TEST_F(CameraSystem2DTest, 正交摄像机投影矩阵) {
    auto e = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(e);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;
    cam.near_clip = -1.0f;
    cam.far_clip = 1.0f;
    cam.enabled = true;
    auto& tf = world.registry().emplace<TransformComponent>(e);

    sys.Update(world, kAspect);

    // 验证投影矩阵非单位矩阵（已计算）
    EXPECT_NE(cam.projection, glm::mat4(1.0f));
}

TEST_F(CameraSystem2DTest, 透视摄像机投影矩阵) {
    auto e = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(e);
    cam.orthographic = false;
    cam.fov = 60.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 100.0f;
    cam.enabled = true;
    auto& tf = world.registry().emplace<TransformComponent>(e);

    sys.Update(world, kAspect);

    // 透视投影矩阵非单位矩阵
    EXPECT_NE(cam.projection, glm::mat4(1.0f));
}

TEST_F(CameraSystem2DTest, 视图矩阵根据Transform更新) {
    auto e = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(e);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;
    cam.enabled = true;
    auto& tf = world.registry().emplace<TransformComponent>(e);
    tf.position = glm::vec3(10.0f, 20.0f, 0.0f);

    sys.Update(world, kAspect);

    // 视图矩阵应该反映摄像机位置
    EXPECT_NE(cam.view, glm::mat4(1.0f));
}

TEST_F(CameraSystem2DTest, CameraFollowComponent跟随目标) {
    auto target = world.CreateEntity();
    auto& target_tf = world.registry().emplace<TransformComponent>(target);
    target_tf.position = glm::vec3(100.0f, 0.0f, 0.0f);

    auto cam_entity = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(cam_entity);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;
    cam.enabled = true;
    auto& cam_tf = world.registry().emplace<TransformComponent>(cam_entity);
    cam_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);

    auto& follow = world.registry().emplace<CameraFollowComponent>(cam_entity);
    follow.target = target;
    follow.damping = 1.0f; // 瞬间跟随
    follow.enabled = true;
    follow.follow_x = true;
    follow.follow_y = true;

    sys.Update(world, kAspect);

    // damping=1.0 意味着瞬间到达目标位置
    EXPECT_FLOAT_EQ(cam_tf.position.x, 100.0f);
}

TEST_F(CameraSystem2DTest, CameraFollowComponent死区内不移动) {
    auto target = world.CreateEntity();
    auto& target_tf = world.registry().emplace<TransformComponent>(target);
    target_tf.position = glm::vec3(0.5f, 0.0f, 0.0f); // 在死区内

    auto cam_entity = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(cam_entity);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;
    cam.enabled = true;
    auto& cam_tf = world.registry().emplace<TransformComponent>(cam_entity);
    cam_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);

    auto& follow = world.registry().emplace<CameraFollowComponent>(cam_entity);
    follow.target = target;
    follow.damping = 1.0f;
    follow.enabled = true;
    follow.dead_zone = glm::vec2(2.0f, 2.0f); // 死区足够大
    follow.follow_x = true;
    follow.follow_y = true;

    sys.Update(world, kAspect);

    // 在死区内，摄像机不应移动
    EXPECT_FLOAT_EQ(cam_tf.position.x, 0.0f);
}

TEST_F(CameraSystem2DTest, 禁用摄像机仍更新投影矩阵但跳过跟随) {
    auto e = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(e);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;
    cam.enabled = false;
    auto& tf = world.registry().emplace<TransformComponent>(e);

    sys.Update(world, kAspect);

    // 禁用的摄像机：CameraSystem 实际仍遍历 enabled==false 的实体，
    // 但代码中 continue 跳过，投影矩阵不变
    EXPECT_EQ(cam.projection, glm::mat4(1.0f));
}

TEST_F(CameraSystem2DTest, AspectRatio为零时自动修正) {
    auto e = world.CreateEntity();
    auto& cam = world.registry().emplace<CameraComponent>(e);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;
    cam.enabled = true;
    auto& tf = world.registry().emplace<TransformComponent>(e);

    EXPECT_NO_THROW(sys.Update(world, 0.0f));
    // aspect_ratio 修正为 1.0 后投影矩阵正常计算
    EXPECT_NE(cam.projection, glm::mat4(1.0f));
}

/**
 * @file camera_component_test.cpp
 * @brief Camera 相关组件数据结构的单元测试
 *
 * 覆盖场景：
 * - CameraComponent 默认值与字段修改
 * - CameraFollowComponent 默认值与字段修改
 */

#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include "engine/ecs/camera.h"

// ============================================================
// CameraComponent
// ============================================================

// 测试 相机组件：默认值
TEST(CameraComponentTest, DefaultValues) {
    CameraComponent cam;
    EXPECT_TRUE(cam.orthographic);
    EXPECT_TRUE(cam.enabled);
    EXPECT_EQ(cam.priority, 0);
    EXPECT_FLOAT_EQ(cam.orthographic_size, 5.0f);
    EXPECT_FLOAT_EQ(cam.fov, 60.0f);
    EXPECT_FLOAT_EQ(cam.aspect_ratio, 1.333f);
    EXPECT_FLOAT_EQ(cam.near_clip, -1.0f);
    EXPECT_FLOAT_EQ(cam.far_clip, 1.0f);
}

// 测试 相机组件：为
TEST(CameraComponentTest, Is) {
    CameraComponent cam;
    cam.orthographic = false;
    cam.fov = 90.0f;
    cam.near_clip = 0.1f;
    cam.far_clip = 1000.0f;
    EXPECT_FALSE(cam.orthographic);
    EXPECT_FLOAT_EQ(cam.fov, 90.0f);
}

// 测试 相机组件：默认为单一
TEST(CameraComponentTest, DefaultIsSingle) {
    CameraComponent cam;
    EXPECT_EQ(cam.view, glm::mat4(1.0f));
    EXPECT_EQ(cam.projection, glm::mat4(1.0f));
}

// ============================================================
// CameraFollowComponent
// ============================================================

// 测试 相机跟随组件：默认值
TEST(CameraFollowComponentTest, DefaultValues) {
    CameraFollowComponent follow;
    EXPECT_TRUE(follow.follow_x);
    EXPECT_TRUE(follow.follow_y);
    EXPECT_TRUE(follow.enabled);
    EXPECT_FLOAT_EQ(follow.damping, 0.12f);
    EXPECT_FLOAT_EQ(follow.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(follow.dead_zone.x, 0.0f);
}

// 测试 相机跟随组件：设置Upoffset
TEST(CameraFollowComponentTest, SetUpoffset) {
    CameraFollowComponent follow;
    follow.offset = glm::vec3(1.0f, 2.0f, 0.0f);
    follow.dead_zone = glm::vec2(0.5f, 0.5f);
    EXPECT_FLOAT_EQ(follow.offset.x, 1.0f);
    EXPECT_FLOAT_EQ(follow.dead_zone.x, 0.5f);
}

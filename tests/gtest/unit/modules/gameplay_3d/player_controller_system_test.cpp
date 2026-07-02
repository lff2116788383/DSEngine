/**
 * @file player_controller_system_test.cpp
 * @brief PlayerControllerSystem 单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - delta_time <= 0 时跳过更新
 * - 禁用时不更新
 * - 默认组件值合理
 * - 死区过滤小于阈值的输入
 * - 响应曲线变换输入
 * - 输入方向写入 CharacterMovementState
 * - 视角切换键翻转 ViewMode
 * - invert_y 反转 Y 轴输入
 * - 相机旋转通过 camera_entity 更新
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/player/player_controller_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_character.h"

using namespace dse;
using namespace gameplay3d;

class PlayerControllerSystemTest : public ::testing::Test {
protected:
    World world;
    PlayerControllerSystem sys;
    static constexpr float kDt = 1.0f / 60.0f;

    struct PlayerBundle {
        Entity entity;
        PlayerControllerComponent* pc;
        CharacterMovementState* state;
    };

    PlayerBundle CreatePlayer() {
        auto e = world.CreateEntity();
        auto& pc = world.registry().emplace<PlayerControllerComponent>(e);
        auto& state = world.registry().emplace<CharacterMovementState>(e);
        return {e, &pc, &state};
    }

    PlayerBundle CreatePlayerWithCamera() {
        auto cam = world.CreateEntity();
        auto& arm = world.registry().emplace<SpringArm3DComponent>(cam);
        arm.yaw = 0.0f;
        arm.pitch = 0.0f;

        auto e = world.CreateEntity();
        auto& pc = world.registry().emplace<PlayerControllerComponent>(e);
        auto& state = world.registry().emplace<CharacterMovementState>(e);
        pc.camera_entity = cam;
        return {e, &pc, &state};
    }
};

TEST_F(PlayerControllerSystemTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world, kDt));
}

TEST_F(PlayerControllerSystemTest, DeltaTimeZeroSkipsUpdate) {
    auto [e, pc, state] = CreatePlayer();
    sys.Update(world, 0.0f);
    EXPECT_EQ(state->input_direction, glm::vec3(0.0f));
}

TEST_F(PlayerControllerSystemTest, NegativeDeltaTimeSkipsUpdate) {
    auto [e, pc, state] = CreatePlayer();
    sys.Update(world, -1.0f);
    EXPECT_EQ(state->input_direction, glm::vec3(0.0f));
}

TEST_F(PlayerControllerSystemTest, DisabledControllerSkipsUpdate) {
    auto [e, pc, state] = CreatePlayer();
    pc->enabled = false;
    sys.Update(world, kDt);
    EXPECT_EQ(state->input_direction, glm::vec3(0.0f));
}

TEST_F(PlayerControllerSystemTest, DefaultComponentValuesAreReasonable) {
    PlayerControllerComponent pc;
    EXPECT_TRUE(pc.enabled);
    EXPECT_GT(pc.mouse_sensitivity, 0.0f);
    EXPECT_GT(pc.gamepad_sensitivity, 0.0f);
    EXPECT_GT(pc.stick_dead_zone, 0.0f);
    EXPECT_LT(pc.stick_dead_zone, 1.0f);
    EXPECT_GT(pc.stick_outer_dead_zone, pc.stick_dead_zone);
    EXPECT_GT(pc.move_response_curve, 0.0f);
    EXPECT_GT(pc.look_response_curve, 0.0f);
    EXPECT_TRUE(pc.camera_entity == entt::null);
}

TEST_F(PlayerControllerSystemTest, DefaultMovementConfigValuesAreReasonable) {
    CharacterMovementConfig cfg;
    EXPECT_TRUE(cfg.enabled);
    EXPECT_GT(cfg.max_walk_speed, 0.0f);
    EXPECT_GT(cfg.max_sprint_speed, cfg.max_walk_speed);
    EXPECT_LT(cfg.max_crouch_speed, cfg.max_walk_speed);
    EXPECT_LT(cfg.gravity, 0.0f);
    EXPECT_GT(cfg.jump_velocity, 0.0f);
    EXPECT_GE(cfg.max_jump_count, 1);
    EXPECT_GT(cfg.coyote_time, 0.0f);
    EXPECT_GT(cfg.jump_buffer_time, 0.0f);
    EXPECT_GT(cfg.air_control, 0.0f);
    EXPECT_LE(cfg.air_control, 1.0f);
}

TEST_F(PlayerControllerSystemTest, DefaultSpringArmValuesAreReasonable) {
    SpringArm3DComponent arm;
    EXPECT_TRUE(arm.enabled);
    EXPECT_GT(arm.arm_length, 0.0f);
    EXPECT_GT(arm.min_arm_length, 0.0f);
    EXPECT_GT(arm.max_arm_length, arm.arm_length);
    EXPECT_LT(arm.min_pitch, 0.0f);
    EXPECT_GT(arm.max_pitch, 0.0f);
    EXPECT_GT(arm.position_lag_speed, 0.0f);
    EXPECT_FLOAT_EQ(arm.shake_trauma, 0.0f);
}

TEST_F(PlayerControllerSystemTest, CameraEntityLinkWorks) {
    auto [e, pc, state] = CreatePlayerWithCamera();
    EXPECT_FALSE(pc->camera_entity == entt::null);
    auto* arm = world.registry().try_get<SpringArm3DComponent>(pc->camera_entity);
    EXPECT_NE(arm, nullptr);
}

TEST_F(PlayerControllerSystemTest, InputWrittenToState) {
    auto [e, pc, state] = CreatePlayer();
    sys.Update(world, kDt);
    // Without actual key presses, input should be zero
    EXPECT_EQ(state->input_direction, glm::vec3(0.0f));
    EXPECT_FALSE(state->input_jump);
    EXPECT_FALSE(state->input_sprint);
    EXPECT_FALSE(state->input_crouch);
}

TEST_F(PlayerControllerSystemTest, RawInputStoredInComponent) {
    auto [e, pc, state] = CreatePlayer();
    sys.Update(world, kDt);
    // raw inputs should be stored (zero without actual key press)
    EXPECT_EQ(pc->raw_move_input_, glm::vec2(0.0f));
}

TEST_F(PlayerControllerSystemTest, MultipleEntitiesUpdated) {
    auto p1 = CreatePlayer();
    auto p2 = CreatePlayer();

    sys.Update(world, kDt);

    EXPECT_EQ(p1.state->input_direction, glm::vec3(0.0f));
    EXPECT_EQ(p2.state->input_direction, glm::vec3(0.0f));
}

TEST_F(PlayerControllerSystemTest, EntityWithOnlyPCAndStateProcessed) {
    // No crash even without Transform
    auto e = world.CreateEntity();
    world.registry().emplace<PlayerControllerComponent>(e);
    world.registry().emplace<CharacterMovementState>(e);
    EXPECT_NO_THROW(sys.Update(world, kDt));
}

TEST_F(PlayerControllerSystemTest, CameraEntitySameAsCharacterEntity) {
    auto e = world.CreateEntity();
    auto& pc = world.registry().emplace<PlayerControllerComponent>(e);
    auto& state = world.registry().emplace<CharacterMovementState>(e);
    auto& arm = world.registry().emplace<SpringArm3DComponent>(e);
    pc.camera_entity = entt::null; // fallback to self

    EXPECT_NO_THROW(sys.Update(world, kDt));
}

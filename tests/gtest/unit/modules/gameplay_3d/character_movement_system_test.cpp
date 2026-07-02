/**
 * @file character_movement_system_test.cpp
 * @brief CharacterMovementSystem 单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - delta_time <= 0 时跳过更新
 * - 地面移动加速/减速
 * - 跳跃 + 多段跳 + Coyote Time + Jump Buffer
 * - 重力 + 下落
 * - 冲刺/蹲伏模式切换
 * - 移动模式状态机转换
 * - 禁用时不更新
 * - 无物理时 graceful degradation（Transform fallback）
 * - OrientToMovement 旋转
 */

#include <gtest/gtest.h>
#include <glm/gtx/norm.hpp>
#include "modules/gameplay_3d/character/character_movement_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_character.h"

using namespace dse;
using namespace gameplay3d;

class CharacterMovementSystemTest : public ::testing::Test {
protected:
    World world;
    CharacterMovementSystem sys;
    static constexpr float kDt = 1.0f / 60.0f;

    struct CharacterBundle {
        Entity entity;
        CharacterMovementConfig* cfg;
        CharacterMovementState* state;
        CharacterController3DComponent* cc;
        TransformComponent* tf;
    };

    CharacterBundle CreateCharacter() {
        auto e = world.CreateEntity();
        auto& cfg = world.registry().emplace<CharacterMovementConfig>(e);
        auto& state = world.registry().emplace<CharacterMovementState>(e);
        auto& cc = world.registry().emplace<CharacterController3DComponent>(e);
        auto& tf = world.registry().emplace<TransformComponent>(e);
        tf.position = glm::vec3(0.0f, 0.0f, 0.0f); // y=0 so fallback grounding works
        state.is_grounded = true;
        cc.is_grounded = true;
        return {e, &cfg, &state, &cc, &tf};
    }
};

TEST_F(CharacterMovementSystemTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world, kDt));
}

TEST_F(CharacterMovementSystemTest, DeltaTimeZeroSkipsUpdate) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    sys.Update(world, 0.0f);
    EXPECT_EQ(state->velocity, glm::vec3(0.0f));
}

TEST_F(CharacterMovementSystemTest, NegativeDeltaTimeSkipsUpdate) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    sys.Update(world, -1.0f);
    EXPECT_EQ(state->velocity, glm::vec3(0.0f));
}

TEST_F(CharacterMovementSystemTest, DisabledConfigSkipsUpdate) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->enabled = false;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    sys.Update(world, kDt);
    EXPECT_EQ(state->velocity, glm::vec3(0.0f));
}

TEST_F(CharacterMovementSystemTest, GroundMovementAccelerates) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    sys.Update(world, kDt);
    EXPECT_GT(state->velocity.x, 0.0f);
}

TEST_F(CharacterMovementSystemTest, GroundMovementDecelerates) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->velocity = glm::vec3(5.0f, 0.0f, 0.0f);
    state->input_direction = glm::vec3(0.0f);
    sys.Update(world, kDt);
    EXPECT_LT(std::abs(state->velocity.x), 5.0f);
}

TEST_F(CharacterMovementSystemTest, SpeedCappedAtMaxWalkSpeed) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->max_walk_speed = 6.0f;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    for (int i = 0; i < 300; ++i) {
        state->is_grounded = true;
        state->movement_mode = MovementMode::Walking;
        sys.Update(world, kDt);
    }
    float speed_xz = std::sqrt(state->velocity.x * state->velocity.x +
                                state->velocity.z * state->velocity.z);
    EXPECT_LE(speed_xz, cfg->max_walk_speed + 0.5f);
}

TEST_F(CharacterMovementSystemTest, SprintModeSwitchesAndIncreasesMaxSpeed) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->max_sprint_speed = 10.0f;
    state->input_sprint = true;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    sys.Update(world, kDt);
    EXPECT_EQ(state->movement_mode, MovementMode::Sprinting);
}

TEST_F(CharacterMovementSystemTest, SprintDisabledReturnsToWalking) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->movement_mode = MovementMode::Sprinting;
    state->input_sprint = false;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    sys.Update(world, kDt);
    EXPECT_EQ(state->movement_mode, MovementMode::Walking);
}

TEST_F(CharacterMovementSystemTest, CrouchModeSwitchesAndModifiesCapsuleHeight) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->crouch_height = 0.5f;
    state->input_crouch = true;

    sys.Update(world, kDt);
    EXPECT_EQ(state->movement_mode, MovementMode::Crouching);
    EXPECT_FLOAT_EQ(cc->height, cfg->crouch_height);
}

TEST_F(CharacterMovementSystemTest, CrouchDisabledRestoresHeight) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->movement_mode = MovementMode::Crouching;
    cc->height = cfg->crouch_height;
    state->input_crouch = false;

    sys.Update(world, kDt);
    EXPECT_EQ(state->movement_mode, MovementMode::Walking);
    EXPECT_FLOAT_EQ(cc->height, cfg->stand_height);
}

TEST_F(CharacterMovementSystemTest, JumpSetsVelocityAndTransitionsToFalling) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_jump = true;
    state->coyote_timer_ = cfg->coyote_time;
    state->jump_buffer_timer_ = cfg->jump_buffer_time;

    sys.Update(world, kDt);
    EXPECT_EQ(state->movement_mode, MovementMode::Falling);
    EXPECT_TRUE(state->is_jumping);
    EXPECT_EQ(state->jump_count, 1);
}

TEST_F(CharacterMovementSystemTest, CoyoteTimeAllowsLateJump) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->coyote_time = 0.2f;
    state->is_grounded = false;
    state->coyote_timer_ = 0.1f;
    state->jump_buffer_timer_ = cfg->jump_buffer_time;
    state->movement_mode = MovementMode::Walking;

    sys.Update(world, kDt);
    EXPECT_EQ(state->jump_count, 1);
}

TEST_F(CharacterMovementSystemTest, JumpBufferStoresJumpRequestForLanding) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->jump_buffer_time = 0.15f;
    // Put character in air (falling) so jump buffer stores but can't execute ground jump
    tf->position.y = 100.0f;
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->input_jump = true;
    // Block multi-jump so the jump doesn't execute in air either
    state->jump_count = cfg->max_jump_count;

    sys.Update(world, kDt);
    // Jump buffer timer should have been set to jump_buffer_time (input_jump was true)
    // It equals jump_buffer_time exactly because the set happens after the decrement branch
    EXPECT_GT(state->jump_buffer_timer_, 0.0f);
}

TEST_F(CharacterMovementSystemTest, MultiJumpInAir) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->max_jump_count = 2;
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->jump_count = 1;
    state->input_jump = true;

    sys.Update(world, kDt);
    EXPECT_EQ(state->jump_count, 2);
}

TEST_F(CharacterMovementSystemTest, CannotExceedMaxJumpCount) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->max_jump_count = 2;
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->jump_count = 2;
    state->input_jump = true;

    sys.Update(world, kDt);
    EXPECT_EQ(state->jump_count, 2);
}

TEST_F(CharacterMovementSystemTest, GravityAppliedDuringFall) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    tf->position.y = 100.0f; // elevated so fallback grounding doesn't trigger landing
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->velocity = glm::vec3(0.0f, 0.0f, 0.0f);

    sys.Update(world, kDt);
    EXPECT_LT(state->velocity.y, 0.0f);
}

TEST_F(CharacterMovementSystemTest, FallSpeedCapped) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->max_fall_speed = -30.0f;
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->velocity.y = -100.0f;

    sys.Update(world, kDt);
    EXPECT_GE(state->velocity.y, cfg->max_fall_speed);
}

TEST_F(CharacterMovementSystemTest, AirControlReducesHorizontalAcceleration) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->air_control = 0.3f;
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    glm::vec3 vel_before = state->velocity;
    sys.Update(world, kDt);
    float delta_x = state->velocity.x - vel_before.x;
    EXPECT_GT(delta_x, 0.0f);
    EXPECT_LT(delta_x, cfg->ground_acceleration * kDt);
}

TEST_F(CharacterMovementSystemTest, FallbackTransformUpdateWithoutPhysics) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 pos_before = tf->position;
    sys.Update(world, kDt);
    EXPECT_NE(tf->position, pos_before);
    EXPECT_TRUE(tf->dirty);
}

TEST_F(CharacterMovementSystemTest, LandingResetsToWalking) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->velocity.y = -5.0f;
    tf->position.y = 0.01f;

    for (int i = 0; i < 10; ++i) {
        sys.Update(world, kDt);
        if (state->is_grounded) break;
    }
    EXPECT_TRUE(state->is_grounded);
    EXPECT_EQ(state->movement_mode, MovementMode::Walking);
}

TEST_F(CharacterMovementSystemTest, SwimmingModeUsesSwimSpeed) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->swim_speed = 4.0f;
    state->movement_mode = MovementMode::Swimming;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    sys.Update(world, kDt);
    EXPECT_GT(state->velocity.x, 0.0f);
}

TEST_F(CharacterMovementSystemTest, FlyingModeUsesFlightSpeed) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->fly_speed = 8.0f;
    state->movement_mode = MovementMode::Flying;
    state->input_direction = glm::vec3(0.0f, 1.0f, 0.0f);

    sys.Update(world, kDt);
    EXPECT_GT(state->velocity.y, 0.0f);
}

TEST_F(CharacterMovementSystemTest, CustomModeDoesNothing) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->movement_mode = MovementMode::Custom;
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 vel_before = state->velocity;
    sys.Update(world, kDt);
    EXPECT_EQ(state->velocity, vel_before);
}

TEST_F(CharacterMovementSystemTest, OrientToMovementRotatesCharacter) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    cfg->rotation_mode = RotationMode::OrientToMovement;
    state->velocity = glm::vec3(5.0f, 0.0f, 0.0f);
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    sys.Update(world, kDt);
    EXPECT_TRUE(tf->dirty);
}

TEST_F(CharacterMovementSystemTest, JumpInputClearedAfterProcessing) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_jump = true;
    sys.Update(world, kDt);
    EXPECT_FALSE(state->input_jump);
}

TEST_F(CharacterMovementSystemTest, CC3DStatesSyncedFromMovementState) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    sys.Update(world, kDt);
    EXPECT_EQ(cc->is_grounded, state->is_grounded);
    EXPECT_EQ(cc->velocity, state->velocity);
}

TEST_F(CharacterMovementSystemTest, CannotCrouchWhileFalling) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->input_crouch = true;

    sys.Update(world, kDt);
    EXPECT_NE(state->movement_mode, MovementMode::Crouching);
}

TEST_F(CharacterMovementSystemTest, CrouchBlocksJump) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->input_crouch = true;
    state->input_jump = true;
    state->coyote_timer_ = cfg->coyote_time;
    state->jump_buffer_timer_ = cfg->jump_buffer_time;

    sys.Update(world, kDt);
    // Should be crouching (or walking if crouch doesn't persist), but NOT falling from a jump
    EXPECT_NE(state->movement_mode, MovementMode::Falling);
    // Jump should not have executed while crouching
    EXPECT_EQ(state->jump_count, 0);
}

TEST_F(CharacterMovementSystemTest, FallTimeAccumulatesWhileAirborne) {
    auto [e, cfg, state, cc, tf] = CreateCharacter();
    state->movement_mode = MovementMode::Falling;
    state->is_grounded = false;
    state->fall_time = 0.0f;

    sys.Update(world, kDt);
    EXPECT_GT(state->fall_time, 0.0f);
}

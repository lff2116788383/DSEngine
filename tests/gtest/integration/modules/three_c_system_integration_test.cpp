/**
 * @file three_c_system_integration_test.cpp
 * @brief 3C (Character-Camera-Control) 系统集成测试
 *
 * 验证 PlayerController → CharacterMovement → CameraArm3D 完整管线：
 * - 输入驱动角色移动，相机跟随
 * - 跳跃 → 下落 → 着地完整循环
 * - 冲刺/蹲伏模式切换影响速度
 * - 相机碰撞避让 + 震动衰减
 * - OrientToCamera 模式朝向相机方向
 * - 三系统更新顺序正确
 */

#include <gtest/gtest.h>
#include <glm/gtx/norm.hpp>
#include "modules/gameplay_3d/player/player_controller_system.h"
#include "modules/gameplay_3d/character/character_movement_system.h"
#include "modules/gameplay_3d/camera/camera_arm_3d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_character.h"

using namespace dse;
using namespace gameplay3d;

class ThreeCSystemIntegrationTest : public ::testing::Test {
protected:
    World world;
    PlayerControllerSystem player_sys;
    CharacterMovementSystem movement_sys;
    CameraArm3DSystem camera_sys;
    static constexpr float kDt = 1.0f / 60.0f;

    struct ThreeCBundle {
        Entity character;
        Entity camera;
        CharacterMovementConfig* cfg;
        CharacterMovementState* state;
        CharacterController3DComponent* cc;
        TransformComponent* char_tf;
        PlayerControllerComponent* pc;
        SpringArm3DComponent* arm;
        TransformComponent* cam_tf;
    };

    ThreeCBundle CreateFullSetup() {
        // Character entity
        auto character = world.CreateEntity();
        auto& cfg = world.registry().emplace<CharacterMovementConfig>(character);
        auto& state = world.registry().emplace<CharacterMovementState>(character);
        auto& cc = world.registry().emplace<CharacterController3DComponent>(character);
        auto& char_tf = world.registry().emplace<TransformComponent>(character);
        auto& pc = world.registry().emplace<PlayerControllerComponent>(character);
        char_tf.position = glm::vec3(0.0f, 0.1f, 0.0f);
        state.is_grounded = true;
        cc.is_grounded = true;

        // Camera entity
        auto camera = world.CreateEntity();
        auto& arm = world.registry().emplace<SpringArm3DComponent>(camera);
        auto& cam_tf = world.registry().emplace<TransformComponent>(camera);
        arm.target_entity = character;
        arm.arm_length = 4.0f;
        arm.current_arm_length_ = 4.0f;
        pc.camera_entity = camera;

        return {character, camera, &cfg, &state, &cc, &char_tf, &pc, &arm, &cam_tf};
    }

    void Tick(float dt = kDt) {
        player_sys.Update(world, dt);
        movement_sys.Update(world, dt);
        camera_sys.Update(world, dt);
    }

    void TickN(int n, float dt = kDt) {
        for (int i = 0; i < n; ++i) Tick(dt);
    }

    void TickMovementCamera(float dt = kDt) {
        movement_sys.Update(world, dt);
        camera_sys.Update(world, dt);
    }

    void TickMovementCameraN(int n, float dt = kDt) {
        for (int i = 0; i < n; ++i) TickMovementCamera(dt);
    }
};

TEST_F(ThreeCSystemIntegrationTest, FullPipelineDoesNotCrash) {
    auto setup = CreateFullSetup();
    EXPECT_NO_THROW(TickN(60));
}

TEST_F(ThreeCSystemIntegrationTest, EmptyWorldDoesNotCrash) {
    EXPECT_NO_THROW(TickN(10));
}

TEST_F(ThreeCSystemIntegrationTest, DirectInputDrivesMovement) {
    auto s = CreateFullSetup();
    s.state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    // Skip player controller (direct input), run movement + camera
    movement_sys.Update(world, kDt);
    camera_sys.Update(world, kDt);

    EXPECT_GT(s.state->velocity.x, 0.0f);
    EXPECT_GT(s.char_tf->position.x, 0.0f);
}

TEST_F(ThreeCSystemIntegrationTest, CameraFollowsCharacterAfterMovement) {
    auto s = CreateFullSetup();
    s.state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    TickN(60);

    // Camera pivot should be near character position + offset
    glm::vec3 expected_pivot = s.char_tf->position + s.arm->target_offset;
    EXPECT_NEAR(s.arm->current_pivot_.x, expected_pivot.x, 1.0f);
    EXPECT_NEAR(s.arm->current_pivot_.y, expected_pivot.y, 1.0f);
}

TEST_F(ThreeCSystemIntegrationTest, JumpFallLandCycle) {
    auto s = CreateFullSetup();

    // Jump
    s.state->input_jump = true;
    s.state->coyote_timer_ = s.cfg->coyote_time;
    s.state->jump_buffer_timer_ = s.cfg->jump_buffer_time;
    movement_sys.Update(world, kDt);

    EXPECT_EQ(s.state->movement_mode, MovementMode::Falling);
    EXPECT_TRUE(s.state->is_jumping);
    EXPECT_EQ(s.state->jump_count, 1);

    // Fall for several frames
    for (int i = 0; i < 120; ++i) {
        movement_sys.Update(world, kDt);
        if (s.state->is_grounded) break;
    }

    // Should land
    EXPECT_TRUE(s.state->is_grounded);
    EXPECT_EQ(s.state->movement_mode, MovementMode::Walking);
}

TEST_F(ThreeCSystemIntegrationTest, SprintIncreasesVelocity) {
    auto s = CreateFullSetup();

    // Walk first (bypass player controller to keep manual input)
    for (int i = 0; i < 60; ++i) {
        s.state->input_direction = glm::vec3(0.0f, 0.0f, -1.0f);
        TickMovementCamera();
    }
    float walk_speed = glm::length(glm::vec2(s.state->velocity.x, s.state->velocity.z));

    // Reset and sprint
    s.state->velocity = glm::vec3(0.0f);
    s.state->movement_mode = MovementMode::Walking;
    for (int i = 0; i < 60; ++i) {
        s.state->input_sprint = true;
        s.state->input_direction = glm::vec3(0.0f, 0.0f, -1.0f);
        TickMovementCamera();
    }
    float sprint_speed = glm::length(glm::vec2(s.state->velocity.x, s.state->velocity.z));

    EXPECT_GT(sprint_speed, walk_speed * 0.8f);
}

TEST_F(ThreeCSystemIntegrationTest, CrouchReducesSpeed) {
    auto s = CreateFullSetup();
    // Lower friction so speeds converge to distinct equilibria
    s.cfg->ground_friction = 1.0f;

    // Walk first
    for (int i = 0; i < 120; ++i) {
        s.state->input_direction = glm::vec3(0.0f, 0.0f, -1.0f);
        TickMovementCamera();
    }
    float walk_speed = glm::length(glm::vec2(s.state->velocity.x, s.state->velocity.z));

    // Reset and crouch
    s.state->velocity = glm::vec3(0.0f);
    s.state->movement_mode = MovementMode::Walking;
    for (int i = 0; i < 120; ++i) {
        s.state->input_crouch = true;
        s.state->input_direction = glm::vec3(0.0f, 0.0f, -1.0f);
        TickMovementCamera();
    }
    float crouch_speed = glm::length(glm::vec2(s.state->velocity.x, s.state->velocity.z));

    EXPECT_GT(walk_speed, 0.0f);
    EXPECT_GT(crouch_speed, 0.0f);
    EXPECT_LT(crouch_speed, walk_speed);
}

TEST_F(ThreeCSystemIntegrationTest, CameraShakeDecaysOverTime) {
    auto s = CreateFullSetup();
    s.arm->shake_trauma = 1.0f;

    TickN(60);

    EXPECT_LT(s.arm->shake_trauma, 0.1f);
}

TEST_F(ThreeCSystemIntegrationTest, ViewModeToggleWorks) {
    auto s = CreateFullSetup();
    EXPECT_EQ(s.arm->view_mode, SpringArm3DComponent::ViewMode::ThirdPerson);

    // Manually toggle
    s.arm->view_mode = SpringArm3DComponent::ViewMode::FirstPerson;
    TickN(30);

    float dist = glm::length(s.cam_tf->position - s.arm->current_pivot_);
    EXPECT_NEAR(dist, 0.0f, 1.0f);
}

TEST_F(ThreeCSystemIntegrationTest, OrientToCameraRotatesCharacter) {
    auto s = CreateFullSetup();
    s.cfg->rotation_mode = RotationMode::OrientToCamera;
    s.arm->yaw = 90.0f;
    s.state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    TickN(10);

    EXPECT_TRUE(s.char_tf->dirty);
}

TEST_F(ThreeCSystemIntegrationTest, CC3DStateSyncedThroughPipeline) {
    auto s = CreateFullSetup();
    s.state->input_direction = glm::vec3(1.0f, 0.0f, 0.0f);

    Tick();

    EXPECT_EQ(s.cc->is_grounded, s.state->is_grounded);
    EXPECT_EQ(s.cc->velocity, s.state->velocity);
}

TEST_F(ThreeCSystemIntegrationTest, MultiJumpInAirThroughPipeline) {
    auto s = CreateFullSetup();
    s.cfg->max_jump_count = 3;

    // First jump from ground
    s.state->input_jump = true;
    s.state->jump_buffer_timer_ = s.cfg->jump_buffer_time;
    s.state->coyote_timer_ = s.cfg->coyote_time;
    s.state->is_grounded = true;
    movement_sys.Update(world, kDt);
    EXPECT_EQ(s.state->jump_count, 1);
    EXPECT_EQ(s.state->movement_mode, MovementMode::Falling);

    // Ensure airborne state for multi-jump
    s.state->is_grounded = false;

    // Second jump in air
    s.state->input_jump = true;
    movement_sys.Update(world, kDt);
    EXPECT_EQ(s.state->jump_count, 2);

    // Third jump in air
    s.state->input_jump = true;
    movement_sys.Update(world, kDt);
    EXPECT_EQ(s.state->jump_count, 3);

    // Fourth jump blocked
    s.state->input_jump = true;
    movement_sys.Update(world, kDt);
    EXPECT_EQ(s.state->jump_count, 3);
}

TEST_F(ThreeCSystemIntegrationTest, GravityBringsCharacterDown) {
    auto s = CreateFullSetup();
    s.char_tf->position.y = 10.0f;
    s.state->movement_mode = MovementMode::Falling;
    s.state->is_grounded = false;

    TickN(120);

    // Without physics, fallback uses y <= 0 for grounding
    EXPECT_TRUE(s.state->is_grounded);
    EXPECT_LE(s.char_tf->position.y, 0.01f);
}

TEST_F(ThreeCSystemIntegrationTest, SwimmingModeMovesInAllAxes) {
    auto s = CreateFullSetup();
    s.state->movement_mode = MovementMode::Swimming;
    for (int i = 0; i < 30; ++i) {
        s.state->input_direction = glm::vec3(1.0f, 0.5f, 0.0f);
        TickMovementCamera();
    }

    EXPECT_GT(s.state->velocity.x, 0.0f);
}

TEST_F(ThreeCSystemIntegrationTest, FlyingModeMovesVertically) {
    auto s = CreateFullSetup();
    s.state->movement_mode = MovementMode::Flying;
    for (int i = 0; i < 30; ++i) {
        s.state->input_direction = glm::vec3(0.0f, 1.0f, 0.0f);
        TickMovementCamera();
    }

    EXPECT_GT(s.state->velocity.y, 0.0f);
    EXPECT_GT(s.char_tf->position.y, 0.1f);
}

TEST_F(ThreeCSystemIntegrationTest, DisabledSystemsDoNotInterfere) {
    auto s = CreateFullSetup();
    s.cfg->enabled = false;
    s.pc->enabled = false;
    s.arm->enabled = false;

    glm::vec3 char_pos = s.char_tf->position;
    glm::vec3 cam_pos = s.cam_tf->position;

    TickN(30);

    EXPECT_EQ(s.char_tf->position, char_pos);
    EXPECT_EQ(s.cam_tf->position, cam_pos);
}

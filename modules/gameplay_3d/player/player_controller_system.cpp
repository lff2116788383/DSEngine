#include "modules/gameplay_3d/player/player_controller_system.h"
#include "engine/ecs/components_3d_character.h"
#include "engine/ecs/transform.h"
#include "engine/input/input.h"
#include "engine/input/key_code.h"
#include "engine/input/action_mapping.h"
#include "engine/core/service_locator.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace dse::gameplay3d {

namespace {

inline glm::vec2 ApplyRadialDeadZone(glm::vec2 raw, float inner, float outer) {
    float len = glm::length(raw);
    if (len < inner) return glm::vec2(0.0f);
    if (len > outer) return raw / len;
    float normalized = (len - inner) / (outer - inner);
    return raw / len * normalized;
}

inline float ApplyResponseCurve(float value, float exponent) {
    float sign = (value >= 0.0f) ? 1.0f : -1.0f;
    return sign * std::pow(std::abs(value), exponent);
}

} // namespace

void PlayerControllerSystem::Update(World& world, float dt) {
    if (dt <= 0.0f) return;

    auto view = world.registry().view<PlayerControllerComponent,
                                       CharacterMovementState>();

    for (auto entity : view) {
        auto& pc    = view.get<PlayerControllerComponent>(entity);
        auto& state = view.get<CharacterMovementState>(entity);

        if (!pc.enabled) continue;

        // ── 读取键盘/鼠标输入 ──
        glm::vec2 move_raw(0.0f);
        glm::vec2 look_raw(0.0f);
        bool jump = false;
        bool sprint = false;
        bool crouch = false;
        bool toggle_view = false;

        if (Input::GetKey(KEY_CODE_W)) move_raw.y += 1.0f;
        if (Input::GetKey(KEY_CODE_S)) move_raw.y -= 1.0f;
        if (Input::GetKey(KEY_CODE_A)) move_raw.x -= 1.0f;
        if (Input::GetKey(KEY_CODE_D)) move_raw.x += 1.0f;
        jump   = Input::GetKeyDown(KEY_CODE_SPACE);
        sprint = Input::GetKey(KEY_CODE_LEFT_SHIFT);
        crouch = Input::GetKeyDown(KEY_CODE_LEFT_CONTROL);
        toggle_view = Input::GetKeyDown(KEY_CODE_V);

        // 鼠标视角
        glm::vec2 mouse_delta = Input::GetSwipeDelta();
        look_raw = mouse_delta * pc.mouse_sensitivity;

        // ── 手柄叠加 ──
        if (Input::IsGamepadConnected(0)) {
            glm::vec2 left_stick(
                Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_X),
                Input::GetGamepadAxis(0, GAMEPAD_AXIS_LEFT_Y));
            glm::vec2 right_stick(
                Input::GetGamepadAxis(0, GAMEPAD_AXIS_RIGHT_X),
                Input::GetGamepadAxis(0, GAMEPAD_AXIS_RIGHT_Y));

            left_stick = ApplyRadialDeadZone(left_stick, pc.stick_dead_zone, pc.stick_outer_dead_zone);
            right_stick = ApplyRadialDeadZone(right_stick, pc.stick_dead_zone, pc.stick_outer_dead_zone);

            if (glm::length(left_stick) > 0.01f) {
                move_raw = left_stick;
            }
            look_raw += right_stick * pc.gamepad_sensitivity;
        }

        // 保存原始值
        pc.raw_move_input_ = move_raw;
        pc.raw_look_input_ = look_raw;

        // ── 应用死区和响应曲线到移动输入 ──
        move_raw = ApplyRadialDeadZone(move_raw, pc.stick_dead_zone, pc.stick_outer_dead_zone);
        float move_len = glm::length(move_raw);
        if (move_len > 0.01f) {
            float curved = ApplyResponseCurve(move_len, pc.move_response_curve);
            move_raw = move_raw / move_len * curved;
        }

        // ── 输入 → 世界空间（相对相机 yaw） ──
        float camera_yaw_rad = 0.0f;

        // 尝试获取相机 yaw
        if (pc.camera_entity != entt::null) {
            auto* arm = world.registry().try_get<SpringArm3DComponent>(pc.camera_entity);
            if (arm) {
                camera_yaw_rad = glm::radians(arm->yaw);
            }
        } else {
            auto* arm = world.registry().try_get<SpringArm3DComponent>(entity);
            if (arm) {
                camera_yaw_rad = glm::radians(arm->yaw);
            }
        }

        float cos_yaw = std::cos(camera_yaw_rad);
        float sin_yaw = std::sin(camera_yaw_rad);

        glm::vec3 world_dir(0.0f);
        world_dir.x = move_raw.x * cos_yaw - move_raw.y * sin_yaw;
        world_dir.z = move_raw.x * sin_yaw + move_raw.y * cos_yaw;

        // 写入 CharacterMovementState
        state.input_direction = world_dir;
        state.input_jump = jump;
        state.input_sprint = sprint;
        state.input_crouch = crouch;

        // ── 更新相机旋转 ──
        auto update_arm = [&](SpringArm3DComponent* arm) {
            if (!arm) return;
            float look_x = ApplyResponseCurve(look_raw.x, pc.look_response_curve);
            float look_y = ApplyResponseCurve(look_raw.y, pc.look_response_curve);
            if (pc.invert_y) look_y = -look_y;

            arm->yaw += look_x;
            arm->pitch -= look_y;

            if (toggle_view) {
                arm->view_mode = (arm->view_mode == SpringArm3DComponent::ViewMode::ThirdPerson)
                                     ? SpringArm3DComponent::ViewMode::FirstPerson
                                     : SpringArm3DComponent::ViewMode::ThirdPerson;
            }
        };

        if (pc.camera_entity != entt::null) {
            update_arm(world.registry().try_get<SpringArm3DComponent>(pc.camera_entity));
        } else {
            update_arm(world.registry().try_get<SpringArm3DComponent>(entity));
        }
    }
}

} // namespace dse::gameplay3d

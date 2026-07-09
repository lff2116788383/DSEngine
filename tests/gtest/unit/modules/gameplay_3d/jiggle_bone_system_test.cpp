/**
 * @file jiggle_bone_system_test.cpp
 * @brief JiggleIntegrate（弹簧末端积分纯函数）单元测试。
 *
 * 只测纯函数，不触碰 ECS/资产加载，保证确定性与快速。
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/animation/jiggle_bone_system.h"

#include <glm/glm.hpp>

using dse::gameplay3d::JiggleIntegrate;
using dse::gameplay3d::JiggleStepInput;

namespace {

constexpr float kDt = 1.0f / 60.0f;

JiggleStepInput MakeInput(const glm::vec3& head, const glm::vec3& rest_tip) {
    JiggleStepInput in;
    in.head_pos = head;
    in.rest_tip = rest_tip;
    in.cur_tip = rest_tip;
    in.prev_tip = rest_tip;
    in.gravity = glm::vec3(0.0f);
    in.stiffness = 0.1f;
    in.damping = 0.3f;
    in.length = glm::length(rest_tip - head);
    return in;
}

} // namespace

// 静止：cur==prev==rest 且无重力 → 末端保持在静止位置
TEST(JiggleIntegrateTest, StaysAtRestWhenUndisturbed) {
    const glm::vec3 head(0.0f);
    const glm::vec3 rest(0.0f, -0.2f, 0.0f);
    JiggleStepInput in = MakeInput(head, rest);

    glm::vec3 tip = JiggleIntegrate(in, kDt);
    EXPECT_NEAR(tip.x, rest.x, 1e-5f);
    EXPECT_NEAR(tip.y, rest.y, 1e-5f);
    EXPECT_NEAR(tip.z, rest.z, 1e-5f);
}

// 长度约束：任意输入后 |tip - head| 恒等于 length
TEST(JiggleIntegrateTest, PreservesBoneLength) {
    const glm::vec3 head(1.0f, 2.0f, 3.0f);
    const glm::vec3 rest = head + glm::vec3(0.0f, -0.25f, 0.0f);
    JiggleStepInput in = MakeInput(head, rest);
    in.gravity = glm::vec3(0.0f, -9.8f, 0.0f);
    // 制造初速度
    in.prev_tip = rest + glm::vec3(0.05f, 0.0f, 0.0f);

    glm::vec3 tip = in.cur_tip;
    for (int i = 0; i < 120; ++i) {
        in.cur_tip = tip;
        glm::vec3 next = JiggleIntegrate(in, kDt);
        in.prev_tip = tip;
        tip = next;
        EXPECT_NEAR(glm::length(tip - head), in.length, 1e-4f) << "step " << i;
    }
}

// 重力：向下重力使末端偏离静止位置（且仍在长度球面上）
TEST(JiggleIntegrateTest, GravityPullsAwayFromRest) {
    const glm::vec3 head(0.0f);
    const glm::vec3 rest(0.2f, 0.0f, 0.0f); // 静止指向 +X（水平），重力沿 -Y
    JiggleStepInput in = MakeInput(head, rest);
    in.gravity = glm::vec3(0.0f, -20.0f, 0.0f);
    in.stiffness = 0.02f;
    in.damping = 0.1f;

    glm::vec3 tip = in.cur_tip;
    for (int i = 0; i < 60; ++i) {
        in.cur_tip = tip;
        glm::vec3 next = JiggleIntegrate(in, kDt);
        in.prev_tip = tip;
        tip = next;
    }
    // 重力应把末端往下拉（y 明显为负）
    EXPECT_LT(tip.y, -0.01f);
    EXPECT_NEAR(glm::length(tip - head), in.length, 1e-4f);
}

// 阻尼收敛：扰动后无外力时末端回到静止位置
TEST(JiggleIntegrateTest, ConvergesBackToRest) {
    const glm::vec3 head(0.0f);
    const glm::vec3 rest(0.0f, -0.3f, 0.0f);
    JiggleStepInput in = MakeInput(head, rest);
    in.stiffness = 0.2f;
    in.damping = 0.5f;
    // 初始扰动
    in.cur_tip = glm::normalize(glm::vec3(0.3f, -0.3f, 0.0f)) * in.length;
    in.prev_tip = in.cur_tip;

    glm::vec3 tip = in.cur_tip;
    for (int i = 0; i < 400; ++i) {
        in.cur_tip = tip;
        glm::vec3 next = JiggleIntegrate(in, kDt);
        in.prev_tip = tip;
        tip = next;
    }
    EXPECT_NEAR(tip.x, rest.x, 5e-3f);
    EXPECT_NEAR(tip.y, rest.y, 5e-3f);
    EXPECT_NEAR(tip.z, rest.z, 5e-3f);
}

// stiffness/damping 越界不崩溃（内部 clamp）
TEST(JiggleIntegrateTest, ClampsOutOfRangeParams) {
    const glm::vec3 head(0.0f);
    const glm::vec3 rest(0.0f, -0.1f, 0.0f);
    JiggleStepInput in = MakeInput(head, rest);
    in.stiffness = 5.0f;   // >1
    in.damping = -2.0f;    // <0
    in.cur_tip = glm::vec3(0.1f, 0.0f, 0.0f);
    in.prev_tip = glm::vec3(0.05f, 0.0f, 0.0f);

    glm::vec3 tip = JiggleIntegrate(in, kDt);
    EXPECT_TRUE(std::isfinite(tip.x));
    EXPECT_TRUE(std::isfinite(tip.y));
    EXPECT_TRUE(std::isfinite(tip.z));
    EXPECT_NEAR(glm::length(tip - head), in.length, 1e-4f);
}

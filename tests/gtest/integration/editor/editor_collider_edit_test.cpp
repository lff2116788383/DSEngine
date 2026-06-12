/**
 * @file editor_collider_edit_test.cpp
 * @brief 碰撞体拖拽编辑纯数学核心（editor_collider_edit）的无头测试。
 *
 * 验证碰撞体局部参数（center/size、offset/radius）↔ 世界矩阵的相互转换——这是
 * 视口里"拖 Gizmo → 写回组件"的关键逻辑。ImGuizmo 视口交互本身（GPU/ImGui）不在此覆盖。
 */

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "editor_collider_edit.h"

using namespace dse::editor::collideredit;

namespace {
constexpr float kEps = 1e-4f;
void ExpectVec3Near(const glm::vec3& a, const glm::vec3& b, float eps = kEps) {
    EXPECT_NEAR(a.x, b.x, eps);
    EXPECT_NEAR(a.y, b.y, eps);
    EXPECT_NEAR(a.z, b.z, eps);
}
}  // namespace

// ── Box：Build/Extract 往返（含实体缩放） ────────────────────────────────────

// 测试 碰撞体编辑：盒往返带实体缩放
TEST(ColliderEdit, BoxRoundTripWithEntityScale) {
    const glm::vec3 epos(10.0f, 5.0f, -2.0f);
    const glm::vec3 escale(2.0f, 1.0f, 0.5f);
    const glm::vec3 center(0.5f, -1.0f, 2.0f);
    const glm::vec3 size(3.0f, 4.0f, 1.0f);

    glm::mat4 m = BuildBoxMatrix(epos, escale, center, size);

    // 世界中心 = epos + center*escale；世界尺寸 = size*escale。
    ExpectVec3Near(glm::vec3(m[3]), epos + center * escale);
    EXPECT_NEAR(m[0][0], size.x * escale.x, kEps);
    EXPECT_NEAR(m[1][1], size.y * escale.y, kEps);
    EXPECT_NEAR(m[2][2], size.z * escale.z, kEps);

    glm::vec3 out_center, out_size;
    ExtractBox(m, epos, escale, out_center, out_size);
    ExpectVec3Near(out_center, center);
    ExpectVec3Near(out_size, size);
}

// ── Box：拖拽放大一个面后，尺寸随之增大（模拟 Gizmo SCALE 改了世界矩阵） ─────────

// 测试 碰撞体编辑：盒调整大小从矩阵增长尺寸
TEST(ColliderEdit, BoxResizeFromMatrixGrowsSize) {
    const glm::vec3 epos(0.0f);
    const glm::vec3 escale(1.0f);
    glm::mat4 m = BuildBoxMatrix(epos, escale, glm::vec3(0.0f), glm::vec3(2.0f));

    // 模拟把 X 尺寸放大 1.5 倍
    m[0][0] *= 1.5f;

    glm::vec3 out_center, out_size;
    ExtractBox(m, epos, escale, out_center, out_size);
    EXPECT_NEAR(out_size.x, 3.0f, kEps);
    EXPECT_NEAR(out_size.y, 2.0f, kEps);
}

// ── Box：移动世界中心后，反解出的 local center 正确（除以实体缩放） ───────────────

// 测试 碰撞体编辑：盒移动中心Divides按实体缩放
TEST(ColliderEdit, BoxMoveCenterDividesByEntityScale) {
    const glm::vec3 epos(1.0f, 2.0f, 3.0f);
    const glm::vec3 escale(2.0f, 4.0f, 8.0f);
    glm::mat4 m = BuildBoxMatrix(epos, escale, glm::vec3(0.0f), glm::vec3(1.0f));

    // 世界中心平移 (+2, +4, +8)
    m[3][0] += 2.0f; m[3][1] += 4.0f; m[3][2] += 8.0f;

    glm::vec3 out_center, out_size;
    ExtractBox(m, epos, escale, out_center, out_size);
    // local 偏移 = 世界偏移 / 实体缩放 = (1,1,1)
    ExpectVec3Near(out_center, glm::vec3(1.0f, 1.0f, 1.0f));
}

// ── Box：尺寸被钳制为正 ──────────────────────────────────────────────────────

// 测试 碰撞体编辑：盒尺寸钳制Positive
TEST(ColliderEdit, BoxSizeClampedPositive) {
    glm::mat4 m = BuildBoxMatrix(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f), glm::vec3(1.0f));
    m[0][0] = 0.0f;  // 退化为 0
    glm::vec3 c, s;
    ExtractBox(m, glm::vec3(0.0f), glm::vec3(1.0f), c, s);
    EXPECT_GT(s.x, 0.0f);
}

// ── Sphere：Build/Extract 往返（均匀缩放） ───────────────────────────────────

// 测试 碰撞体编辑：球往返
TEST(ColliderEdit, SphereRoundTrip) {
    const glm::vec3 epos(3.0f, 0.0f, 1.0f);
    const glm::vec3 escale(2.0f, 2.0f, 2.0f);
    const glm::vec3 center(1.0f, 0.0f, -0.5f);
    const float radius = 0.75f;

    glm::mat4 m = BuildSphereMatrix(epos, escale, center, radius);
    // 直径 = radius*2*max(scale)
    EXPECT_NEAR(m[0][0], radius * 2.0f * 2.0f, kEps);

    glm::vec3 out_center; float out_radius;
    ExtractSphere(m, epos, escale, out_center, out_radius);
    ExpectVec3Near(out_center, center);
    EXPECT_NEAR(out_radius, radius, kEps);
}

// ── Sphere：放大世界矩阵 → 半径增大 ──────────────────────────────────────────

// 测试 碰撞体编辑：球调整大小增长Radius
TEST(ColliderEdit, SphereResizeGrowsRadius) {
    glm::mat4 m = BuildSphereMatrix(glm::vec3(0.0f), glm::vec3(1.0f), glm::vec3(0.0f), 1.0f);
    // 均匀放大 2 倍
    m[0][0] *= 2.0f; m[1][1] *= 2.0f; m[2][2] *= 2.0f;
    glm::vec3 c; float r;
    ExtractSphere(m, glm::vec3(0.0f), glm::vec3(1.0f), c, r);
    EXPECT_NEAR(r, 2.0f, kEps);
}

// ── SafeDiv：零除保护 ────────────────────────────────────────────────────────

// 测试 碰撞体编辑：安全Div Guards零
TEST(ColliderEdit, SafeDivGuardsZero) {
    EXPECT_NEAR(SafeDiv(4.0f, 2.0f), 2.0f, kEps);
    EXPECT_NEAR(SafeDiv(4.0f, 0.0f), 4.0f, kEps);  // 退化时原样返回，避免 NaN/inf
}

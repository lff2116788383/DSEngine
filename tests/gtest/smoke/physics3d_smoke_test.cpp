/**
 * @file physics3d_smoke_test.cpp
 * @brief Physics3D 真实 PhysX 模拟冒烟测试
 *
 * 在 PhysX DLL 可用时运行真实刚体下落、碰撞检测、射线命中等场景。
 * 不可用时通过 GTEST_SKIP 跳过。
 *
 * 覆盖：
 * - Init/Shutdown 完整生命周期
 * - 动态刚体受重力下落
 * - 静态地面阻挡下落
 * - Raycast 命中刚体
 * - AddImpulse 施加冲量
 * - 碰撞事件产生
 * - 多帧连续模拟稳定性
 * - RemoveActor 安全移除
 */

#include <gtest/gtest.h>

#if !defined(DSE_ENABLE_PHYSX)
TEST(Physics3DSmokeTest, PhysXNotEnabled_SkipAll) {
    GTEST_SKIP() << "DSE_ENABLE_PHYSX not defined";
}
#else

#include "engine/physics/physics3d/physics3d_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"

using namespace dse;
using namespace dse::physics3d;

class Physics3DSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!sys_.Init(world_)) {
            skip_ = true;
            GTEST_SKIP() << "PhysX Init failed (DLL missing?)";
        }
    }
    void TearDown() override {
        if (!skip_) sys_.Shutdown();
    }

    /// 创建带 BoxCollider 的动态刚体，返回 entity
    entt::entity CreateDynamicBox(const glm::vec3& pos, const glm::vec3& size = glm::vec3(1.0f), float mass = 1.0f) {
        auto e = world_.registry().create();
        auto& t = world_.registry().emplace<TransformComponent>(e);
        t.position = pos;
        t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        auto& rb = world_.registry().emplace<RigidBody3DComponent>(e);
        rb.type = RigidBody3DType::Dynamic;
        rb.mass = mass;
        auto& box = world_.registry().emplace<BoxCollider3DComponent>(e);
        box.size = size;
        return e;
    }

    /// 创建静态地面（大平板）
    entt::entity CreateStaticGround(float y = 0.0f) {
        auto e = world_.registry().create();
        auto& t = world_.registry().emplace<TransformComponent>(e);
        t.position = glm::vec3(0.0f, y, 0.0f);
        t.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        auto& rb = world_.registry().emplace<RigidBody3DComponent>(e);
        rb.type = RigidBody3DType::Static;
        auto& box = world_.registry().emplace<BoxCollider3DComponent>(e);
        box.size = glm::vec3(100.0f, 1.0f, 100.0f);
        return e;
    }

    void StepN(int n, float dt = 1.0f / 60.0f) {
        for (int i = 0; i < n; ++i)
            sys_.FixedUpdate(world_, dt);
    }

    World world_;
    Physics3DSystem sys_;
    bool skip_ = false;
};

// ---- 测试用例 ----

TEST_F(Physics3DSmokeTest, TestCase2) {
    auto box = CreateDynamicBox(glm::vec3(0.0f, 10.0f, 0.0f));

    StepN(60); // 1 秒

    auto& t = world_.registry().get<TransformComponent>(box);
    // 受重力 g=9.81 下落 1 秒，理论位移 ~4.9m，位置应远低于 10
    EXPECT_LT(t.position.y, 6.0f);
}

TEST_F(Physics3DSmokeTest, TestCase3) {
    CreateStaticGround(0.0f);
    auto box = CreateDynamicBox(glm::vec3(0.0f, 5.0f, 0.0f));

    StepN(300); // 5 秒，足够落到地面并稳定

    auto& t = world_.registry().get<TransformComponent>(box);
    // 地面 y=0，box 半高 0.5 + 地面半高 0.5 → 停在 ~1.0 附近
    EXPECT_GT(t.position.y, -1.0f); // 不应穿过地面
    EXPECT_LT(t.position.y, 3.0f);  // 应该已下落
}

TEST_F(Physics3DSmokeTest, RaycasthitRigidBody) {
    CreateDynamicBox(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(2.0f));

    StepN(1); // 让 PhysX 创建 actor

    auto result = sys_.Raycast(
        glm::vec3(0.0f, 10.0f, 0.0f),  // 从上方
        glm::vec3(0.0f, -1.0f, 0.0f),  // 向下射线
        20.0f
    );
    EXPECT_TRUE(result.hit);
    EXPECT_LT(result.distance, 15.0f);
}

TEST_F(Physics3DSmokeTest, AddImpulseApplyImpulse) {
    auto box = CreateDynamicBox(glm::vec3(0.0f, 10.0f, 0.0f));

    StepN(1); // 创建 actor

    // 施加向上冲量抵抗重力
    sys_.AddImpulse(box, glm::vec3(0.0f, 50.0f, 0.0f));

    StepN(30); // 0.5 秒

    auto& t = world_.registry().get<TransformComponent>(box);
    // 冲量向上，0.5 秒后位置应高于初始位置或接近
    EXPECT_GT(t.position.y, 5.0f);
}

TEST_F(Physics3DSmokeTest, Event) {
    CreateStaticGround(0.0f);
    CreateDynamicBox(glm::vec3(0.0f, 2.0f, 0.0f));

    // 模拟多帧让刚体落到地面
    bool has_collision = false;
    for (int i = 0; i < 300; ++i) {
        sys_.FixedUpdate(world_, 1.0f / 60.0f);
        if (!sys_.GetCollisionEvents().empty()) {
            has_collision = true;
            break;
        }
    }
    EXPECT_TRUE(has_collision);
}

TEST_F(Physics3DSmokeTest, MultiContinuousFramesStable) {
    CreateStaticGround(0.0f);
    for (int i = 0; i < 5; ++i) {
        CreateDynamicBox(glm::vec3(static_cast<float>(i) * 2.0f, 10.0f, 0.0f));
    }

    // 10 秒模拟
    EXPECT_NO_THROW(StepN(600));
}

TEST_F(Physics3DSmokeTest, RemoveActorsafeRemoval) {
    auto box = CreateDynamicBox(glm::vec3(0.0f, 5.0f, 0.0f));

    StepN(1); // 创建 actor

    EXPECT_NO_THROW(sys_.RemoveActor(box));

    // 移除后继续模拟不崩溃
    EXPECT_NO_THROW(StepN(60));
}

TEST_F(Physics3DSmokeTest, RaycastEmptySceneMiss) {
    StepN(1);
    auto result = sys_.Raycast(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        100.0f
    );
    EXPECT_FALSE(result.hit);
}

#endif // DSE_ENABLE_PHYSX

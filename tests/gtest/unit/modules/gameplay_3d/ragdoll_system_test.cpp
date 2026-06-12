/**
 * @file ragdoll_system_test.cpp
 * @brief RagdollSystem 单元测试（无 PhysX 依赖的安全性测试）
 *
 * 测试策略：
 * - RagdollComponent 默认值验证
 * - RagdollSystem 构造安全
 * - SetAssetManager / SetPhysics3D nullptr 安全
 * - 空 World FixedUpdate 不崩溃
 * - 有 RagdollComponent 但未激活时 FixedUpdate 安全
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/ragdoll/ragdoll_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_physics.h"

using namespace dse;
using namespace dse::gameplay3d;

#ifdef DSE_ENABLE_PHYSX

TEST(RagdollComponentTest, DefaultValues) {
    RagdollComponent rc;
    EXPECT_FALSE(rc.active);
    EXPECT_TRUE(rc.auto_setup);
    EXPECT_FLOAT_EQ(rc.total_mass, 10.0f);
    EXPECT_FLOAT_EQ(rc.joint_stiffness, 0.0f);
    EXPECT_FLOAT_EQ(rc.joint_damping, 50.0f);
    EXPECT_EQ(rc.collision_layer, 0x0002);
    EXPECT_EQ(rc.collision_mask, 0xFFFF);
    EXPECT_TRUE(rc.bone_setups.empty());
    EXPECT_TRUE(rc.runtime_bones.empty());
    EXPECT_FALSE(rc.initialized);
}

TEST(RagdollSystemTest, Safety) {
    RagdollSystem sys;
    // 构造后不崩溃即可
}

TEST(RagdollSystemTest, SetNullptrSafety) {
    RagdollSystem sys;
    sys.SetAssetManager(nullptr);
    sys.SetPhysics3D(nullptr);
}

TEST(RagdollSystemTest, EmptyWorldDoesNotCrash) {
    RagdollSystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

TEST(RagdollSystemTest, ZerodtDoesNotCrash) {
    RagdollSystem sys;
    World world;
    sys.FixedUpdate(world, 0.0f);
}

TEST(RagdollSystemTest, WithNotRagdollDoesNotCrash) {
    RagdollSystem sys;
    World world;
    auto entity = world.registry().create();
    world.registry().emplace<RagdollComponent>(entity);
    // active=false，不触发物理创建
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

TEST(RagdollSystemTest, DeactivateSecurityForComponentlessEntities) {
    RagdollSystem sys;
    World world;
    auto entity = world.registry().create();
    // entity 没有 RagdollComponent，Deactivate 不应崩溃
    sys.Deactivate(world, entity);
}

#endif // DSE_ENABLE_PHYSX

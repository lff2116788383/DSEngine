/**
 * @file buoyancy_system_test.cpp
 * @brief BuoyancySystem 单元测试（无 PhysX 依赖的安全性测试）
 *
 * 测试策略：
 * - BuoyancyComponent / BuoyancySamplePoint 默认值验证
 * - BuoyancySystem 构造安全
 * - SetPhysics3D nullptr 安全
 * - 空 World FixedUpdate 不崩溃
 * - 有 BuoyancyComponent 实体时 FixedUpdate 安全（无 Physics3D 后端）
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/buoyancy/buoyancy_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_physics.h"

using namespace dse;
using namespace dse::gameplay3d;

#ifdef DSE_ENABLE_PHYSX

TEST(BuoyancySamplePointTest, DefaultValues) {
    BuoyancySamplePoint sp;
    EXPECT_FLOAT_EQ(sp.offset.x, 0.0f);
    EXPECT_FLOAT_EQ(sp.offset.y, 0.0f);
    EXPECT_FLOAT_EQ(sp.offset.z, 0.0f);
    EXPECT_FLOAT_EQ(sp.force_scale, 1.0f);
}

TEST(BuoyancyComponentTest, DefaultValues) {
    BuoyancyComponent bc;
    EXPECT_TRUE(bc.enabled);
    EXPECT_FLOAT_EQ(bc.water_level, 0.0f);
    EXPECT_TRUE(bc.use_fluid_system);
    EXPECT_FLOAT_EQ(bc.buoyancy_force, 10.0f);
    EXPECT_FLOAT_EQ(bc.water_drag, 3.0f);
    EXPECT_FLOAT_EQ(bc.water_angular_drag, 1.0f);
    EXPECT_FLOAT_EQ(bc.submerge_depth, 1.0f);
    EXPECT_TRUE(bc.sample_points.empty());
    EXPECT_FLOAT_EQ(bc.submerge_ratio, 0.0f);
}

TEST(BuoyancySystemTest, Safety) {
    BuoyancySystem sys;
}

TEST(BuoyancySystemTest, SetPhysics3D_NullptrSafety) {
    BuoyancySystem sys;
    sys.SetPhysics3D(nullptr);
}

TEST(BuoyancySystemTest, EmptyWorldDoesNotCrash) {
    BuoyancySystem sys;
    World world;
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

TEST(BuoyancySystemTest, ZerodtDoesNotCrash) {
    BuoyancySystem sys;
    World world;
    sys.FixedUpdate(world, 0.0f);
}

TEST(BuoyancySystemTest, WithBuoyancyComponentWithoutPhysics3DDoesNotCrash) {
    BuoyancySystem sys;
    // physics3d_ == nullptr → FixedUpdate 内应安全退出
    World world;
    auto entity = world.registry().create();
    world.registry().emplace<BuoyancyComponent>(entity);
    sys.FixedUpdate(world, 1.0f / 60.0f);
}

TEST(BuoyancyComponentTest, AddTosamplingpoint) {
    BuoyancyComponent bc;
    bc.sample_points.push_back({glm::vec3(0, -0.5f, 0), 1.5f});
    bc.sample_points.push_back({glm::vec3(1, -0.5f, 0), 0.5f});
    EXPECT_EQ(bc.sample_points.size(), 2u);
    EXPECT_FLOAT_EQ(bc.sample_points[0].force_scale, 1.5f);
    EXPECT_FLOAT_EQ(bc.sample_points[1].offset.x, 1.0f);
}

#endif // DSE_ENABLE_PHYSX

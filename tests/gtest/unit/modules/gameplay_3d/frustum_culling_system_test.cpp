/**
 * @file frustum_culling_system_test.cpp
 * @brief FrustumCullingSystem 视锥体剔除系统的单元测试
 *
 * 覆盖场景：
 * - Update 调用不崩溃（空 World）
 * - 带实体的 Update
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/frustum_culling_system.h"
#include "engine/ecs/world.h"

using namespace dse::gameplay3d;

TEST(FrustumCullingSystemTest, 空World调用Update不崩溃) {
    World world;
    FrustumCullingSystem sys;
    sys.Update(world);
}

TEST(FrustumCullingSystemTest, 带实体World调用Update不崩溃) {
    World world;
    FrustumCullingSystem sys;
    auto e = world.CreateEntity();
    sys.Update(world);
}

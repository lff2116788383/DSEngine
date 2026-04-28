/**
* @file tilemap_system_test.cpp
* @brief 瓦片地图系统单元测试，验证 TilemapSystem 对空/有实体 World 的 Update 行为
*/

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/tilemap.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"

using namespace dse;
using namespace gameplay2d;

TEST(TilemapSystemTest, 空World调用Update不崩溃) {
    World world;
    TilemapSystem system;
    // 空 World 调用 Update 不应崩溃
    EXPECT_NO_THROW(system.Update(world.registry()));
}

TEST(TilemapSystemTest, 带TilemapComponent实体Update不崩溃) {
    World world;
    TilemapSystem system;
    auto entity = world.CreateEntity();
    world.registry().emplace<TilemapComponent>(entity);
    // 有 TilemapComponent 的实体调用 Update 不应崩溃
    EXPECT_NO_THROW(system.Update(world.registry()));
}

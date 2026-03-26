#include "catch/catch.hpp"
#include "engine/ecs/world.h"

// 正向测试：创建与销毁实体应正确更新存活状态与计数。
TEST_CASE("Given_CreatedEntities_When_DestroyOne_Then_AliveStateAndCountAreConsistent", "[engine][unit][world]") {
    auto& world = World::Instance();
    world.Clear();

    const Entity e1 = world.CreateEntity();
    const Entity e2 = world.CreateEntity();
    REQUIRE(world.EntityCount() == 2);
    REQUIRE(world.IsAlive(e1));
    REQUIRE(world.IsAlive(e2));

    world.DestroyEntity(e1);
    REQUIRE(world.EntityCount() == 1);
    REQUIRE_FALSE(world.IsAlive(e1));
    REQUIRE(world.IsAlive(e2));
}

// 边界测试：Clear 后世界应为空，重复 Clear 不应产生额外副作用。
TEST_CASE("Given_ClearedWorld_When_ClearAgain_Then_CountRemainsZero", "[engine][unit][world]") {
    auto& world = World::Instance();
    world.Clear();
    world.CreateEntity();
    REQUIRE(world.EntityCount() == 1);

    world.Clear();
    REQUIRE(world.EntityCount() == 0);

    world.Clear();
    REQUIRE(world.EntityCount() == 0);
}

// 反向测试：销毁无效实体时应安全返回且不影响现有实体计数。
TEST_CASE("Given_InvalidEntity_When_Destroy_Then_NoCountCorruption", "[engine][unit][world]") {
    auto& world = World::Instance();
    world.Clear();
    world.CreateEntity();
    REQUIRE(world.EntityCount() == 1);

    world.DestroyEntity(entt::null);
    REQUIRE(world.EntityCount() == 1);
}

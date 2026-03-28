#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：地形组件的默认参数。
TEST_CASE("Given_DefaultTerrainComponent_When_Created_Then_IsDirtyIsTrue", "[engine][unit][terrain]") {
    TerrainComponent terrain;
    REQUIRE(terrain.enabled == true);
    REQUIRE(terrain.is_dirty == true);
    REQUIRE(terrain.width == 100.0f);
    REQUIRE(terrain.depth == 100.0f);
    REQUIRE(terrain.max_height == 20.0f);
    REQUIRE(terrain.resolution_x == 64);
    REQUIRE(terrain.resolution_z == 64);
    REQUIRE(terrain.use_dynamic_lod == true);
    REQUIRE(terrain.max_lod_levels == 4);
    REQUIRE(terrain.current_lod == 0);
    REQUIRE(terrain.vao == 0);
}

// 边界测试：配置极端尺寸的地形组件。
TEST_CASE("Given_TerrainComponent_When_SetExtremeDimensions_Then_ValuesAreUpdated", "[engine][unit][terrain]") {
    TerrainComponent terrain;
    terrain.width = 10000.0f;
    terrain.depth = 10000.0f;
    terrain.resolution_x = 1024;
    terrain.resolution_z = 1024;
    
    REQUIRE(terrain.width == 10000.0f);
    REQUIRE(terrain.resolution_x == 1024);
}

// 反向测试：处理空高度数据的情况
TEST_CASE("Given_TerrainComponent_When_HeightDataIsEmpty_Then_HandledSafely", "[engine][unit][terrain]") {
    TerrainComponent terrain;
    terrain.height_data.clear();
    
    REQUIRE(terrain.height_data.empty());
    REQUIRE(terrain.is_dirty == true);
}

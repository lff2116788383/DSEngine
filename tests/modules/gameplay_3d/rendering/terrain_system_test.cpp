#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"
#include "engine/render/rhi/rhi_device.h"
#include "modules/gameplay_3d/rendering/terrain_system.h"

using namespace dse;

namespace {

class RecordingCommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc&) override {}
    void EndRenderPass() override {}
    void SetPipelineState(unsigned int) override {}
    void SetCamera(const glm::mat4&, const glm::mat4&) override {}
    void DrawBatch(const std::vector<DrawBatchItem>&) override {}
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override {
        draw_mesh_batch_calls++;
        last_mesh_batch = items;
    }
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>&) override {}
    void ClearColor(const glm::vec4&) override {}
    void SetGlobalMat4(const std::string&, const glm::mat4&) override {}
    void SetGlobalMat4Array(const std::string&, const std::vector<glm::mat4>&) override {}
    void SetGlobalFloatArray(const std::string&, const std::vector<float>&) override {}
    void DrawSkybox(unsigned int) override {}
    void DrawPostProcess(unsigned int, const std::string&, const std::vector<float>&) override {}
    void DrawParticles3D(const std::vector<Particle3DDrawItem>&, const glm::mat4&, const glm::mat4&) override {}

    int draw_mesh_batch_calls = 0;
    std::vector<MeshDrawItem> last_mesh_batch;
};

} // namespace

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

TEST_CASE("Given_DirtyTerrain_When_Rendered_Then_BoundingBoxAndMeshBatchAreGenerated", "[engine][unit][terrain][3d]") {
    World world;
    gameplay3d::TerrainSystem system;
    RecordingCommandBuffer cmd;

    const auto camera = world.CreateEntity();
    auto& camera_transform = world.registry().emplace<TransformComponent>(camera);
    camera_transform.local_to_world = glm::mat4(1.0f);
    world.registry().emplace<Camera3DComponent>(camera);

    const auto terrain_entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(terrain_entity);
    transform.local_to_world = glm::mat4(1.0f);

    auto& terrain = world.registry().emplace<TerrainComponent>(terrain_entity);
    terrain.width = 20.0f;
    terrain.depth = 10.0f;
    terrain.max_height = 3.0f;
    terrain.resolution_x = 4;
    terrain.resolution_z = 4;
    terrain.max_lod_levels = 3;
    terrain.is_dirty = true;
    terrain.use_dynamic_lod = false;
    terrain.visible = true;

    system.Render(world, cmd);

    REQUIRE_FALSE(terrain.is_dirty);
    REQUIRE(world.registry().all_of<BoundingBoxComponent>(terrain_entity));
    const auto& bbox = world.registry().get<BoundingBoxComponent>(terrain_entity);
    REQUIRE(bbox.min_extents.x == Approx(-10.0f));
    REQUIRE(bbox.min_extents.y == Approx(0.0f));
    REQUIRE(bbox.min_extents.z == Approx(-5.0f));
    REQUIRE(bbox.max_extents.x == Approx(10.0f));
    REQUIRE(bbox.max_extents.y == Approx(3.0f));
    REQUIRE(bbox.max_extents.z == Approx(5.0f));

    REQUIRE(cmd.draw_mesh_batch_calls == 1);
    REQUIRE(cmd.last_mesh_batch.size() == 1);
    const auto& item = cmd.last_mesh_batch.front();
    REQUIRE(item.vertices.size() == 16);
    REQUIRE(item.indices.size() == 24);
    REQUIRE(item.receive_shadow == true);
    REQUIRE(item.lighting_enabled == true);
}

TEST_CASE("Given_DynamicLodTerrain_When_CameraFarAway_Then_CurrentLodIsClampedAndCoarserIndicesAreUsed", "[engine][unit][terrain][3d]") {
    World world;
    gameplay3d::TerrainSystem system;
    RecordingCommandBuffer cmd;

    const auto camera = world.CreateEntity();
    auto& camera_transform = world.registry().emplace<TransformComponent>(camera);
    camera_transform.local_to_world = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 400.0f));
    world.registry().emplace<Camera3DComponent>(camera);

    const auto terrain_entity = world.CreateEntity();
    auto& transform = world.registry().emplace<TransformComponent>(terrain_entity);
    transform.local_to_world = glm::mat4(1.0f);

    auto& terrain = world.registry().emplace<TerrainComponent>(terrain_entity);
    terrain.width = 32.0f;
    terrain.depth = 32.0f;
    terrain.resolution_x = 9;
    terrain.resolution_z = 9;
    terrain.max_lod_levels = 4;
    terrain.lod_distance_factor = 50.0f;
    terrain.use_dynamic_lod = true;
    terrain.is_dirty = false;
    terrain.visible = true;
    terrain.height_data.assign(terrain.resolution_x * terrain.resolution_z, 0.0f);

    system.Render(world, cmd);

    REQUIRE(terrain.current_lod == 3);
    REQUIRE(cmd.draw_mesh_batch_calls == 1);
    REQUIRE(cmd.last_mesh_batch.size() == 1);
    const auto& item = cmd.last_mesh_batch.front();
    REQUIRE(item.vertices.size() == 81);
    REQUIRE(item.indices.empty());
}

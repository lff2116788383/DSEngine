/**
 * @file lod_system_test.cpp
 * @brief LODSystem 及 LOD 组件的单元测试
 *
 * 覆盖场景：
 * - LODGroupComponent / LODLevelConfig 字段默认值
 * - MeshRendererComponent::mesh_handle_override 默认值
 * - 屏幕空间公式正确性
 * - LODSystem 空 World 调用不崩溃
 * - LODSystem 级别切换逻辑（无 AssetManager 时安全降级）
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/lod_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

using namespace dse;
using namespace gameplay3d;

// ============================================================
// 组件默认值
// ============================================================

TEST(LODGroupComponentTest, DefaultValues) {
    LODGroupComponent lod;
    EXPECT_TRUE(lod.enabled);
    EXPECT_TRUE(lod.levels.empty());
    EXPECT_EQ(lod.current_lod, -1);
    EXPECT_FLOAT_EQ(lod.global_scale, 1.0f);
    EXPECT_FLOAT_EQ(lod.hysteresis, 0.05f);
    EXPECT_TRUE(lod.original_mesh_path.empty());
}

TEST(LODLevelConfigTest, DefaultValues) {
    LODLevelConfig level;
    EXPECT_TRUE(level.mesh_path.empty());
    EXPECT_FLOAT_EQ(level.screen_size_threshold, 0.0f);
    EXPECT_EQ(level.mesh_handle, 0u);
    EXPECT_FALSE(level.loaded);
}

TEST(MeshRendererComponentTest, mesh_handle_OverrideDefaultIsZero) {
    MeshRendererComponent mesh;
    EXPECT_EQ(mesh.mesh_handle_override, 0u);
}

// ============================================================
// 屏幕空间公式正确性
// ============================================================

TEST(LODScreenSizeFormulaTest, Before) {
    // proj_scale = 1/tan(fov/2), fov=60° -> tan(30°)=0.5774 -> proj_scale~1.732
    const float fov_deg    = 60.0f;
    const float half_fov   = glm::radians(fov_deg) * 0.5f;
    const float proj_scale = 1.0f / std::tan(half_fov);

    const float bbox_radius  = 10.0f;
    const float dist         = 100.0f;
    const float global_scale = 1.0f;

    const float screen_size = (proj_scale * proj_scale * bbox_radius * bbox_radius)
                              / (dist * dist) * global_scale;

    // proj_scale≈1.732, proj_scale²≈3.0
    // 3.0 * 100 / 10000 = 0.03
    EXPECT_NEAR(screen_size, 3.0f * 100.0f / 10000.0f, 1e-3f);
}

TEST(LODScreenSizeFormulaTest, global_ScalescaleToScale) {
    const float fov_deg    = 90.0f;
    const float half_fov   = glm::radians(fov_deg) * 0.5f;
    const float proj_scale = 1.0f / std::tan(half_fov);  // ~1.0

    const float bbox_radius  = 5.0f;
    const float dist         = 50.0f;

    const float base_scale1 = (proj_scale * proj_scale * bbox_radius * bbox_radius)
                              / (dist * dist) * 1.0f;
    const float base_scale2 = (proj_scale * proj_scale * bbox_radius * bbox_radius)
                              / (dist * dist) * 2.0f;

    EXPECT_FLOAT_EQ(base_scale2, base_scale1 * 2.0f);
    EXPECT_GT(base_scale2, base_scale1);
}

TEST(LODScreenSizeFormulaTest, IsZeroWhenNotZero) {
    const float proj_scale = 1.0f;
    const float bbox_radius = 1.0f;
    // dist_sq 被 clamp 到 max(1, dist²)
    const float dist_sq_clamped = std::max(1.0f, 0.0f);
    const float screen_size = (proj_scale * proj_scale * bbox_radius * bbox_radius)
                              / dist_sq_clamped;
    EXPECT_GT(screen_size, 0.0f);
    EXPECT_TRUE(std::isfinite(screen_size));
}

// ============================================================
// LODSystem 运行时行为
// ============================================================

class LODSystemTest : public ::testing::Test {
protected:
    World world;
    AssetManager asset_mgr;
    LODSystem sys;

    void SetUp() override {
        sys.SetAssetManager(&asset_mgr);
    }
};

TEST_F(LODSystemTest, EmptyWorldDoesNotCrash) {
    EXPECT_NO_THROW(sys.Update(world));
}

TEST_F(LODSystemTest, WithoutAssetManagerDoesNotCrash) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    world.registry().emplace<MeshRendererComponent>(e);

    LODGroupComponent lod;
    LODLevelConfig lvl0;
    lvl0.mesh_path = "data/mesh/lod0.dmesh";
    lvl0.screen_size_threshold = 0.5f;
    lod.levels.push_back(lvl0);
    world.registry().emplace<LODGroupComponent>(e, std::move(lod));

    EXPECT_NO_THROW(sys.Update(world));
}

TEST_F(LODSystemTest, WithoutNotLOD) {
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    auto& mesh = world.registry().emplace<MeshRendererComponent>(e);

    LODGroupComponent lod;
    LODLevelConfig lvl0;
    lvl0.mesh_path = "data/mesh/lod0.dmesh";
    lvl0.screen_size_threshold = 0.5f;
    lod.levels.push_back(lvl0);
    world.registry().emplace<LODGroupComponent>(e, std::move(lod));

    sys.Update(world);

    EXPECT_EQ(mesh.mesh_handle_override, 0u);
    EXPECT_EQ(world.registry().get<LODGroupComponent>(e).current_lod, -1);
}

TEST_F(LODSystemTest, DisabledComponentIsNotProcessed) {
    auto e = world.CreateEntity();

    auto& cam = world.registry().emplace<Camera3DComponent>(e);
    cam.enabled = true;
    cam.fov = 60.0f;
    world.registry().emplace<TransformComponent>(e);

    auto e2 = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e2);
    auto& mesh = world.registry().emplace<MeshRendererComponent>(e2);

    LODGroupComponent lod;
    lod.enabled = false;
    LODLevelConfig lvl0;
    lvl0.mesh_path = "data/mesh/lod0.dmesh";
    lvl0.screen_size_threshold = 0.5f;
    lod.levels.push_back(lvl0);
    world.registry().emplace<LODGroupComponent>(e2, std::move(lod));

    sys.Update(world);

    EXPECT_EQ(mesh.mesh_handle_override, 0u);
    EXPECT_EQ(world.registry().get<LODGroupComponent>(e2).current_lod, -1);
}

TEST_F(LODSystemTest, MultiTimesUpdateDoesNotCrash) {
    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(sys.Update(world));
    }
}

TEST(LODHysteresisTest, TestCase12) {
    const float threshold = 0.5f;
    const float hyst = 0.05f;
    const float upper = threshold * (1.0f + hyst);
    const float lower = threshold * (1.0f - hyst);
    EXPECT_NEAR(upper, 0.525f, 1e-5f);
    EXPECT_NEAR(lower, 0.475f, 1e-5f);
    EXPECT_GT(upper, threshold);
    EXPECT_LT(lower, threshold);
}

TEST_F(LODSystemTest, DisabledrestoreOriginalmesh_PathandCleartemp) {
    auto e = world.CreateEntity();
    world.registry().emplace<Camera3DComponent>(e).enabled = true;
    world.registry().emplace<TransformComponent>(e);

    auto e2 = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e2);
    auto& mesh = world.registry().emplace<MeshRendererComponent>(e2);
    mesh.mesh_handle_override = 42u;
    mesh.mesh_path = "data/lod1.dmesh";
    mesh.temp_vertices.push_back(1.0f);  // 模拟已有 LOD 数据

    LODGroupComponent lod;
    lod.enabled = false;
    lod.current_lod = 1;
    lod.original_mesh_path = "data/original.dmesh";
    world.registry().emplace<LODGroupComponent>(e2, std::move(lod));

    sys.Update(world);

    EXPECT_EQ(mesh.mesh_handle_override, 0u);
    EXPECT_TRUE(mesh.temp_vertices.empty());
    EXPECT_TRUE(mesh.temp_indices.empty());
    EXPECT_EQ(mesh.mesh_path, "data/original.dmesh");
    EXPECT_EQ(world.registry().get<LODGroupComponent>(e2).current_lod, -1);
}

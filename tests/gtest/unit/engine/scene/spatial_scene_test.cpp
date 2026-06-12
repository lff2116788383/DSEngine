/**
 * @file spatial_scene_test.cpp
 * @brief SpatialScene 动静态 Octree 分离 + VisibleSet 分层可见集 单元测试
 *
 * 覆盖：
 * - BuildStatic 正确区分动态/静态实体
 * - CullFrustum 输出分层 VisibleSet
 * - 动态实体不在静态树中
 * - Invalidate 触发重建
 * - MarkStatic/MarkDynamic 手动切换
 * - 空场景不崩溃
 * - VisibleSet 分类正确（opaque / transparent）
 */

#include <gtest/gtest.h>
#include "engine/scene/spatial_scene.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/transform.h"
#include <glm/gtc/matrix_transform.hpp>

using namespace dse;
using namespace dse::scene;

class SpatialSceneTest : public ::testing::Test {
protected:
    /// 创建一个带 BoundingBox 的静态实体
    entt::entity CreateStaticEntity(const glm::vec3& pos, const glm::vec3& extents = glm::vec3(1.0f)) {
        auto e = world_.registry().create();
        auto& t = world_.registry().emplace<TransformComponent>(e);
        t.position = pos;
        t.local_to_world = glm::translate(glm::mat4(1.0f), pos);
        auto& bb = world_.registry().emplace<BoundingBoxComponent>(e);
        bb.min_extents = -extents;
        bb.max_extents = extents;
        auto& mr = world_.registry().emplace<MeshRendererComponent>(e);
        mr.visible = false;
        mr.color = glm::vec4(1.0f);
        mr.shader_variant = "MESH_PBR";
        return e;
    }

    /// 创建一个带 Dynamic RigidBody 的实体
    entt::entity CreateDynamicEntity(const glm::vec3& pos, const glm::vec3& extents = glm::vec3(1.0f)) {
        auto e = CreateStaticEntity(pos, extents);
        auto& rb = world_.registry().emplace<RigidBody3DComponent>(e);
        rb.type = RigidBody3DType::Dynamic;
        rb.mass = 1.0f;
        return e;
    }

    /// 创建一个半透明实体
    entt::entity CreateTransparentEntity(const glm::vec3& pos) {
        auto e = CreateStaticEntity(pos);
        auto& mr = world_.registry().get<MeshRendererComponent>(e);
        mr.color.a = 0.5f;
        return e;
    }

    /// 构造一个朝 -Z 看的 view_proj 矩阵
    glm::mat4 MakeViewProj(const glm::vec3& eye = glm::vec3(0, 0, 50),
                           const glm::vec3& target = glm::vec3(0, 0, 0)) {
        auto view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
        auto proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 200.0f);
        return proj * view;
    }

    World world_;
    SpatialScene spatial_;
    VisibleSet visible_;
};

// ---- 基础功能 ----

TEST_F(SpatialSceneTest, EmptySceneBuildStaticDoesNotCrash) {
    EXPECT_NO_THROW(spatial_.BuildStatic(world_));
    // 空场景无 BoundingBoxComponent 实体时 IsBuilt() 返回 false，
    // 这是预期行为：允许 FrustumCullingSystem 下一帧重试以捕获异步加载的实体
    EXPECT_FALSE(spatial_.IsBuilt());
    EXPECT_EQ(spatial_.GetStaticCount(), 0u);
    EXPECT_EQ(spatial_.GetDynamicCount(), 0u);
}

TEST_F(SpatialSceneTest, EmptySceneCullFrustumDoesNotCrash) {
    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    EXPECT_NO_THROW(spatial_.CullFrustum(MakeViewProj(), world_, visible_));
    EXPECT_EQ(visible_.TotalCount(), 0u);
}

TEST_F(SpatialSceneTest, Entity) {
    CreateStaticEntity(glm::vec3(0, 0, 0));
    CreateStaticEntity(glm::vec3(5, 0, 0));

    spatial_.BuildStatic(world_);
    EXPECT_EQ(spatial_.GetStaticCount(), 2u);
    EXPECT_EQ(spatial_.GetDynamicCount(), 0u);
}

TEST_F(SpatialSceneTest, RigidBody) {
    CreateStaticEntity(glm::vec3(0, 0, 0));
    CreateDynamicEntity(glm::vec3(5, 0, 0));

    spatial_.BuildStatic(world_);
    EXPECT_EQ(spatial_.GetStaticCount(), 1u);
    EXPECT_EQ(spatial_.GetDynamicCount(), 1u);
}

// ---- 剔除正确性 ----

TEST_F(SpatialSceneTest, FrustumInsideEntitymarkedAsCan) {
    auto e = CreateStaticEntity(glm::vec3(0, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    auto& mr = world_.registry().get<MeshRendererComponent>(e);
    EXPECT_TRUE(mr.visible);
    EXPECT_EQ(visible_.opaque.size(), 1u);
}

TEST_F(SpatialSceneTest, FrustumOutsideEntitymarkedAsNotCan) {
    auto e = CreateStaticEntity(glm::vec3(0, 0, -300));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    auto& mr = world_.registry().get<MeshRendererComponent>(e);
    EXPECT_FALSE(mr.visible);
    EXPECT_EQ(visible_.opaque.size(), 0u);
}

TEST_F(SpatialSceneTest, EntityFrustumInsideCan) {
    auto e = CreateDynamicEntity(glm::vec3(0, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    auto& mr = world_.registry().get<MeshRendererComponent>(e);
    EXPECT_TRUE(mr.visible);
    EXPECT_GE(visible_.opaque.size(), 1u);
}

// ---- 分层可见集 ----

TEST_F(SpatialSceneTest, OpaqueEntitiesFallUnderopaqueset) {
    CreateStaticEntity(glm::vec3(0, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    EXPECT_EQ(visible_.opaque.size(), 1u);
    EXPECT_EQ(visible_.transparent.size(), 0u);
}

TEST_F(SpatialSceneTest, HalfEntitytransparentset) {
    CreateTransparentEntity(glm::vec3(0, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    EXPECT_EQ(visible_.opaque.size(), 0u);
    EXPECT_EQ(visible_.transparent.size(), 1u);
}

TEST_F(SpatialSceneTest, SceneCorrect) {
    CreateStaticEntity(glm::vec3(0, 0, 0));
    CreateTransparentEntity(glm::vec3(3, 0, 0));
    CreateDynamicEntity(glm::vec3(-3, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    // 2 opaque (1 static + 1 dynamic) + 1 transparent
    EXPECT_EQ(visible_.opaque.size(), 2u);
    EXPECT_EQ(visible_.transparent.size(), 1u);
}

TEST_F(SpatialSceneTest, OpaqueObjectsCastShadowsByDefault) {
    CreateStaticEntity(glm::vec3(0, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    EXPECT_EQ(visible_.shadow_casters.size(), 1u);
}

TEST_F(SpatialSceneTest, HalfNot) {
    CreateTransparentEntity(glm::vec3(0, 0, 0));

    spatial_.BuildStatic(world_);
    spatial_.UpdateDynamicBounds(world_);
    spatial_.CullFrustum(MakeViewProj(), world_, visible_);

    EXPECT_EQ(visible_.shadow_casters.size(), 0u);
}

// ---- Invalidate 和重建 ----

TEST_F(SpatialSceneTest, InvalidateTriggerNextRebuild) {
    CreateStaticEntity(glm::vec3(0, 0, 0));
    spatial_.BuildStatic(world_);
    EXPECT_TRUE(spatial_.IsBuilt());

    spatial_.Invalidate();
    EXPECT_FALSE(spatial_.IsBuilt());
}

TEST_F(SpatialSceneTest, MarkDynamicRebuildAfterSwitching) {
    auto e = CreateStaticEntity(glm::vec3(0, 0, 0));
    spatial_.BuildStatic(world_);
    EXPECT_EQ(spatial_.GetStaticCount(), 1u);
    EXPECT_EQ(spatial_.GetDynamicCount(), 0u);

    spatial_.MarkDynamic(e);
    EXPECT_FALSE(spatial_.IsBuilt());

    spatial_.BuildStatic(world_);
    // 注意：BuildStatic 会重新根据组件自动判定动/静，
    // 由于实体没有 RigidBody3D，它会重新被归为静态
    EXPECT_EQ(spatial_.GetStaticCount(), 1u);
}

// ---- 多帧稳定性 ----

TEST_F(SpatialSceneTest, MultiContinuousFramesDoesNotCrash) {
    for (int i = 0; i < 20; ++i) {
        CreateStaticEntity(glm::vec3(static_cast<float>(i) * 3.0f, 0, 0));
    }
    for (int i = 0; i < 5; ++i) {
        CreateDynamicEntity(glm::vec3(static_cast<float>(i) * -3.0f, 0, 0));
    }

    spatial_.BuildStatic(world_);
    for (int frame = 0; frame < 100; ++frame) {
        spatial_.UpdateDynamicBounds(world_);
        EXPECT_NO_THROW(spatial_.CullFrustum(MakeViewProj(), world_, visible_));
    }
    EXPECT_GT(visible_.TotalCount(), 0u);
}

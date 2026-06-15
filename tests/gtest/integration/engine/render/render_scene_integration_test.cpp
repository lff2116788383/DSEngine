/**
 * @file render_scene_integration_test.cpp
 * @brief Render ↔ Scene 集成测试
 *
 * 验证场景：
 * - RenderGraph 声明资源、添加 Pass、编译与执行的基础链路
 * - Scene 变更（添加/移除实体）不影响 RenderGraph 稳定性
 * - MeshRendererComponent 与场景实体的关联
 * - PostProcessComponent 参数设置
 * - 光源组件（Directional/Point/Spot）默认值
 *
 * 注意：RenderGraph 真实执行需要 OpenGL 上下文，本测试覆盖
 *       组件数据层和 DAG 构建逻辑，不含 GPU 渲染验证。
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/render/render_graph.h"
#include "engine/scene/transform_system.h"

using namespace dse;

class RenderSceneIntegrationTest : public ::testing::Test {
protected:
    World world;
};

// 测试 渲染场景集成：渲染图Declare资源且添加通道
TEST_F(RenderSceneIntegrationTest, RenderGraphDeclareResourcesAndAddPass) {
    dse::render::RenderGraph dag;
    auto res1 = dag.DeclareResource("color");
    auto res2 = dag.DeclareResource("depth");
    auto pass = dag.AddPass("test_pass");
    dag.PassWrite(pass, res1);
    dag.PassRead(pass, res2);
    dag.MarkOutput(res1);

    bool compiled = dag.Compile();
    EXPECT_TRUE(compiled);
}

// 测试 渲染场景集成：渲染图编译空图
TEST_F(RenderSceneIntegrationTest, RenderGraphCompileEmptyGraph) {
    dse::render::RenderGraph dag;
    bool compiled = dag.Compile();
    EXPECT_TRUE(compiled);
}

// 测试 渲染场景集成：渲染图能够应重建之后重置
TEST_F(RenderSceneIntegrationTest, RenderGraphCanBeRebuiltAfterReset) {
    dse::render::RenderGraph dag;
    auto res = dag.DeclareResource("color");
    auto pass = dag.AddPass("p1");
    dag.PassWrite(pass, res);
    dag.MarkOutput(res);
    EXPECT_TRUE(dag.Compile());

    dag.Reset();
    // 重新构建不同结构
    auto res2 = dag.DeclareResource("new_color");
    auto pass2 = dag.AddPass("p2");
    dag.PassWrite(pass2, res2);
    dag.MarkOutput(res2);
    EXPECT_TRUE(dag.Compile());
}

// 测试 渲染场景集成：网格渲染器组件默认值
TEST_F(RenderSceneIntegrationTest, MeshRendererComponentDefaultValues) {
    MeshRendererComponent mesh;
    EXPECT_TRUE(mesh.visible);
    EXPECT_TRUE(mesh.receive_shadow);
    EXPECT_TRUE(mesh.depth_test_enabled);
    EXPECT_TRUE(mesh.depth_write_enabled);
    EXPECT_FLOAT_EQ(mesh.color.r, 1.0f);
    EXPECT_EQ(mesh.shader_variant, "MESH_UNLIT");
    EXPECT_EQ(mesh.material_data_source, MeshRendererComponent::MaterialDataSource::ComponentFallback);
}

// 测试 渲染场景集成：后期处理组件默认值
TEST_F(RenderSceneIntegrationTest, PostProcessComponentDefaultValues) {
    PostProcessComponent pp;
    EXPECT_TRUE(pp.enabled);
    EXPECT_TRUE(pp.bloom_enabled);
    EXPECT_FLOAT_EQ(pp.bloom_threshold, 1.0f);
    EXPECT_FLOAT_EQ(pp.bloom_intensity, 0.5f);
    EXPECT_FLOAT_EQ(pp.exposure, 1.0f);
    EXPECT_FLOAT_EQ(pp.gamma, 2.2f);
    EXPECT_FALSE(pp.ssao_enabled);
}

// 测试 渲染场景集成：方向光灯光3D组件默认值
TEST_F(RenderSceneIntegrationTest, DirectionalLight3DComponentDefaultValues) {
    DirectionalLight3DComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_TRUE(light.cast_shadow);
    EXPECT_FLOAT_EQ(light.intensity, 1.0f);
    EXPECT_EQ(light.direction.y, -1.0f);
}

// 测试 渲染场景集成：点灯光组件默认值
TEST_F(RenderSceneIntegrationTest, PointLightComponentDefaultValues) {
    PointLightComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.radius, 10.0f);
    EXPECT_FALSE(light.cast_shadow);
}

// 测试 渲染场景集成：聚光灯光组件默认值
TEST_F(RenderSceneIntegrationTest, SpotLightComponentDefaultValues) {
    SpotLightComponent light;
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.radius, 20.0f);
    EXPECT_FLOAT_EQ(light.inner_cone_angle, 12.5f);
    EXPECT_FLOAT_EQ(light.outer_cone_angle, 17.5f);
    EXPECT_FALSE(light.cast_shadow);
}

// 测试 渲染场景集成：场景实体不渲染图
TEST_F(RenderSceneIntegrationTest, SceneEntityNotRenderGraph) {
    dse::render::RenderGraph dag;
    auto res = dag.DeclareResource("scene_color");
    auto pass = dag.AddPass("scene");
    dag.PassWrite(pass, res);
    dag.MarkOutput(res);
    ASSERT_TRUE(dag.Compile());

    // 添加实体
    for (int i = 0; i < 5; ++i) {
        auto e = world.CreateEntity();
        world.registry().emplace<TransformComponent>(e);
        world.registry().emplace<MeshRendererComponent>(e);
    }

    // RenderGraph 仍可编译
    EXPECT_TRUE(dag.Compile());

    // 移除实体
    world.Clear();
    EXPECT_TRUE(dag.Compile());
}

// 测试 渲染场景集成：变换系统之后更新网格Rendererconstant
TEST_F(RenderSceneIntegrationTest, TransformSystemAfterUpdateMeshRendererconstant) {
    auto e = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(e);
    tf.position = glm::vec3(1.0f, 2.0f, 3.0f);
    tf.dirty = true;
    auto& mesh = world.registry().emplace<MeshRendererComponent>(e);
    mesh.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    TransformSystem transform_sys;
    transform_sys.Update(world);

    // MeshRenderer 不应被 TransformSystem 修改
    EXPECT_FLOAT_EQ(world.registry().get<MeshRendererComponent>(e).color.r, 1.0f);
    EXPECT_FLOAT_EQ(world.registry().get<MeshRendererComponent>(e).color.g, 0.0f);
}

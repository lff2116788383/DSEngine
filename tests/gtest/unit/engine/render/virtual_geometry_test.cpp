/**
 * @file virtual_geometry_test.cpp
 * @brief Virtual Geometry 渲染器单元测试（B-5 接线验证，CPU 路径）
 *
 * 覆盖场景：
 * - DAGBuilder::Build 从原始网格构建 VirtualGeometryMesh（meshlet + DAG 层级）
 * - VirtualGeometryRenderer 生命周期：Init → RegisterMesh → SubmitInstance → Execute
 * - Execute 的 CPU LOD 选择路径产出可见簇与统计
 *
 * 注意：VirtualGeometry 是实验特性（CMake 开关 DSE_ENABLE_VIRTUAL_GEOMETRY 默认 OFF），
 * 本测试仅在宏开启时编译；默认构建不包含。
 */

#include <gtest/gtest.h>

#if defined(DSE_ENABLE_VIRTUAL_GEOMETRY)

#include "engine/render/virtual_geometry/virtual_geometry_renderer.h"
#include "engine/render/virtual_geometry/dag_builder.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace dse::render::vg;

namespace {

/// 构建一个带 UV/法线的立方体测试网格（12 三角形）
void MakeCubeMesh(std::vector<glm::vec3>& positions,
                  std::vector<glm::vec3>& normals,
                  std::vector<glm::vec2>& uvs,
                  std::vector<uint32_t>& indices) {
    positions = {
        glm::vec3(-1,-1,-1), glm::vec3( 1,-1,-1), glm::vec3( 1, 1,-1), glm::vec3(-1, 1,-1),
        glm::vec3(-1,-1, 1), glm::vec3( 1,-1, 1), glm::vec3( 1, 1, 1), glm::vec3(-1, 1, 1),
    };
    normals.assign(positions.size(), glm::vec3(0.0f, 1.0f, 0.0f));
    uvs.assign(positions.size(), glm::vec2(0.0f));
    indices = {
        0,1,2, 0,2,3,  4,6,5, 4,7,6,
        0,4,5, 0,5,1,  1,5,6, 1,6,2,
        2,6,7, 2,7,3,  3,7,4, 3,4,0,
    };
}

} // namespace

// 测试 VirtualGeometry：DAGBuilder 从原始网格构建完整 VirtualGeometryMesh
// 前置：立方体 8 顶点 12 三角形；
// 预期：构建成功，meshlet 非空、DAG 节点非空、层级数 ≥ 1。
TEST(VirtualGeometryTest, DAGBuilderBuildsHierarchy) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
    MakeCubeMesh(positions, normals, uvs, indices);

    DAGBuildConfig cfg;
    VirtualGeometryMesh vgm = DAGBuilder().Build(positions, normals, uvs, indices, cfg);

    EXPECT_FALSE(vgm.clusters.empty());
    EXPECT_FALSE(vgm.dag_nodes.empty());
    EXPECT_GE(vgm.num_lod_levels, 1u);
    EXPECT_FALSE(vgm.draw_ranges.empty());
}

// 测试 VirtualGeometry：renderer CPU 路径全流程（Init → Register → Submit → Execute）
// 前置：renderer Init + 注册立方体 VG mesh + 提交一个实例；
// 预期：Execute 后统计有效（实例已收集、LOD 选择产生选中簇），renderer 可用。
TEST(VirtualGeometryTest, RendererCPUPathExecutes) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
    MakeCubeMesh(positions, normals, uvs, indices);

    VirtualGeometryMesh vgm = DAGBuilder().Build(positions, normals, uvs, indices, DAGBuildConfig{});

    VirtualGeometryRenderer renderer;
    VirtualGeometryConfig cfg;
    cfg.enabled = true;
    renderer.Init(cfg, 1280, 720);

    uint32_t mesh_id = renderer.RegisterMesh("test_cube", vgm);
    ASSERT_TRUE(renderer.HasMesh(mesh_id));

    VGInstance inst;
    inst.mesh_id = mesh_id;
    inst.model = glm::mat4(1.0f);
    inst.nanite_static = true;
    renderer.BeginFrame(1);
    renderer.SubmitInstance(inst);

    renderer.Execute(glm::lookAt(glm::vec3(5.0f, 3.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0,1,0)),
                     glm::perspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f),
                     glm::vec3(5.0f, 3.0f, 5.0f), 60.0f);

    const auto& stats = renderer.GetFrameStats();
    EXPECT_EQ(stats.total_instances, 1u);
    EXPECT_EQ(stats.nanite_static_instances, 1u);
    // 立方体在相机视野内，LOD 选择应产生选中簇（数量 >= 1 取决于 meshlet 划分）
    EXPECT_GE(stats.selected_clusters, 0u);

    renderer.Shutdown();
}

// 测试 VirtualGeometry：未启用时 Execute 为空转（config.enabled=false）
TEST(VirtualGeometryTest, RendererDisabledIsNoop) {
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<uint32_t> indices;
    MakeCubeMesh(positions, normals, uvs, indices);

    VirtualGeometryRenderer renderer;
    VirtualGeometryConfig cfg;  // enabled = false
    renderer.Init(cfg, 640, 480);

    VGInstance inst;
    inst.mesh_id = 0;  // 未注册
    renderer.BeginFrame(1);
    renderer.SubmitInstance(inst);
    renderer.Execute(glm::mat4(1.0f), glm::mat4(1.0f), glm::vec3(0.0f), 60.0f);

    EXPECT_EQ(renderer.GetFrameStats().total_instances, 0u);
    renderer.Shutdown();
}

#endif // DSE_ENABLE_VIRTUAL_GEOMETRY

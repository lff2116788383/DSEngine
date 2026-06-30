/**
 * @file meshlet_cull_test.cpp
 * @brief Meshlet Cull Pass 单元测试
 */

#include <gtest/gtest.h>
#include "engine/render/meshlet/meshlet_cull_pass.h"
#include "engine/render/meshlet/meshlet_builder.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace dse::render;

class MeshletCullPassTest : public ::testing::Test {
protected:
    MeshletCullPass cull_pass;
    MeshletMesh test_mesh;
    uint32_t mesh_id = 0;

    void SetUp() override {
        // Create a simple mesh with known bounds
        std::vector<glm::vec3> positions;
        std::vector<uint32_t> indices;

        // Create a 10x10 grid (200 triangles) at z=0
        const int size = 10;
        for (int y = 0; y <= size; ++y) {
            for (int x = 0; x <= size; ++x) {
                positions.push_back(glm::vec3(
                    static_cast<float>(x),
                    static_cast<float>(y),
                    0.0f));
            }
        }
        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                uint32_t tl = y * (size + 1) + x;
                uint32_t tr = tl + 1;
                uint32_t bl = (y + 1) * (size + 1) + x;
                uint32_t br = bl + 1;
                indices.push_back(tl);
                indices.push_back(tr);
                indices.push_back(bl);
                indices.push_back(tr);
                indices.push_back(br);
                indices.push_back(bl);
            }
        }

        MeshletBuilder builder;
        MeshletBuildConfig config;
        config.max_triangles = 32;
        test_mesh = builder.Build(positions, indices, config);

        mesh_id = cull_pass.RegisterMesh(test_mesh);
    }
};

TEST_F(MeshletCullPassTest, RegisterAndUnregister) {
    EXPECT_EQ(cull_pass.GetRegisteredMeshCount(), 1u);

    MeshletMesh empty_mesh;
    uint32_t id2 = cull_pass.RegisterMesh(empty_mesh);
    EXPECT_EQ(cull_pass.GetRegisteredMeshCount(), 2u);

    cull_pass.UnregisterMesh(id2);
    EXPECT_EQ(cull_pass.GetRegisteredMeshCount(), 1u);
}

TEST_F(MeshletCullPassTest, BeginFrame_ClearsInstances) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));
    EXPECT_EQ(cull_pass.GetInstanceCount(), 1u);
    EXPECT_GT(cull_pass.GetTotalMeshletCount(), 0u);

    cull_pass.BeginFrame();
    EXPECT_EQ(cull_pass.GetInstanceCount(), 0u);
    EXPECT_EQ(cull_pass.GetTotalMeshletCount(), 0u);
}

TEST_F(MeshletCullPassTest, AddInstance_IncreasesMeshletCount) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));

    uint32_t expected_meshlets = static_cast<uint32_t>(test_mesh.meshlets.size());
    EXPECT_EQ(cull_pass.GetTotalMeshletCount(), expected_meshlets);

    // Second instance doubles the count
    cull_pass.AddInstance(mesh_id, glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, 0.0f)));
    EXPECT_EQ(cull_pass.GetTotalMeshletCount(), expected_meshlets * 2);
}

TEST_F(MeshletCullPassTest, PrepareGPUData_GeneratesCommands) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));

    glm::mat4 view = glm::lookAt(glm::vec3(5.0f, 5.0f, 10.0f),
                                   glm::vec3(5.0f, 5.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    uint32_t count = cull_pass.PrepareGPUData(view, proj, glm::vec3(5.0f, 5.0f, 10.0f));
    EXPECT_EQ(count, static_cast<uint32_t>(test_mesh.meshlets.size()));

    const auto& cmds = cull_pass.GetDrawCommands();
    EXPECT_EQ(cmds.size(), test_mesh.meshlets.size());

    // All should be visible initially (before culling)
    for (const auto& cmd : cmds) {
        EXPECT_EQ(cmd.instance_count, 1u);
        EXPECT_GT(cmd.count, 0u);
    }
}

TEST_F(MeshletCullPassTest, CullCPU_AllVisible) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));

    // Camera looking directly at the mesh
    glm::mat4 view = glm::lookAt(glm::vec3(5.0f, 5.0f, 10.0f),
                                   glm::vec3(5.0f, 5.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 vp = proj * view;

    cull_pass.PrepareGPUData(view, proj, glm::vec3(5.0f, 5.0f, 10.0f));
    cull_pass.CullCPU(vp, glm::vec3(5.0f, 5.0f, 10.0f));

    uint32_t visible = cull_pass.GetVisibleMeshletCount();
    uint32_t total = cull_pass.GetTotalMeshletCount();
    EXPECT_GT(visible, 0u);
    EXPECT_EQ(visible, total); // All should be visible when camera looks at mesh
}

TEST_F(MeshletCullPassTest, CullCPU_FrustumCulling) {
    cull_pass.BeginFrame();
    // Place instance far away
    glm::mat4 far_model = glm::translate(glm::mat4(1.0f), glm::vec3(1000.0f, 0.0f, 0.0f));
    cull_pass.AddInstance(mesh_id, far_model);

    // Camera looking in -Z, mesh is far in +X
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f),
                                   glm::vec3(0.0f, 0.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 50.0f);
    glm::mat4 vp = proj * view;

    cull_pass.PrepareGPUData(view, proj, glm::vec3(0.0f, 0.0f, 10.0f));

    MeshletCullConfig config;
    config.enable_frustum_cull = true;
    config.enable_occlusion_cull = false;
    config.enable_cone_cull = false;

    cull_pass.CullCPU(vp, glm::vec3(0.0f, 0.0f, 10.0f), config);

    uint32_t visible = cull_pass.GetVisibleMeshletCount();
    // Mesh at x=1000 should be outside 45° FOV frustum looking at -Z
    EXPECT_EQ(visible, 0u);
}

TEST_F(MeshletCullPassTest, CullCPU_ConeCulling) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));

    // Camera very far behind the mesh (normals face +Z, camera far on -Z axis)
    // Distance must be large enough that all meshlet to_camera vectors converge to (0,0,-1)
    glm::vec3 cam_pos(5.0f, 5.0f, -10000.0f);
    glm::mat4 view = glm::lookAt(cam_pos,
                                   glm::vec3(5.0f, 5.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 20000.0f);
    glm::mat4 vp = proj * view;

    cull_pass.PrepareGPUData(view, proj, cam_pos);

    MeshletCullConfig config;
    config.enable_frustum_cull = false;  // Disable frustum to isolate cone test
    config.enable_occlusion_cull = false;
    config.enable_cone_cull = true;
    config.cone_cull_threshold = 0.0f;

    cull_pass.CullCPU(vp, cam_pos, config);

    // With cone culling from far behind, all flat meshlets facing +Z should be culled
    uint32_t visible = cull_pass.GetVisibleMeshletCount();
    uint32_t total = cull_pass.GetTotalMeshletCount();
    EXPECT_LT(visible, total);
}

TEST_F(MeshletCullPassTest, CullCPU_DisabledCulling) {
    cull_pass.BeginFrame();
    glm::mat4 far_model = glm::translate(glm::mat4(1.0f), glm::vec3(1000.0f, 0.0f, 0.0f));
    cull_pass.AddInstance(mesh_id, far_model);

    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f),
                                   glm::vec3(0.0f, 0.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 50.0f);
    glm::mat4 vp = proj * view;

    cull_pass.PrepareGPUData(view, proj, glm::vec3(0.0f, 0.0f, 10.0f));

    // All culling disabled → everything visible
    MeshletCullConfig config;
    config.enable_frustum_cull = false;
    config.enable_occlusion_cull = false;
    config.enable_cone_cull = false;

    cull_pass.CullCPU(vp, glm::vec3(0.0f, 0.0f, 10.0f), config);

    EXPECT_EQ(cull_pass.GetVisibleMeshletCount(), cull_pass.GetTotalMeshletCount());
}

TEST_F(MeshletCullPassTest, MultipleInstances) {
    cull_pass.BeginFrame();

    // Add 3 instances at different positions
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));
    cull_pass.AddInstance(mesh_id, glm::translate(glm::mat4(1.0f), glm::vec3(20.0f, 0.0f, 0.0f)));
    cull_pass.AddInstance(mesh_id, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 20.0f, 0.0f)));

    EXPECT_EQ(cull_pass.GetInstanceCount(), 3u);
    uint32_t meshlets_per_instance = static_cast<uint32_t>(test_mesh.meshlets.size());
    EXPECT_EQ(cull_pass.GetTotalMeshletCount(), meshlets_per_instance * 3);
}

TEST_F(MeshletCullPassTest, InvalidMeshId) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(9999, glm::mat4(1.0f)); // non-existent mesh
    EXPECT_EQ(cull_pass.GetInstanceCount(), 0u);
    EXPECT_EQ(cull_pass.GetTotalMeshletCount(), 0u);
}

TEST_F(MeshletCullPassTest, GPUData_CorrectSize) {
    cull_pass.BeginFrame();
    cull_pass.AddInstance(mesh_id, glm::mat4(1.0f));

    glm::mat4 view = glm::lookAt(glm::vec3(5.0f, 5.0f, 10.0f),
                                   glm::vec3(5.0f, 5.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    uint32_t count = cull_pass.PrepareGPUData(view, proj, glm::vec3(5.0f, 5.0f, 10.0f));

    EXPECT_EQ(cull_pass.GetMeshletGPUData().size(), count);
    EXPECT_EQ(cull_pass.GetDrawCommands().size(), count);

    // Verify GPU data has valid world-space sphere
    for (const auto& gpu : cull_pass.GetMeshletGPUData()) {
        EXPECT_GT(gpu.sphere.w, 0.0f); // radius > 0
    }
}

TEST_F(MeshletCullPassTest, ScaledInstance) {
    cull_pass.BeginFrame();
    glm::mat4 scaled = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
    cull_pass.AddInstance(mesh_id, scaled);

    glm::mat4 view = glm::lookAt(glm::vec3(10.0f, 10.0f, 20.0f),
                                   glm::vec3(10.0f, 10.0f, 0.0f),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);

    cull_pass.PrepareGPUData(view, proj, glm::vec3(10.0f, 10.0f, 20.0f));

    // Check that world-space radius is scaled
    const auto& gpu_data = cull_pass.GetMeshletGPUData();
    for (size_t i = 0; i < gpu_data.size(); ++i) {
        float world_radius = gpu_data[i].sphere.w;
        float local_radius = test_mesh.meshlets[i].radius;
        EXPECT_NEAR(world_radius, local_radius * 2.0f, 0.01f);
    }
}

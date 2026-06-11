/**
 * @file tree_system_test.cpp
 * @brief TreeSystem + TreeComponent 纯逻辑单元测试（无 GPU/窗口）
 *
 * 测试策略：
 * - TreeComponent 各字段默认值
 * - ChunkKey 等价逻辑的唯一性 & 可逆性
 * - IsAABBInFrustum / ExtractFrustumPlanes 等价逻辑的正确性
 * - TreeSystem Init/Shutdown 生命周期安全性
 * - GenerateChunkInstances 确定性（通过 Update 两次验证缓存不变）
 */

#include "gtest/gtest.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "modules/gameplay_3d/rendering/tree_system.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <set>

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// 等价辅助函数（TreeSystem private static 方法的镜像）
// ============================================================

static uint64_t TestChunkKey(int cx, int cz) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
            static_cast<uint64_t>(static_cast<uint32_t>(cz));
}

static void TestExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out_planes[6]) {
    for (int i = 0; i < 4; ++i) {
        out_planes[0][i] = vp[i][3] + vp[i][0]; // left
        out_planes[1][i] = vp[i][3] - vp[i][0]; // right
        out_planes[2][i] = vp[i][3] + vp[i][1]; // bottom
        out_planes[3][i] = vp[i][3] - vp[i][1]; // top
        out_planes[4][i] = vp[i][3] + vp[i][2]; // near
        out_planes[5][i] = vp[i][3] - vp[i][2]; // far
    }
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(out_planes[i]));
        if (len > 1e-6f) out_planes[i] /= len;
    }
}

static bool TestIsAABBInFrustum(const glm::vec4 planes[6],
                                 const glm::vec3& aabb_min,
                                 const glm::vec3& aabb_max) {
    for (int i = 0; i < 6; ++i) {
        glm::vec3 p(
            planes[i].x > 0.0f ? aabb_max.x : aabb_min.x,
            planes[i].y > 0.0f ? aabb_max.y : aabb_min.y,
            planes[i].z > 0.0f ? aabb_max.z : aabb_min.z);
        if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0.0f)
            return false;
    }
    return true;
}

// ============================================================
// 1.1 TreeComponent 默认值验证
// ============================================================

TEST(TreeComponentTest, DefaultValues_EnabledAnd) {
    TreeComponent tc;
    EXPECT_TRUE(tc.enabled);
    EXPECT_FLOAT_EQ(tc.density, 0.02f);
    EXPECT_FLOAT_EQ(tc.spawn_radius, 120.0f);
    EXPECT_FLOAT_EQ(tc.chunk_size, 32.0f);
}

TEST(TreeComponentTest, DefaultValues_Case) {
    TreeComponent tc;
    EXPECT_FLOAT_EQ(tc.min_scale, 0.8f);
    EXPECT_FLOAT_EQ(tc.max_scale, 1.3f);
}

TEST(TreeComponentTest, DefaultValues_LOD) {
    TreeComponent tc;
    EXPECT_FLOAT_EQ(tc.lod1_distance, 60.0f);
    EXPECT_FLOAT_EQ(tc.billboard_distance, 150.0f);
    EXPECT_FLOAT_EQ(tc.cull_distance, 200.0f);
}

TEST(TreeComponentTest, DefaultValues_And) {
    TreeComponent tc;
    EXPECT_FLOAT_EQ(tc.wind_strength, 0.3f);
    EXPECT_TRUE(tc.cast_shadow);
    EXPECT_FLOAT_EQ(tc.shadow_distance, 80.0f);
}

TEST(TreeComponentTest, DefaultValues_When) {
    TreeComponent tc;
    EXPECT_EQ(tc.cached_instance_count_, 0);
}

TEST(TreeComponentTest, DefaultValues_IsEmpty) {
    TreeComponent tc;
    EXPECT_TRUE(tc.mesh_path.empty());
    EXPECT_TRUE(tc.lod1_mesh_path.empty());
    EXPECT_TRUE(tc.billboard_texture_path.empty());
}

// ============================================================
// 1.2 ChunkKey 唯一性和可逆性
// ============================================================

TEST(TreeChunkKeyTest, DifferentCoordinatesProduceDifferentKey) {
    EXPECT_NE(TestChunkKey(0, 0), TestChunkKey(1, 0));
    EXPECT_NE(TestChunkKey(0, 0), TestChunkKey(0, 1));
    EXPECT_NE(TestChunkKey(1, 2), TestChunkKey(2, 1));
    EXPECT_NE(TestChunkKey(10, 20), TestChunkKey(20, 10));
}

TEST(TreeChunkKeyTest, Key) {
    EXPECT_EQ(TestChunkKey(5, 7), TestChunkKey(5, 7));
    EXPECT_EQ(TestChunkKey(0, 0), TestChunkKey(0, 0));
    EXPECT_EQ(TestChunkKey(-3, -4), TestChunkKey(-3, -4));
}

TEST(TreeChunkKeyTest, NegativeCoordinatesCorrect) {
    uint64_t k_neg = TestChunkKey(-1, -1);
    uint64_t k_pos = TestChunkKey(1, 1);
    EXPECT_NE(k_neg, k_pos);

    EXPECT_NE(TestChunkKey(-1, 0), TestChunkKey(1, 0));
    EXPECT_NE(TestChunkKey(0, -1), TestChunkKey(0, 1));
}

TEST(TreeChunkKeyTest, KeyWithout) {
    std::set<uint64_t> keys;
    for (int cx = -10; cx <= 10; ++cx) {
        for (int cz = -10; cz <= 10; ++cz) {
            uint64_t k = TestChunkKey(cx, cz);
            EXPECT_TRUE(keys.insert(k).second)
                << "collision at cx=" << cx << " cz=" << cz;
        }
    }
    EXPECT_EQ(keys.size(), 21u * 21u);
}

TEST(TreeChunkKeyTest, Can_Case32) {
    int cx = -42, cz = 99;
    uint64_t key = TestChunkKey(cx, cz);
    int recovered_cx = static_cast<int>(static_cast<uint32_t>(key >> 32));
    int recovered_cz = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
    EXPECT_EQ(recovered_cx, cx);
    EXPECT_EQ(recovered_cz, cz);
}

// ============================================================
// 1.3 IsAABBInFrustum 已知平面测试
// ============================================================

TEST(TreeFrustumTest, AABBFullyInsideFrustum) {
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 vp = proj * view;

    glm::vec4 planes[6];
    TestExtractFrustumPlanes(vp, planes);

    EXPECT_TRUE(TestIsAABBInFrustum(planes,
        glm::vec3(-1.0f, -1.0f, -10.0f),
        glm::vec3(1.0f, 1.0f, -5.0f)));
}

TEST(TreeFrustumTest, AABBFullyOutsideFrustum_Back) {
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 vp = proj * view;

    glm::vec4 planes[6];
    TestExtractFrustumPlanes(vp, planes);

    EXPECT_FALSE(TestIsAABBInFrustum(planes,
        glm::vec3(-1.0f, -1.0f, 5.0f),
        glm::vec3(1.0f, 1.0f, 10.0f)));
}

TEST(TreeFrustumTest, AABBFullyOutsideFrustum_FarAway) {
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 50.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 vp = proj * view;

    glm::vec4 planes[6];
    TestExtractFrustumPlanes(vp, planes);

    EXPECT_FALSE(TestIsAABBInFrustum(planes,
        glm::vec3(-1.0f, -1.0f, -200.0f),
        glm::vec3(1.0f, 1.0f, -100.0f)));
}

TEST(TreeFrustumTest, AABBOutsideFrustum_LeftSide) {
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 vp = proj * view;

    glm::vec4 planes[6];
    TestExtractFrustumPlanes(vp, planes);

    EXPECT_FALSE(TestIsAABBInFrustum(planes,
        glm::vec3(-1000.0f, -1.0f, -10.0f),
        glm::vec3(-500.0f, 1.0f, -5.0f)));
}

TEST(TreeFrustumTest, Alignment_Inside) {
    glm::vec4 planes[6] = {
        glm::vec4( 1.0f, 0.0f, 0.0f, 10.0f),  // x >= -10
        glm::vec4(-1.0f, 0.0f, 0.0f, 10.0f),  // x <=  10
        glm::vec4( 0.0f, 1.0f, 0.0f, 10.0f),  // y >= -10
        glm::vec4( 0.0f,-1.0f, 0.0f, 10.0f),  // y <=  10
        glm::vec4( 0.0f, 0.0f, 1.0f, 10.0f),  // z >= -10
        glm::vec4( 0.0f, 0.0f,-1.0f, 10.0f),  // z <=  10
    };
    EXPECT_TRUE(TestIsAABBInFrustum(planes,
        glm::vec3(-5.0f), glm::vec3(5.0f)));
}

TEST(TreeFrustumTest, Alignment_Outside) {
    glm::vec4 planes[6] = {
        glm::vec4( 1.0f, 0.0f, 0.0f, 10.0f),
        glm::vec4(-1.0f, 0.0f, 0.0f, 10.0f),
        glm::vec4( 0.0f, 1.0f, 0.0f, 10.0f),
        glm::vec4( 0.0f,-1.0f, 0.0f, 10.0f),
        glm::vec4( 0.0f, 0.0f, 1.0f, 10.0f),
        glm::vec4( 0.0f, 0.0f,-1.0f, 10.0f),
    };
    EXPECT_FALSE(TestIsAABBInFrustum(planes,
        glm::vec3(20.0f, 20.0f, 20.0f),
        glm::vec3(30.0f, 30.0f, 30.0f)));
}

// ============================================================
// 1.4 TreeSystem 生命周期
// ============================================================

TEST(TreeSystemTest, DefaultDoesNotCrash) {
    TreeSystem sys;
    (void)sys;
    SUCCEED();
}

TEST(TreeSystemTest, Init_NullptrDeviceDoesNotCrash) {
    TreeSystem sys;
    sys.Init(nullptr);
    SUCCEED();
}

TEST(TreeSystemTest, Shutdown_EmptyWorldDoesNotCrash) {
    TreeSystem sys;
    sys.Init(nullptr);
    World world;
    sys.Shutdown(world);
    SUCCEED();
}

TEST(TreeSystemTest, InitLaterShutdownDoesNotCrash) {
    TreeSystem sys;
    sys.Init(nullptr);
    World world;
    sys.Shutdown(world);
    SUCCEED();
}

// ============================================================
// 1.5 GenerateChunkInstances 确定性测试
// ============================================================

TEST(TreeSystemTest, Updatetwice_TheNumberOfCacheInstancesRemainsUnchanged) {
    TreeSystem sys;
    sys.Init(nullptr);

    World world;
    auto& reg = world.registry();

    Entity tree_entity = world.CreateEntity();
    auto& tc = reg.emplace<TreeComponent>(tree_entity);
    tc.enabled = true;
    tc.density = 0.05f;
    tc.spawn_radius = 50.0f;
    tc.chunk_size = 16.0f;
    tc.seed = 99999;

    auto& tf = reg.emplace<TransformComponent>(tree_entity);
    tf.position = glm::vec3(0.0f);

    Entity cam_entity = world.CreateEntity();
    auto& cam = reg.emplace<Camera3DComponent>(cam_entity);
    cam.enabled = true;
    auto& cam_tf = reg.emplace<TransformComponent>(cam_entity);
    cam_tf.position = glm::vec3(0.0f, 10.0f, 0.0f);
    cam_tf.local_to_world = glm::translate(glm::mat4(1.0f), cam_tf.position);

    sys.Update(world, 0.016f);
    int count_first = tc.cached_instance_count_;

    sys.Update(world, 0.016f);
    int count_second = tc.cached_instance_count_;

    EXPECT_EQ(count_first, count_second);

    sys.Shutdown(world);
}

TEST(TreeSystemTest, Update_EmptyWorldDoesNotCrash) {
    TreeSystem sys;
    sys.Init(nullptr);
    World world;
    sys.Update(world, 0.016f);
    sys.Shutdown(world);
    SUCCEED();
}

// ============================================================
// TreeInstanceLayout / TreeChunkData 默认值
// ============================================================

TEST(TreeInstanceLayoutTest, InitializeAllZero) {
    TreeInstanceLayout layout{};
    EXPECT_FLOAT_EQ(layout.position.x, 0.0f);
    EXPECT_FLOAT_EQ(layout.position.y, 0.0f);
    EXPECT_FLOAT_EQ(layout.position.z, 0.0f);
    EXPECT_FLOAT_EQ(layout.yaw, 0.0f);
    EXPECT_FLOAT_EQ(layout.scale, 0.0f);
}

TEST(TreeChunkDataTest, DefaultValues) {
    TreeChunkData chunk;
    EXPECT_TRUE(chunk.layouts.empty());
    EXPECT_FALSE(chunk.valid);
    EXPECT_FLOAT_EQ(chunk.aabb_min.x, 0.0f);
    EXPECT_FLOAT_EQ(chunk.aabb_max.x, 0.0f);
}

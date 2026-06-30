/**
 * @file lightmap_baker_test.cpp
 * @brief GI Lightmap Baker 单元测试
 *
 * 测试烘焙配置、场景构建、烘焙结果和文件 I/O。
 */

#include <gtest/gtest.h>
#include "engine/render/gi/lightmap_baker.h"
#include <filesystem>

using namespace dse::render;

// ─── BakeConfig 测试 ────────────────────────────────────────────────────────

TEST(LightmapBakeConfigTest, DefaultValues) {
    LightmapBakeConfig config;

    EXPECT_EQ(config.resolution, 512u);
    EXPECT_EQ(config.samples_per_texel, 64u);
    EXPECT_EQ(config.bounces, 2u);
    EXPECT_GT(config.bias, 0.0f);
    EXPECT_TRUE(config.bake_ao);
    EXPECT_GT(config.ao_radius, 0.0f);
    EXPECT_TRUE(config.denoise);
}

// ─── BakeScene 测试 ─────────────────────────────────────────────────────────

TEST(BakeSceneTest, EmptyScene) {
    BakeScene scene;
    EXPECT_TRUE(scene.triangles.empty());
    EXPECT_TRUE(scene.lights.empty());
}

TEST(BakeSceneTest, AddTriangle) {
    BakeScene scene;
    BakeTriangle tri;
    tri.v0 = glm::vec3(0, 0, 0);
    tri.v1 = glm::vec3(1, 0, 0);
    tri.v2 = glm::vec3(0, 0, 1);
    tri.n0 = tri.n1 = tri.n2 = glm::vec3(0, 1, 0);
    tri.uv0 = glm::vec2(0, 0);
    tri.uv1 = glm::vec2(1, 0);
    tri.uv2 = glm::vec2(0, 1);
    tri.albedo = glm::vec3(0.8f);
    tri.mesh_id = 1;

    scene.triangles.push_back(tri);
    EXPECT_EQ(scene.triangles.size(), 1u);
    EXPECT_EQ(scene.triangles[0].mesh_id, 1u);
}

TEST(BakeSceneTest, AddLight) {
    BakeScene scene;
    BakeLight light;
    light.type = BakeLight::Type::Directional;
    light.direction = glm::vec3(0, -1, 0);
    light.color = glm::vec3(1.0f);
    light.intensity = 2.0f;

    scene.lights.push_back(light);
    EXPECT_EQ(scene.lights.size(), 1u);
    EXPECT_FLOAT_EQ(scene.lights[0].intensity, 2.0f);
}

TEST(BakeSceneTest, LightTypes) {
    EXPECT_NE(static_cast<int>(BakeLight::Type::Directional),
              static_cast<int>(BakeLight::Type::Point));
    EXPECT_NE(static_cast<int>(BakeLight::Type::Point),
              static_cast<int>(BakeLight::Type::Spot));
}

// ─── LightmapBaker 烘焙测试 ────────────────────────────────────────────────

TEST(LightmapBakerTest, BakeEmptyScene_Succeeds) {
    LightmapBaker baker;
    BakeScene scene;
    LightmapBakeConfig config;
    config.resolution = 4; // minimal for speed
    config.samples_per_texel = 1;
    config.bounces = 1;

    // Empty scene should still produce a valid result (all black)
    LightmapResult result = baker.Bake(scene, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.width, 4u);
    EXPECT_EQ(result.height, 4u);
    EXPECT_EQ(result.irradiance.size(), 16u); // 4x4
}

TEST(LightmapBakerTest, BakeSimpleScene_ProducesNonZeroIrradiance) {
    LightmapBaker baker;
    BakeScene scene;

    // Floor triangle with lightmap UVs covering [0,1]
    BakeTriangle tri;
    tri.v0 = glm::vec3(-5, 0, -5);
    tri.v1 = glm::vec3(5, 0, -5);
    tri.v2 = glm::vec3(0, 0, 5);
    tri.n0 = tri.n1 = tri.n2 = glm::vec3(0, 1, 0);
    tri.uv0 = glm::vec2(0, 0);
    tri.uv1 = glm::vec2(1, 0);
    tri.uv2 = glm::vec2(0.5f, 1.0f);
    tri.albedo = glm::vec3(0.8f);
    scene.triangles.push_back(tri);

    // Directional light from above
    BakeLight light;
    light.type = BakeLight::Type::Directional;
    light.direction = glm::vec3(0, -1, 0);
    light.color = glm::vec3(1.0f);
    light.intensity = 3.0f;
    scene.lights.push_back(light);

    LightmapBakeConfig config;
    config.resolution = 8;
    config.samples_per_texel = 4;
    config.bounces = 1;
    config.denoise = false;

    LightmapResult result = baker.Bake(scene, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.width, 8u);
    EXPECT_EQ(result.height, 8u);

    // At least some texels should have non-zero irradiance
    bool has_light = false;
    for (const auto& c : result.irradiance) {
        if (c.r > 0.0f || c.g > 0.0f || c.b > 0.0f) {
            has_light = true;
            break;
        }
    }
    EXPECT_TRUE(has_light);
}

TEST(LightmapBakerTest, BakeWithAO_ProducesAOChannel) {
    LightmapBaker baker;
    BakeScene scene;

    BakeTriangle tri;
    tri.v0 = glm::vec3(-1, 0, -1);
    tri.v1 = glm::vec3(1, 0, -1);
    tri.v2 = glm::vec3(0, 0, 1);
    tri.n0 = tri.n1 = tri.n2 = glm::vec3(0, 1, 0);
    tri.uv0 = glm::vec2(0, 0);
    tri.uv1 = glm::vec2(1, 0);
    tri.uv2 = glm::vec2(0.5f, 1);
    scene.triangles.push_back(tri);

    LightmapBakeConfig config;
    config.resolution = 4;
    config.samples_per_texel = 2;
    config.bounces = 1;
    config.bake_ao = true;
    config.denoise = false;

    LightmapResult result = baker.Bake(scene, config);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.ao.size(), 16u); // 4x4
}

TEST(LightmapBakerTest, ProgressCallback_IsCalled) {
    LightmapBaker baker;
    BakeScene scene;

    BakeTriangle tri;
    tri.v0 = glm::vec3(0, 0, 0);
    tri.v1 = glm::vec3(1, 0, 0);
    tri.v2 = glm::vec3(0, 0, 1);
    tri.n0 = tri.n1 = tri.n2 = glm::vec3(0, 1, 0);
    tri.uv0 = glm::vec2(0, 0);
    tri.uv1 = glm::vec2(1, 0);
    tri.uv2 = glm::vec2(0, 1);
    scene.triangles.push_back(tri);

    LightmapBakeConfig config;
    config.resolution = 4;
    config.samples_per_texel = 1;
    config.bounces = 1;
    config.denoise = false;

    int callback_count = 0;
    float last_progress = -1.0f;
    auto cb = [&](float p) {
        EXPECT_GE(p, last_progress);
        last_progress = p;
        ++callback_count;
    };

    baker.Bake(scene, config, cb);
    EXPECT_GT(callback_count, 0);
}

// ─── 文件 I/O 测试 ──────────────────────────────────────────────────────────

TEST(LightmapBakerTest, SaveAndLoad_RoundTrip) {
    LightmapBaker baker;
    BakeScene scene;

    BakeTriangle tri;
    tri.v0 = glm::vec3(-2, 0, -2);
    tri.v1 = glm::vec3(2, 0, -2);
    tri.v2 = glm::vec3(0, 0, 2);
    tri.n0 = tri.n1 = tri.n2 = glm::vec3(0, 1, 0);
    tri.uv0 = glm::vec2(0, 0);
    tri.uv1 = glm::vec2(1, 0);
    tri.uv2 = glm::vec2(0.5f, 1);
    scene.triangles.push_back(tri);

    BakeLight light;
    light.type = BakeLight::Type::Directional;
    light.direction = glm::vec3(0, -1, 0);
    light.color = glm::vec3(1.0f);
    light.intensity = 1.0f;
    scene.lights.push_back(light);

    LightmapBakeConfig config;
    config.resolution = 4;
    config.samples_per_texel = 2;
    config.bounces = 1;
    config.bake_ao = true;
    config.denoise = false;

    LightmapResult result = baker.Bake(scene, config);
    ASSERT_TRUE(result.success);

    // Save
    std::string path = "test_lightmap_roundtrip.dlightmap";
    ASSERT_TRUE(LightmapBaker::SaveToFile(result, path));

    // Load
    LightmapResult loaded;
    ASSERT_TRUE(LightmapBaker::LoadFromFile(path, loaded));

    EXPECT_EQ(loaded.width, result.width);
    EXPECT_EQ(loaded.height, result.height);
    EXPECT_EQ(loaded.irradiance.size(), result.irradiance.size());
    EXPECT_EQ(loaded.ao.size(), result.ao.size());

    // Verify data matches (within float precision for HDR encoding)
    for (size_t i = 0; i < result.irradiance.size(); ++i) {
        EXPECT_NEAR(loaded.irradiance[i].r, result.irradiance[i].r, 0.01f);
        EXPECT_NEAR(loaded.irradiance[i].g, result.irradiance[i].g, 0.01f);
        EXPECT_NEAR(loaded.irradiance[i].b, result.irradiance[i].b, 0.01f);
    }

    // Cleanup
    std::filesystem::remove(path);
}

// ─── LightmapComponent 测试 ─────────────────────────────────────────────────

TEST(LightmapComponentTest, DefaultValues) {
    LightmapComponent comp;

    EXPECT_TRUE(comp.lightmap_path.empty());
    EXPECT_EQ(comp.lightmap_handle, 0u);
    EXPECT_FLOAT_EQ(comp.intensity, 1.0f);
    EXPECT_TRUE(comp.use_ao);

    // ST offset: scale=1, offset=0
    EXPECT_FLOAT_EQ(comp.st_offset.x, 1.0f);
    EXPECT_FLOAT_EQ(comp.st_offset.y, 1.0f);
    EXPECT_FLOAT_EQ(comp.st_offset.z, 0.0f);
    EXPECT_FLOAT_EQ(comp.st_offset.w, 0.0f);
}

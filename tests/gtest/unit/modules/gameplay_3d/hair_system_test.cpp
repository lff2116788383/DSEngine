/**
 * @file hair_system_test.cpp
 * @brief HairSystem 单元测试（无 GPU）
 *
 * 测试策略：
 * - HairComponent ECS 默认值
 * - HairSystem 默认状态
 * - instances() / GetCachedAsset() 空状态
 */

#include <gtest/gtest.h>
#include "modules/gameplay_3d/rendering/hair_system.h"
#include "engine/ecs/components_3d.h"
#include <glm/glm.hpp>

using namespace dse;
using namespace dse::gameplay3d;

// ============================================================
// HairComponent 默认值
// ============================================================

TEST(HairComponentTest, 默认值) {
    HairComponent hc;
    EXPECT_TRUE(hc.enabled);
    EXPECT_TRUE(hc.hair_asset_path.empty());

    EXPECT_FLOAT_EQ(hc.damping, 0.04f);
    EXPECT_FLOAT_EQ(hc.stiffness_local, 0.8f);
    EXPECT_FLOAT_EQ(hc.stiffness_global, 0.4f);
    EXPECT_FLOAT_EQ(hc.gravity, 9.81f);
    EXPECT_FLOAT_EQ(hc.wind.x, 0.0f);
    EXPECT_FLOAT_EQ(hc.wind_turbulence, 0.2f);
}

TEST(HairComponentTest, 渲染参数默认值) {
    HairComponent hc;
    EXPECT_FLOAT_EQ(hc.root_color.r, 0.1f);
    EXPECT_FLOAT_EQ(hc.tip_color.a, 1.0f);
    EXPECT_FLOAT_EQ(hc.fiber_radius, 0.04f);
    EXPECT_FLOAT_EQ(hc.opacity, 0.9f);
    EXPECT_FLOAT_EQ(hc.specular_power_primary, 80.0f);
    EXPECT_FLOAT_EQ(hc.specular_power_secondary, 20.0f);
}

TEST(HairComponentTest, LOD参数默认值) {
    HairComponent hc;
    EXPECT_FLOAT_EQ(hc.lod0_distance, 20.0f);
    EXPECT_FLOAT_EQ(hc.lod1_distance, 40.0f);
    EXPECT_FLOAT_EQ(hc.lod2_distance, 80.0f);
    EXPECT_FLOAT_EQ(hc.cull_distance, 120.0f);
}

TEST(HairComponentTest, Follower参数默认值) {
    HairComponent hc;
    EXPECT_EQ(hc.num_follow_per_guide, 4);
    EXPECT_FLOAT_EQ(hc.follow_root_offset, 1.5f);
    EXPECT_TRUE(hc.cast_shadow);
    EXPECT_TRUE(hc.receive_shadow);
}

TEST(HairComponentTest, 运行时索引默认值) {
    HairComponent hc;
    EXPECT_EQ(hc.hair_instance_index_, -1);
}

// ============================================================
// HairSystem 默认状态
// ============================================================

TEST(HairSystemTest, 默认构造) {
    HairSystem sys;
    EXPECT_TRUE(sys.instances().empty());
}

TEST(HairSystemTest, GetCachedAsset_未缓存返回nullptr) {
    HairSystem sys;
    EXPECT_EQ(sys.GetCachedAsset("nonexistent.dhair"), nullptr);
    EXPECT_EQ(sys.GetCachedAsset(""), nullptr);
}

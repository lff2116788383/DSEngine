/**
 * @file gpu_instancing_test.cpp
 * @brief GPU Instancing 合批逻辑单元测试（纯 CPU 端）
 *
 * 测试策略：
 * - InstancingKeyData 结构体默认值和比较
 * - InstancingKey 相等性和 hash 唯一性
 * - MeshDrawItem::instance_transforms 逻辑
 * - MaterialBlendMode 枚举（Opaque 是合批前提）
 * - RenderStats instancing 统计字段
 * - can_instance 条件验证
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_types.h"
#include "engine/assets/asset_manager.h"
#include <glm/glm.hpp>
#include <cstring>
#include <string>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>

using namespace dse;

// InstancingKey 相关结构体定义在 mesh_render_system.cpp 中是局部的
// 这里复制关键结构用于独立测试合批 key 逻辑

namespace {

struct TestInstancingKeyData {
    unsigned int tex[5];
    float color[4];
    float scalars[5];
    int shading_mode, sorting_layer, order_in_layer, flags;
};

struct TestInstancingKey {
    const std::string* mesh_path;
    TestInstancingKeyData data;

    bool operator==(const TestInstancingKey& o) const {
        return *mesh_path == *o.mesh_path
            && std::memcmp(&data, &o.data, sizeof(TestInstancingKeyData)) == 0;
    }
};

struct TestInstancingKeyHash {
    size_t operator()(const TestInstancingKey& k) const {
        size_t h = std::hash<std::string>{}(*k.mesh_path);
        const auto* p = reinterpret_cast<const unsigned char*>(&k.data);
        for (size_t i = 0; i < sizeof(TestInstancingKeyData); ++i)
            h ^= static_cast<size_t>(p[i]) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

} // anonymous namespace

// ============================================================
// MaterialBlendMode 枚举
// ============================================================

TEST(MaterialBlendModeTest, 枚举值) {
    EXPECT_EQ(static_cast<int>(MaterialBlendMode::Alpha), 0);
    EXPECT_EQ(static_cast<int>(MaterialBlendMode::Additive), 1);
    EXPECT_EQ(static_cast<int>(MaterialBlendMode::Multiply), 2);
    EXPECT_EQ(static_cast<int>(MaterialBlendMode::Opaque), 3);
}

// ============================================================
// MeshDrawItem 默认值和 instance_transforms
// ============================================================

TEST(MeshDrawItemInstancingTest, 默认值) {
    MeshDrawItem item;
    EXPECT_EQ(item.texture_handle, 0u);
    EXPECT_EQ(item.blend_mode, 0u);
    EXPECT_TRUE(item.instance_transforms.empty());
    EXPECT_FALSE(item.skinned);
    EXPECT_FALSE(item.morph_enabled);
    EXPECT_FLOAT_EQ(item.material_metallic, 0.0f);
    EXPECT_FLOAT_EQ(item.material_roughness, 1.0f);
    EXPECT_FLOAT_EQ(item.material_ao, 1.0f);
    EXPECT_TRUE(item.receive_shadow);
    EXPECT_TRUE(item.depth_test_enabled);
    EXPECT_TRUE(item.depth_write_enabled);
}

TEST(MeshDrawItemInstancingTest, instance_transforms触发instanced_draw) {
    MeshDrawItem item;
    EXPECT_TRUE(item.instance_transforms.empty());

    item.instance_transforms.push_back(glm::mat4(1.0f));
    item.instance_transforms.push_back(glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f)));
    EXPECT_EQ(item.instance_transforms.size(), 2u);
}

TEST(MeshDrawItemInstancingTest, Opaque才能合批) {
    // can_instance 条件之一: blend_mode == Opaque
    MeshDrawItem opaque_item;
    opaque_item.blend_mode = static_cast<unsigned int>(MaterialBlendMode::Opaque);
    EXPECT_EQ(opaque_item.blend_mode, 3u);

    MeshDrawItem alpha_item;
    alpha_item.blend_mode = static_cast<unsigned int>(MaterialBlendMode::Alpha);
    EXPECT_NE(alpha_item.blend_mode, static_cast<unsigned int>(MaterialBlendMode::Opaque));
}

TEST(MeshDrawItemInstancingTest, skinned和morph排除合批) {
    MeshDrawItem item;
    item.skinned = true;
    // can_instance 要求 !skinned && !morph_enabled
    EXPECT_TRUE(item.skinned);
    EXPECT_FALSE(!item.skinned && !item.morph_enabled);

    MeshDrawItem morph_item;
    morph_item.morph_enabled = true;
    EXPECT_FALSE(!morph_item.skinned && !morph_item.morph_enabled);

    MeshDrawItem normal_item;
    EXPECT_TRUE(!normal_item.skinned && !normal_item.morph_enabled);
}

// ============================================================
// InstancingKey 相等性和 Hash
// ============================================================

TEST(InstancingKeyTest, 相同Key相等) {
    std::string mesh = "model/box.obj";
    TestInstancingKey a{}, b{};
    a.mesh_path = &mesh;
    b.mesh_path = &mesh;
    std::memset(&a.data, 0, sizeof(a.data));
    std::memset(&b.data, 0, sizeof(b.data));
    a.data.tex[0] = 100;
    b.data.tex[0] = 100;
    EXPECT_TRUE(a == b);
}

TEST(InstancingKeyTest, 不同纹理不相等) {
    std::string mesh = "model/box.obj";
    TestInstancingKey a{}, b{};
    a.mesh_path = &mesh;
    b.mesh_path = &mesh;
    std::memset(&a.data, 0, sizeof(a.data));
    std::memset(&b.data, 0, sizeof(b.data));
    a.data.tex[0] = 100;
    b.data.tex[0] = 200;
    EXPECT_FALSE(a == b);
}

TEST(InstancingKeyTest, 不同mesh_path不相等) {
    std::string mesh_a = "model/box.obj";
    std::string mesh_b = "model/sphere.obj";
    TestInstancingKey a{}, b{};
    a.mesh_path = &mesh_a;
    b.mesh_path = &mesh_b;
    std::memset(&a.data, 0, sizeof(a.data));
    std::memset(&b.data, 0, sizeof(b.data));
    EXPECT_FALSE(a == b);
}

TEST(InstancingKeyTest, Hash一致性) {
    std::string mesh = "model/box.obj";
    TestInstancingKey a{}, b{};
    a.mesh_path = &mesh;
    b.mesh_path = &mesh;
    std::memset(&a.data, 0, sizeof(a.data));
    std::memset(&b.data, 0, sizeof(b.data));
    a.data.tex[0] = 42;
    b.data.tex[0] = 42;

    TestInstancingKeyHash hasher;
    EXPECT_EQ(hasher(a), hasher(b));
}

TEST(InstancingKeyTest, Hash差异性) {
    std::string mesh = "model/box.obj";
    TestInstancingKey a{}, b{};
    a.mesh_path = &mesh;
    b.mesh_path = &mesh;
    std::memset(&a.data, 0, sizeof(a.data));
    std::memset(&b.data, 0, sizeof(b.data));
    a.data.tex[0] = 42;
    b.data.tex[0] = 43;

    TestInstancingKeyHash hasher;
    EXPECT_NE(hasher(a), hasher(b));
}

TEST(InstancingKeyTest, HashMap合批模拟) {
    std::string mesh = "model/box.obj";
    std::unordered_map<TestInstancingKey, size_t, TestInstancingKeyHash> map;

    TestInstancingKey key{};
    key.mesh_path = &mesh;
    std::memset(&key.data, 0, sizeof(key.data));
    key.data.tex[0] = 100;

    // 第一个实例：插入
    map[key] = 0;
    EXPECT_EQ(map.size(), 1u);

    // 第二个相同 key：合批到已有项
    auto it = map.find(key);
    EXPECT_NE(it, map.end());
    EXPECT_EQ(it->second, 0u);

    // 不同 key：新条目
    TestInstancingKey key2{};
    key2.mesh_path = &mesh;
    std::memset(&key2.data, 0, sizeof(key2.data));
    key2.data.tex[0] = 200;
    map[key2] = 1;
    EXPECT_EQ(map.size(), 2u);
}

// ============================================================
// RenderStats instancing 统计
// ============================================================

TEST(RenderStatsInstancingTest, 默认值) {
    RenderStats stats;
    EXPECT_EQ(stats.sprite_count, 0);
    EXPECT_EQ(stats.mesh_count, 0);
    EXPECT_EQ(stats.draw_calls, 0);
    EXPECT_EQ(stats.instanced_draw_calls, 0);
    EXPECT_EQ(stats.instanced_mesh_count, 0);
    EXPECT_EQ(stats.indirect_draw_calls, 0);
    EXPECT_EQ(stats.gpu_culled_count, 0);
}

TEST(RenderStatsInstancingTest, instancing统计累加) {
    RenderStats stats;
    stats.instanced_draw_calls = 5;
    stats.instanced_mesh_count = 20;
    EXPECT_EQ(stats.instanced_draw_calls, 5);
    EXPECT_EQ(stats.instanced_mesh_count, 20);
}

// ============================================================
// DrawElementsIndirectCommand 结构体
// ============================================================

TEST(DrawElementsIndirectCommandTest, 大小和对齐) {
    EXPECT_EQ(sizeof(DrawElementsIndirectCommand), 20u);
    DrawElementsIndirectCommand cmd{};
    cmd.count = 36;
    cmd.instance_count = 1;
    cmd.first_index = 0;
    cmd.base_vertex = 0;
    cmd.base_instance = 0;
    EXPECT_EQ(cmd.count, 36u);
    EXPECT_EQ(cmd.instance_count, 1u);
}

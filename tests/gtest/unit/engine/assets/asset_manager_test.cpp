/**
 * @file asset_manager_test.cpp
 * @brief AssetManager 缓存机制单元测试
 *
 * 覆盖场景：
 * - MaterialInstance 创建/获取/列表
 * - 弱引用缓存：外部释放后 UnloadUnused 清理
 * - ConfigureDataRoot / GetDataRoot 配置
 * - MaterialAsset 属性读写
 * - MaterialBlendMode 枚举
 * - DmeshAsset/DanimAsset/DskelAsset 数据资产封装
 */

#include <gtest/gtest.h>
#include "engine/assets/asset_manager.h"
#include <memory>
#include <vector>

// ============================================================
// MaterialInstance 生命周期测试
// ============================================================

TEST(AssetManagerTest, CreateMaterialInstance返回有效实例) {
    AssetManager mgr;
    auto mat = mgr.CreateMaterialInstance("test_material");
    ASSERT_NE(mat, nullptr);
    EXPECT_EQ(mat->GetName(), "test_material");
    EXPECT_GT(mat->GetId(), 0u);
}

TEST(AssetManagerTest, CreateMaterialInstance自增ID) {
    AssetManager mgr;
    auto mat1 = mgr.CreateMaterialInstance("mat1");
    auto mat2 = mgr.CreateMaterialInstance("mat2");
    EXPECT_LT(mat1->GetId(), mat2->GetId());
}

TEST(AssetManagerTest, GetMaterialInstance通过ID获取) {
    AssetManager mgr;
    auto mat = mgr.CreateMaterialInstance("test");
    unsigned int id = mat->GetId();

    auto retrieved = mgr.GetMaterialInstance(id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->GetId(), id);
    EXPECT_EQ(retrieved->GetName(), "test");
}

TEST(AssetManagerTest, GetMaterialInstance不存在返回nullptr) {
    AssetManager mgr;
    auto result = mgr.GetMaterialInstance(999999u);
    EXPECT_EQ(result, nullptr);
}

TEST(AssetManagerTest, ListMaterialInstanceIds返回存活实例) {
    AssetManager mgr;
    auto mat1 = mgr.CreateMaterialInstance("mat1");
    auto mat2 = mgr.CreateMaterialInstance("mat2");

    auto ids = mgr.ListMaterialInstanceIds();
    EXPECT_EQ(ids.size(), 2u);

    // 验证 ID 存在
    EXPECT_NE(std::find(ids.begin(), ids.end(), mat1->GetId()), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), mat2->GetId()), ids.end());
}

TEST(AssetManagerTest, 弱引用释放后GetMaterialInstance返回nullptr) {
    AssetManager mgr;
    unsigned int id;
    {
        auto mat = mgr.CreateMaterialInstance("temp");
        id = mat->GetId();
        // mat 离开作用域后 weak_ptr 失效
    }
    auto result = mgr.GetMaterialInstance(id);
    EXPECT_EQ(result, nullptr);
}

TEST(AssetManagerTest, 弱引用释放后ListMaterialInstanceIds不包含已过期) {
    AssetManager mgr;
    auto mat1 = mgr.CreateMaterialInstance("alive");
    {
        auto mat2 = mgr.CreateMaterialInstance("dead");
        (void)mat2;
        // mat2 离开作用域
    }

    auto ids = mgr.ListMaterialInstanceIds();
    EXPECT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], mat1->GetId());
}

TEST(AssetManagerTest, UnloadUnused清理已过期的弱引用条目) {
    AssetManager mgr;
    {
        auto mat = mgr.CreateMaterialInstance("will_expire");
        (void)mat;
    }
    // 调用 UnloadUnused 应清除已过期的 materials_ 条目
    mgr.UnloadUnused();

    // 再次 ListMaterialInstanceIds 应返回空（或只有仍存活的）
    auto ids = mgr.ListMaterialInstanceIds();
    EXPECT_TRUE(ids.empty());
}

TEST(AssetManagerTest, UnloadUnused保留仍在使用的实例) {
    AssetManager mgr;
    auto mat_alive = mgr.CreateMaterialInstance("alive");
    {
        auto mat_dead = mgr.CreateMaterialInstance("dead");
        (void)mat_dead;
    }
    mgr.UnloadUnused();

    auto ids = mgr.ListMaterialInstanceIds();
    EXPECT_EQ(ids.size(), 1u);
    EXPECT_EQ(ids[0], mat_alive->GetId());
}

// ============================================================
// MaterialAsset 属性测试
// ============================================================

TEST(MaterialAssetTest, 默认属性值) {
    MaterialAsset mat(1, "default_test");
    EXPECT_EQ(mat.GetId(), 1u);
    EXPECT_EQ(mat.GetName(), "default_test");
    EXPECT_EQ(mat.GetShaderVariant(), "SPRITE_UNLIT");
    EXPECT_EQ(mat.GetTextureHandle(), 0u);
    EXPECT_EQ(mat.GetTint(), glm::vec4(1.0f));
    EXPECT_EQ(mat.GetUvRect(), glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
    EXPECT_EQ(mat.GetBlendMode(), MaterialBlendMode::Alpha);
}

TEST(MaterialAssetTest, SetName修改名称) {
    MaterialAsset mat(1, "old_name");
    mat.SetName("new_name");
    EXPECT_EQ(mat.GetName(), "new_name");
}

TEST(MaterialAssetTest, SetShaderVariant修改着色器变体) {
    MaterialAsset mat(1, "test");
    mat.SetShaderVariant("PBR_LIT");
    EXPECT_EQ(mat.GetShaderVariant(), "PBR_LIT");
}

TEST(MaterialAssetTest, SetTextureHandle修改句柄) {
    MaterialAsset mat(1, "test");
    mat.SetTextureHandle(42u);
    EXPECT_EQ(mat.GetTextureHandle(), 42u);
}

TEST(MaterialAssetTest, SetTint修改染色) {
    MaterialAsset mat(1, "test");
    glm::vec4 tint(0.5f, 0.6f, 0.7f, 0.8f);
    mat.SetTint(tint);
    EXPECT_EQ(mat.GetTint(), tint);
}

TEST(MaterialAssetTest, SetBlendMode修改混合模式) {
    MaterialAsset mat(1, "test");
    mat.SetBlendMode(MaterialBlendMode::Additive);
    EXPECT_EQ(mat.GetBlendMode(), MaterialBlendMode::Additive);

    mat.SetBlendMode(MaterialBlendMode::Opaque);
    EXPECT_EQ(mat.GetBlendMode(), MaterialBlendMode::Opaque);
}

TEST(MaterialAssetTest, 设置基色和自发光) {
    MaterialAsset mat(1, "test");
    glm::vec4 base_color(0.1f, 0.2f, 0.3f, 1.0f);
    glm::vec3 emissive(1.0f, 0.5f, 0.0f);
    mat.SetBaseColor(base_color);
    mat.SetEmissiveColor(emissive);
    EXPECT_EQ(mat.GetBaseColor(), base_color);
    EXPECT_EQ(mat.GetEmissiveColor(), emissive);
}

TEST(MaterialAssetTest, TextureSlots默认全零) {
    MaterialAsset mat(1, "test");
    auto slots = mat.GetTextureSlots();
    EXPECT_EQ(slots.albedo, 0u);
    EXPECT_EQ(slots.normal, 0u);
    EXPECT_EQ(slots.metallic_roughness, 0u);
    EXPECT_EQ(slots.emissive, 0u);
    EXPECT_EQ(slots.occlusion, 0u);
}

TEST(MaterialAssetTest, ScalarOverrides默认值) {
    MaterialAsset mat(1, "test");
    auto scalars = mat.GetScalarOverrides();
    EXPECT_FLOAT_EQ(scalars.metallic, 0.0f);
    EXPECT_FLOAT_EQ(scalars.roughness, 0.5f);
    EXPECT_FLOAT_EQ(scalars.ao, 1.0f);
    EXPECT_FLOAT_EQ(scalars.normal_strength, 1.0f);
    EXPECT_FLOAT_EQ(scalars.alpha_cutoff, 0.5f);
    EXPECT_FALSE(scalars.alpha_test);
}

TEST(MaterialAssetTest, RasterOverrides默认值) {
    MaterialAsset mat(1, "test");
    auto raster = mat.GetRasterOverrides();
    EXPECT_FALSE(raster.double_sided);
}

// ============================================================
// 数据资产封装测试
// ============================================================

TEST(DmeshAssetTest, 存储路径和数据) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    DmeshAsset mesh("test.dmesh", data);
    EXPECT_EQ(mesh.GetPath(), "test.dmesh");
    EXPECT_EQ(mesh.GetData().size(), 4u);
    EXPECT_EQ(mesh.GetData()[0], 0x01);
    EXPECT_EQ(mesh.GetData()[3], 0x04);
}

TEST(DanimAssetTest, 存储路径和数据) {
    std::vector<uint8_t> data = {0xAA, 0xBB};
    DanimAsset anim("test.danim", data);
    EXPECT_EQ(anim.GetPath(), "test.danim");
    EXPECT_EQ(anim.GetData().size(), 2u);
}

TEST(DskelAssetTest, 存储路径和数据) {
    std::vector<uint8_t> data(256, 0xFF);
    DskelAsset skel("test.dskel", data);
    EXPECT_EQ(skel.GetPath(), "test.dskel");
    EXPECT_EQ(skel.GetData().size(), 256u);
}

TEST(AudioClipAssetTest, 存储路径和数据) {
    std::vector<uint8_t> data = {0x52, 0x49, 0x46, 0x46};  // RIFF header
    AudioClipAsset clip("test.wav", data);
    EXPECT_EQ(clip.GetPath(), "test.wav");
    EXPECT_EQ(clip.GetData().size(), 4u);
}

// ============================================================
// AssetManager 配置测试
// ============================================================

TEST(AssetManagerTest, ConfigureDataRoot设置和获取) {
    AssetManager mgr;
    EXPECT_EQ(mgr.GetDataRoot(), "data");  // 默认值

    mgr.ConfigureDataRoot("custom_assets");
    EXPECT_EQ(mgr.GetDataRoot(), "custom_assets");
}

TEST(AssetManagerTest, 多次创建同名材质实例不冲突) {
    AssetManager mgr;
    auto mat1 = mgr.CreateMaterialInstance("same_name");
    auto mat2 = mgr.CreateMaterialInstance("same_name");
    // 同名但不同 ID
    EXPECT_NE(mat1->GetId(), mat2->GetId());
    EXPECT_EQ(mat1->GetName(), mat2->GetName());
}

TEST(AssetManagerTest, 大量材质实例创建和获取) {
    AssetManager mgr;
    constexpr int kCount = 100;
    std::vector<unsigned int> ids;
    ids.reserve(kCount);

    for (int i = 0; i < kCount; ++i) {
        auto mat = mgr.CreateMaterialInstance("mat_" + std::to_string(i));
        ids.push_back(mat->GetId());
    }

    // 验证所有实例都可获取
    for (int i = 0; i < kCount; ++i) {
        auto mat = mgr.GetMaterialInstance(ids[i]);
        ASSERT_NE(mat, nullptr);
        EXPECT_EQ(mat->GetName(), "mat_" + std::to_string(i));
    }

    auto listed_ids = mgr.ListMaterialInstanceIds();
    EXPECT_EQ(listed_ids.size(), static_cast<size_t>(kCount));
}

// ============================================================
// 路径解析测试
// ============================================================

TEST(AssetManagerTest, NormalizeAssetPath空字符串返回空) {
    AssetManager mgr;
    EXPECT_EQ(mgr.NormalizeAssetPath(""), "");
}

TEST(AssetManagerTest, NormalizeAssetPath剥离Data前缀) {
    AssetManager mgr;
    // data/ 前缀应被剥离为相对逻辑路径
    std::string result = mgr.NormalizeAssetPath("data/textures/sprite.png");
    EXPECT_EQ(result, "textures/sprite.png");
}

TEST(AssetManagerTest, NormalizeAssetPath无前缀保留原路径) {
    AssetManager mgr;
    // 无 data/ 前缀的路径应保留
    std::string result = mgr.NormalizeAssetPath("textures/sprite.png");
    EXPECT_EQ(result, "textures/sprite.png");
}

TEST(AssetManagerTest, NormalizeAssetPath剥离BinData前缀) {
    AssetManager mgr;
    std::string result = mgr.NormalizeAssetPath("bin/data/audio/bgm.wav");
    EXPECT_EQ(result, "audio/bgm.wav");
}

TEST(AssetManagerTest, NormalizeAssetPath路径规范化) {
    AssetManager mgr;
    // 路径中含 ./ 或 ../ 应被规范化
    std::string result = mgr.NormalizeAssetPath("textures/../audio/./bgm.wav");
    EXPECT_FALSE(result.empty());
    // 规范化后不应包含 ./
    EXPECT_EQ(result.find("./"), std::string::npos);
}

TEST(AssetManagerTest, ResolveAssetPath拼接DataRoot) {
    AssetManager mgr;
    mgr.ConfigureDataRoot("data");
    std::string result = mgr.ResolveAssetPath("textures/sprite.png");
    // 应基于 data root 拼接
    EXPECT_NE(result, "textures/sprite.png");  // 不应等于原始逻辑路径
    EXPECT_NE(result.find("textures/sprite.png"), std::string::npos);  // 但应包含
}

TEST(AssetManagerTest, ConfigureDataRoot空值不修改) {
    AssetManager mgr;
    std::string original = mgr.GetDataRoot();
    mgr.ConfigureDataRoot("");
    EXPECT_EQ(mgr.GetDataRoot(), original);  // 空值不改变 data root
}

TEST(AssetManagerTest, ConfigureDataRoot自定义路径) {
    AssetManager mgr;
    mgr.ConfigureDataRoot("custom_assets");
    EXPECT_EQ(mgr.GetDataRoot(), "custom_assets");
}

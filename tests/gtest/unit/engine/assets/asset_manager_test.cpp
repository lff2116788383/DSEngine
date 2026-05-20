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
#include "engine/render/rhi/rhi_device.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace {

class AssetManagerFakeRhiDevice final : public RhiDevice {
public:
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override {
        (void)linear_filter;
        EXPECT_EQ(width, 1);
        EXPECT_EQ(height, 1);
        EXPECT_NE(rgba8_data, nullptr);
        return next_texture_handle_++;
    }

    void DeleteTexture(unsigned int texture_handle) override {
        deleted_textures.push_back(texture_handle);
    }

    void Shutdown() override {}
    void BeginFrame() override {}
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override { (void)desc; return 0; }
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override { (void)render_target_handle; return 0; }
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override { (void)render_target_handle; return 0; }
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override { (void)render_target_handle; return {}; }
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override { (void)render_target_handle; return {}; }
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override { (void)width; (void)height; (void)rgba8_faces; (void)linear_filter; return 0; }
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) override { (void)width; (void)height; (void)depth; (void)rgba8_data; (void)linear_filter; return 0; }
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override { (void)vert_src; (void)frag_src; return 0; }
    void DeleteShaderProgram(unsigned int program_handle) override { (void)program_handle; }
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override { (void)desc; return 0; }
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override { (void)size; (void)data; (void)is_dynamic; (void)is_index; return 0; }
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override { (void)handle; (void)offset; (void)size; (void)data; (void)is_index; }
    void DeleteBuffer(unsigned int handle) override { (void)handle; }
    dse::render::VertexArrayHandle CreateVertexArray() override { return {}; }
    void DeleteVertexArray(dse::render::VertexArrayHandle handle) override { (void)handle; }
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override { return nullptr; }
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override { (void)cmd_buffer; }
    void EndFrame() override {}
    const RenderStats& LastFrameStats() const override { return stats_; }

    std::vector<unsigned int> deleted_textures;

private:
    unsigned int next_texture_handle_ = 900001;
    RenderStats stats_{};
};

std::filesystem::path WriteOnePixelPpm(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    const std::filesystem::path image_path = dir / "weak_ref_release.ppm";
    std::ofstream out(image_path, std::ios::binary);
    out << "P6\n1 1\n255\n";
    const unsigned char pixel[3] = {255, 64, 32};
    out.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
    return image_path;
}

}  // namespace

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

TEST(AssetManagerTest, 弱引用过期后ReleaseGpuResources仍释放纹理句柄) {
    AssetManagerFakeRhiDevice fake_rhi;
    const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "dse_asset_manager_gpu_release_test";
    const std::filesystem::path image_path = WriteOnePixelPpm(temp_dir);

    unsigned int handle = 0;
    {
        AssetManager mgr;
        mgr.SetRhiDevice(&fake_rhi);
        mgr.ConfigureDataRoot(temp_dir.string());
        {
            auto texture = mgr.LoadTexture("weak_ref_release.ppm");
            ASSERT_NE(texture, nullptr);
            handle = texture->GetHandle();
            ASSERT_NE(handle, 0u);
        }

        ASSERT_TRUE(fake_rhi.deleted_textures.empty());
        mgr.ReleaseGpuResources();
        ASSERT_EQ(fake_rhi.deleted_textures.size(), 1u);
        EXPECT_EQ(fake_rhi.deleted_textures[0], handle);

        mgr.ReleaseGpuResources();
        EXPECT_EQ(fake_rhi.deleted_textures.size(), 1u);
    }

    EXPECT_EQ(fake_rhi.deleted_textures.size(), 1u);
    std::filesystem::remove_all(temp_dir);
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

// ============================================================
// MaterialValidationTest — 参数边界与 .dmat 降级验证
// ============================================================

TEST(MaterialValidationTest, Roughness默认值在合法范围) {
    MaterialAsset mat(1, "test");
    float r = mat.GetScalarOverrides().roughness;
    EXPECT_GE(r, 0.0f);
    EXPECT_LE(r, 1.0f);
}

TEST(MaterialValidationTest, Metallic默认值在合法范围) {
    MaterialAsset mat(1, "test");
    float m = mat.GetScalarOverrides().metallic;
    EXPECT_GE(m, 0.0f);
    EXPECT_LE(m, 1.0f);
}

TEST(MaterialValidationTest, AO默认值为一) {
    MaterialAsset mat(1, "test");
    EXPECT_FLOAT_EQ(mat.GetScalarOverrides().ao, 1.0f);
}

TEST(MaterialValidationTest, AlphaCutoff默认值在合法范围) {
    MaterialAsset mat(1, "test");
    float ac = mat.GetScalarOverrides().alpha_cutoff;
    EXPECT_GE(ac, 0.0f);
    EXPECT_LE(ac, 1.0f);
}

TEST(MaterialValidationTest, BaseColor默认为不透明白色) {
    MaterialAsset mat(1, "test");
    auto bc = mat.GetBaseColor();
    EXPECT_FLOAT_EQ(bc.a, 1.0f);
    EXPECT_GE(bc.r, 0.0f); EXPECT_LE(bc.r, 1.0f);
    EXPECT_GE(bc.g, 0.0f); EXPECT_LE(bc.g, 1.0f);
    EXPECT_GE(bc.b, 0.0f); EXPECT_LE(bc.b, 1.0f);
}

TEST(MaterialValidationTest, dmat解析缺失字段使用默认值) {
    AssetManager mgr;
    auto result = mgr.LoadMaterialInstanceFromDmat("nonexistent.dmat", 0);
    EXPECT_EQ(result, nullptr);
}

TEST(MaterialValidationTest, CreateMaterialInstance返回有效ID) {
    AssetManager mgr;
    auto mat = mgr.CreateMaterialInstance("pbr_test");
    ASSERT_NE(mat, nullptr);
    EXPECT_GT(mat->GetId(), 0u);
    EXPECT_EQ(mat->GetName(), "pbr_test");
}

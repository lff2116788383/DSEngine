/**
 * @file dssl_material_test.cpp
 * @brief DSSLMaterialInstance / DSSLMaterialLoader 单元测试（纯 CPU 端）
 *
 * 测试策略：
 * - DSSLUniformInfo 默认值
 * - DSSLShaderType 枚举值
 * - DSSLMaterialInstance uniform 设置/获取
 * - DSSLMaterialInstance 材质属性映射（GetBaseColor/GetMetallic 等）
 * - RenderModes 默认值和 GetAlphaTest/GetDoubleSided 逻辑
 * - DSSLMaterialLoader::Instance() 单例
 * - DSSLMaterialLoader::GetInstance() 未注册返回 nullptr
 * - DSSLMaterialLoader::Clear() 清除缓存
 */

#include <gtest/gtest.h>
#include "engine/render/material/dssl_material_instance.h"
#include "engine/render/material/dssl_material_loader.h"
#include <glm/glm.hpp>
#include <filesystem>
#include <fstream>

using namespace dse::render;

// ============================================================
// DSSLUniformInfo 默认值
// ============================================================

TEST(DSSLUniformInfoTest, 默认值) {
    DSSLUniformInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.type.empty());
    EXPECT_TRUE(info.default_value.empty());
    EXPECT_FALSE(info.is_sampler);
    EXPECT_TRUE(info.hints.empty());
}

// ============================================================
// DSSLShaderType 枚举
// ============================================================

TEST(DSSLShaderTypeTest, 枚举值) {
    EXPECT_EQ(static_cast<int>(DSSLShaderType::Surface), 0);
    EXPECT_EQ(static_cast<int>(DSSLShaderType::Unlit), 1);
    EXPECT_EQ(static_cast<int>(DSSLShaderType::Particle), 2);
    EXPECT_EQ(static_cast<int>(DSSLShaderType::Sky), 3);
    EXPECT_EQ(static_cast<int>(DSSLShaderType::Postprocess), 4);
    EXPECT_EQ(static_cast<int>(DSSLShaderType::Canvas), 5);
}

// ============================================================
// DSSLMaterialInstance 创建和基础属性
// ============================================================

TEST(DSSLMaterialInstanceTest, 构造函数) {
    DSSLMaterialInstance inst(42, "test.dssl");
    EXPECT_EQ(inst.GetId(), 42u);
    EXPECT_EQ(inst.GetDSSLPath(), "test.dssl");
    EXPECT_EQ(inst.GetShaderType(), DSSLShaderType::Surface);
    EXPECT_TRUE(inst.GetUniformInfos().empty());
}

TEST(DSSLMaterialInstanceTest, SetShaderType) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetShaderType(DSSLShaderType::Unlit);
    EXPECT_EQ(inst.GetShaderType(), DSSLShaderType::Unlit);
}

// ============================================================
// Uniform 设置/获取往返
// ============================================================

TEST(DSSLMaterialInstanceTest, Float设置获取) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetFloat("roughness", 0.75f);
    EXPECT_FLOAT_EQ(inst.GetFloat("roughness"), 0.75f);
    EXPECT_FLOAT_EQ(inst.GetFloat("nonexist", 99.0f), 99.0f);
}

TEST(DSSLMaterialInstanceTest, Vec2设置获取) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec2("uv_offset", glm::vec2(0.5f, 0.3f));
    glm::vec2 v = inst.GetVec2("uv_offset");
    EXPECT_FLOAT_EQ(v.x, 0.5f);
    EXPECT_FLOAT_EQ(v.y, 0.3f);
}

TEST(DSSLMaterialInstanceTest, Vec3设置获取) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec3("emission_color", glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 v = inst.GetVec3("emission_color");
    EXPECT_FLOAT_EQ(v.r, 1.0f);
    EXPECT_FLOAT_EQ(v.g, 0.0f);
}

TEST(DSSLMaterialInstanceTest, Vec4设置获取) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec4("albedo_color", glm::vec4(0.2f, 0.3f, 0.4f, 1.0f));
    glm::vec4 v = inst.GetVec4("albedo_color");
    EXPECT_FLOAT_EQ(v.r, 0.2f);
    EXPECT_FLOAT_EQ(v.a, 1.0f);
}

TEST(DSSLMaterialInstanceTest, Int设置获取) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetInt("layers", 3);
    EXPECT_EQ(inst.GetInt("layers"), 3);
    EXPECT_EQ(inst.GetInt("nonexist", -1), -1);
}

TEST(DSSLMaterialInstanceTest, Texture设置获取) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetTexture("albedo_tex", 100);
    EXPECT_EQ(inst.GetTexture("albedo_tex"), 100u);
    EXPECT_EQ(inst.GetTexture("nonexist"), 0u);
}

// ============================================================
// 材质属性映射
// ============================================================

TEST(DSSLMaterialInstanceTest, GetBaseColor_默认白色) {
    DSSLMaterialInstance inst(1, "x.dssl");
    glm::vec4 bc = inst.GetBaseColor();
    EXPECT_FLOAT_EQ(bc.r, 1.0f);
    EXPECT_FLOAT_EQ(bc.g, 1.0f);
    EXPECT_FLOAT_EQ(bc.b, 1.0f);
    EXPECT_FLOAT_EQ(bc.a, 1.0f);
}

TEST(DSSLMaterialInstanceTest, GetBaseColor_albedo_color优先) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec4("albedo_color", glm::vec4(0.1f, 0.2f, 0.3f, 0.9f));
    inst.SetVec4("base_color", glm::vec4(0.5f));
    glm::vec4 bc = inst.GetBaseColor();
    EXPECT_FLOAT_EQ(bc.r, 0.1f);
    EXPECT_FLOAT_EQ(bc.a, 0.9f);
}

TEST(DSSLMaterialInstanceTest, GetBaseColor_vec3回退) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec3("albedo_color", glm::vec3(0.5f, 0.6f, 0.7f));
    glm::vec4 bc = inst.GetBaseColor();
    EXPECT_FLOAT_EQ(bc.r, 0.5f);
    EXPECT_FLOAT_EQ(bc.a, 1.0f);
}

TEST(DSSLMaterialInstanceTest, GetMetallic_默认零) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FLOAT_EQ(inst.GetMetallic(), 0.0f);
}

TEST(DSSLMaterialInstanceTest, GetRoughness_默认0_5) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FLOAT_EQ(inst.GetRoughness(), 0.5f);
}

TEST(DSSLMaterialInstanceTest, GetAO_默认1) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FLOAT_EQ(inst.GetAO(), 1.0f);
}

TEST(DSSLMaterialInstanceTest, GetNormalStrength_默认1) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FLOAT_EQ(inst.GetNormalStrength(), 1.0f);
}

TEST(DSSLMaterialInstanceTest, GetAlphaCutoff_默认0_5) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FLOAT_EQ(inst.GetAlphaCutoff(), 0.5f);
}

TEST(DSSLMaterialInstanceTest, GetEmissiveColor_默认黑色) {
    DSSLMaterialInstance inst(1, "x.dssl");
    glm::vec3 ec = inst.GetEmissiveColor();
    EXPECT_FLOAT_EQ(ec.r, 0.0f);
    EXPECT_FLOAT_EQ(ec.g, 0.0f);
    EXPECT_FLOAT_EQ(ec.b, 0.0f);
}

TEST(DSSLMaterialInstanceTest, GetEmissiveColor_vec3优先) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec3("emission_color", glm::vec3(1.0f, 0.5f, 0.0f));
    glm::vec3 ec = inst.GetEmissiveColor();
    EXPECT_FLOAT_EQ(ec.r, 1.0f);
    EXPECT_FLOAT_EQ(ec.g, 0.5f);
}

TEST(DSSLMaterialInstanceTest, GetEmissiveColor_vec4回退) {
    DSSLMaterialInstance inst(1, "x.dssl");
    inst.SetVec4("emissive_color", glm::vec4(0.2f, 0.3f, 0.4f, 1.0f));
    glm::vec3 ec = inst.GetEmissiveColor();
    EXPECT_FLOAT_EQ(ec.r, 0.2f);
    EXPECT_FLOAT_EQ(ec.b, 0.4f);
}

// ============================================================
// Texture 映射（多命名回退）
// ============================================================

TEST(DSSLMaterialInstanceTest, GetAlbedoTexture_多名称回退) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_EQ(inst.GetAlbedoTexture(), 0u);

    inst.SetTexture("base_texture", 200);
    EXPECT_EQ(inst.GetAlbedoTexture(), 200u);

    inst.SetTexture("albedo_tex", 300);
    EXPECT_EQ(inst.GetAlbedoTexture(), 300u);
}

TEST(DSSLMaterialInstanceTest, GetNormalTexture_多名称回退) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_EQ(inst.GetNormalTexture(), 0u);

    inst.SetTexture("normal_map", 500);
    EXPECT_EQ(inst.GetNormalTexture(), 500u);

    inst.SetTexture("normal_tex", 600);
    EXPECT_EQ(inst.GetNormalTexture(), 600u);
}

// ============================================================
// RenderModes 默认值和逻辑
// ============================================================

TEST(DSSLMaterialInstanceTest, RenderModes_默认值) {
    DSSLMaterialInstance inst(1, "x.dssl");
    auto& rm = inst.GetRenderModes();
    EXPECT_EQ(rm.blend, "disabled");
    EXPECT_EQ(rm.cull, "back");
    EXPECT_EQ(rm.lighting_model, "pbr");
    EXPECT_TRUE(rm.shadows_enabled);
    EXPECT_FALSE(rm.alpha_test);
}

TEST(DSSLMaterialInstanceTest, GetAlphaTest_false默认) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FALSE(inst.GetAlphaTest());
}

TEST(DSSLMaterialInstanceTest, GetAlphaTest_设置后true) {
    DSSLMaterialInstance inst(1, "x.dssl");
    DSSLMaterialInstance::RenderModes modes;
    modes.alpha_test = true;
    inst.SetRenderModes(modes);
    EXPECT_TRUE(inst.GetAlphaTest());
}

TEST(DSSLMaterialInstanceTest, GetDoubleSided_cull_disabled) {
    DSSLMaterialInstance inst(1, "x.dssl");
    EXPECT_FALSE(inst.GetDoubleSided());

    DSSLMaterialInstance::RenderModes modes;
    modes.cull = "disabled";
    inst.SetRenderModes(modes);
    EXPECT_TRUE(inst.GetDoubleSided());
}

// ============================================================
// DSSLMaterialLoader 单例和基础操作
// ============================================================

TEST(DSSLMaterialLoaderTest, Instance_单例) {
    auto& a = DSSLMaterialLoader::Instance();
    auto& b = DSSLMaterialLoader::Instance();
    EXPECT_EQ(&a, &b);
}

TEST(DSSLMaterialLoaderTest, GetInstance_未注册返回nullptr) {
    auto& loader = DSSLMaterialLoader::Instance();
    EXPECT_EQ(loader.GetInstance(999999), nullptr);
    EXPECT_EQ(loader.GetInstance(0), nullptr);
}

TEST(DSSLMaterialLoaderTest, LoadFromFile_不存在文件返回nullptr) {
    auto& loader = DSSLMaterialLoader::Instance();
    auto inst = loader.LoadFromFile("nonexistent_path_12345.dssl");
    EXPECT_EQ(inst, nullptr);
}

TEST(DSSLMaterialLoaderTest, CreateInstance_不存在文件返回nullptr) {
    auto& loader = DSSLMaterialLoader::Instance();
    auto inst = loader.CreateInstance("nonexistent_path_67890.dssl");
    EXPECT_EQ(inst, nullptr);
}

namespace {
std::filesystem::path WriteTempDSSL(const std::string& name, const std::string& content) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}
} // namespace

TEST(DSSLMaterialLoaderTest, LoadFromFile_空文件返回nullptr) {
    // 文件能打开但内容为空 → 不是有效 DSSL，应失败而非返回全默认空模板。
    auto& loader = DSSLMaterialLoader::Instance();
    loader.Clear();
    auto path = WriteTempDSSL("dse_empty_material.dssl", "");
    EXPECT_EQ(loader.LoadFromFile(path.string()), nullptr);
    std::filesystem::remove(path);
}

TEST(DSSLMaterialLoaderTest, LoadFromFile_纯注释文件返回nullptr) {
    // 只有注释/空行、无任何有效内容 → 同样视为无效 DSSL。
    auto& loader = DSSLMaterialLoader::Instance();
    loader.Clear();
    auto path = WriteTempDSSL("dse_comment_only_material.dssl",
                              "// just a comment\n\n   \n// another\n");
    EXPECT_EQ(loader.LoadFromFile(path.string()), nullptr);
    std::filesystem::remove(path);
}

TEST(DSSLMaterialLoaderTest, LoadFromFile_有效DSSL成功) {
    // 回归保护：含 shader_type / uniform 的有效 DSSL 仍能正常加载并解析。
    auto& loader = DSSLMaterialLoader::Instance();
    loader.Clear();
    const std::string src =
        "shader_type unlit\n"
        "uniform float roughness = 0.3;\n"
        "void fragment() {}\n";
    auto path = WriteTempDSSL("dse_valid_material.dssl", src);
    auto inst = loader.LoadFromFile(path.string());
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->GetShaderType(), DSSLShaderType::Unlit);
    EXPECT_FLOAT_EQ(inst->GetFloat("roughness"), 0.3f);
    std::filesystem::remove(path);
}

TEST(DSSLMaterialLoaderTest, LoadFromFile_vec3默认值解析w补1) {
    // 回归保护：vec3 默认值经解析后应得到 (x,y,z,1.0)，
    // 校验 vec3 解析不再依赖 "vec4"+substr 字符串拼接 hack。
    auto& loader = DSSLMaterialLoader::Instance();
    loader.Clear();
    const std::string src =
        "shader_type unlit\n"
        "uniform vec3 emission_color = vec3(0.2, 0.4, 0.6);\n"
        "uniform vec4 albedo_color = vec4(0.1, 0.2, 0.3, 0.5);\n"
        "void fragment() {}\n";
    auto path = WriteTempDSSL("dse_vec3_default_material.dssl", src);
    auto inst = loader.LoadFromFile(path.string());
    ASSERT_NE(inst, nullptr);
    // vec3 默认值：第 4 分量补 1.0。
    const glm::vec4 emission = inst->GetVec4("emission_color");
    EXPECT_FLOAT_EQ(emission.x, 0.2f);
    EXPECT_FLOAT_EQ(emission.y, 0.4f);
    EXPECT_FLOAT_EQ(emission.z, 0.6f);
    EXPECT_FLOAT_EQ(emission.w, 1.0f);
    // vec4 默认值：四分量原样保留（不受 vec3 路径影响）。
    const glm::vec4 albedo = inst->GetVec4("albedo_color");
    EXPECT_FLOAT_EQ(albedo.x, 0.1f);
    EXPECT_FLOAT_EQ(albedo.y, 0.2f);
    EXPECT_FLOAT_EQ(albedo.z, 0.3f);
    EXPECT_FLOAT_EQ(albedo.w, 0.5f);
    std::filesystem::remove(path);
}

TEST(DSSLMaterialLoaderTest, Clear_清除后查询为空) {
    auto& loader = DSSLMaterialLoader::Instance();
    loader.Clear();
    EXPECT_EQ(loader.GetInstance(900000), nullptr);
}

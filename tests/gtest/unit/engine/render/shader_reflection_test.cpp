/**
 * @file shader_reflection_test.cpp
 * @brief shader_reflection.h 辅助函数单元测试
 *
 * 覆盖：ComputeFlatTextureUnits, AutoCreateInputLayout, ValidateTextureSlotOverlaps,
 *       ValidateUBOSize, ValidateUBOBindings, ValidateVertexInputs,
 *       ExtractDescriptorBindings
 */

#include <gtest/gtest.h>
#include "engine/render/shader_reflection.h"
#include "engine/render/shaders/generated/embed/pbr_frag_reflect.gen.h"
#include "engine/render/shaders/generated/embed/pbr_vert_reflect.gen.h"

using namespace dse::render;

// ============================================================
// ComputeFlatTextureUnits
// ============================================================

TEST(ComputeFlatTextureUnitsTest, 空反射返回零) {
    shader_reflect::StageReflection empty{};
    std::vector<gl_reflect::TextureUnitEntry> entries;
    uint32_t next = gl_reflect::ComputeFlatTextureUnits(empty, entries);
    EXPECT_EQ(next, 0u);
    EXPECT_TRUE(entries.empty());
}

TEST(ComputeFlatTextureUnitsTest, 单纹理_unit_0) {
    shader_reflect::ResourceBinding tex = {
        "u_texture", shader_reflect::ResourceType::SampledImage,
        2, 1, 0, 1, shader_reflect::ImageDimension::Dim2D, false
    };
    shader_reflect::StageReflection refl{};
    refl.sampled_images = &tex;
    refl.sampled_image_count = 1;

    std::vector<gl_reflect::TextureUnitEntry> entries;
    uint32_t next = gl_reflect::ComputeFlatTextureUnits(refl, entries);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_STREQ(entries[0].name, "u_texture");
    EXPECT_EQ(entries[0].unit, 0u);
    EXPECT_EQ(entries[0].array_count, 1u);
    EXPECT_EQ(next, 1u);
}

TEST(ComputeFlatTextureUnitsTest, 数组纹理占据连续unit) {
    shader_reflect::ResourceBinding textures[] = {
        {"u_texture", shader_reflect::ResourceType::SampledImage,
         2, 1, 0, 1, shader_reflect::ImageDimension::Dim2D, false},
        {"u_shadow_maps", shader_reflect::ResourceType::SampledImage,
         2, 2, 0, 3, shader_reflect::ImageDimension::Dim2D, false},
        {"u_after_shadow", shader_reflect::ResourceType::SampledImage,
         2, 3, 0, 1, shader_reflect::ImageDimension::Dim2D, false},
    };
    shader_reflect::StageReflection refl{};
    refl.sampled_images = textures;
    refl.sampled_image_count = 3;

    std::vector<gl_reflect::TextureUnitEntry> entries;
    uint32_t next = gl_reflect::ComputeFlatTextureUnits(refl, entries);
    ASSERT_EQ(entries.size(), 3u);
    // u_texture: unit=0, count=1
    EXPECT_EQ(entries[0].unit, 0u);
    EXPECT_EQ(entries[0].array_count, 1u);
    // u_shadow_maps: unit=1, count=3 (占 1,2,3)
    EXPECT_EQ(entries[1].unit, 1u);
    EXPECT_EQ(entries[1].array_count, 3u);
    // u_after_shadow: unit=4
    EXPECT_EQ(entries[2].unit, 4u);
    EXPECT_EQ(next, 5u);
}

TEST(ComputeFlatTextureUnitsTest, base_unit_偏移) {
    shader_reflect::ResourceBinding tex = {
        "u_texture", shader_reflect::ResourceType::SampledImage,
        2, 1, 0, 1, shader_reflect::ImageDimension::Dim2D, false
    };
    shader_reflect::StageReflection refl{};
    refl.sampled_images = &tex;
    refl.sampled_image_count = 1;

    std::vector<gl_reflect::TextureUnitEntry> entries;
    uint32_t next = gl_reflect::ComputeFlatTextureUnits(refl, entries, 5);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].unit, 5u);
    EXPECT_EQ(next, 6u);
}

TEST(ComputeFlatTextureUnitsTest, PBR反射数据_无重叠) {
    using namespace generated_shaders::reflect;
    std::vector<gl_reflect::TextureUnitEntry> entries;
    gl_reflect::ComputeFlatTextureUnits(kpbr_frag_reflection, entries);
    // 校验无重叠
    EXPECT_EQ(shader_reflect_debug::ValidateTextureSlotOverlaps(entries), 0u);
    // PBR frag 应有多个纹理
    EXPECT_GE(entries.size(), 10u);
}

// ============================================================
// AutoCreateInputLayout (DX11)
// ============================================================

TEST(AutoCreateInputLayoutTest, PBR顶点布局) {
    using namespace generated_shaders::reflect;
    std::vector<dx11_reflect::InputElementDesc> elements;
    uint32_t count = dx11_reflect::AutoCreateInputLayout(kpbr_vert_reflection, elements);
    EXPECT_GT(count, 0u);
    EXPECT_EQ(count, static_cast<uint32_t>(elements.size()));
    // 第一个元素应是 TEXCOORD 语义
    EXPECT_STREQ(elements[0].semantic_name, "TEXCOORD");
}

TEST(AutoCreateInputLayoutTest, 空输入返回零) {
    shader_reflect::StageReflection empty{};
    std::vector<dx11_reflect::InputElementDesc> elements;
    uint32_t count = dx11_reflect::AutoCreateInputLayout(empty, elements);
    EXPECT_EQ(count, 0u);
    EXPECT_TRUE(elements.empty());
}

// ============================================================
// ValidateTextureSlotOverlaps
// ============================================================

TEST(ValidateTextureSlotOverlapsTest, 无重叠) {
    std::vector<gl_reflect::TextureUnitEntry> entries = {
        {"tex0", 0, 1},
        {"tex1", 1, 1},
        {"tex2", 2, 3}, // 占 2,3,4
        {"tex3", 5, 1},
    };
    EXPECT_EQ(shader_reflect_debug::ValidateTextureSlotOverlaps(entries), 0u);
}

TEST(ValidateTextureSlotOverlapsTest, 检测重叠) {
    std::vector<gl_reflect::TextureUnitEntry> entries = {
        {"tex0", 0, 3}, // 占 0,1,2
        {"tex1", 2, 1}, // 与 tex0 重叠
    };
    EXPECT_GT(shader_reflect_debug::ValidateTextureSlotOverlaps(entries), 0u);
}

// ============================================================
// ValidateUBOSize
// ============================================================

TEST(ValidateUBOSizeTest, 大小匹配返回true) {
    shader_reflect::ResourceBinding ubo = {
        "PerFrame", shader_reflect::ResourceType::UniformBuffer,
        0, 0, 144, 1, shader_reflect::ImageDimension::Dim2D, false
    };
    shader_reflect::StageReflection refl{};
    refl.uniform_buffers = &ubo;
    refl.uniform_buffer_count = 1;

    EXPECT_TRUE(shader_reflect_debug::ValidateUBOSize(refl, "PerFrame", 144));
}

TEST(ValidateUBOSizeTest, 大小不匹配返回false) {
    shader_reflect::ResourceBinding ubo = {
        "PerFrame", shader_reflect::ResourceType::UniformBuffer,
        0, 0, 144, 1, shader_reflect::ImageDimension::Dim2D, false
    };
    shader_reflect::StageReflection refl{};
    refl.uniform_buffers = &ubo;
    refl.uniform_buffer_count = 1;

    EXPECT_FALSE(shader_reflect_debug::ValidateUBOSize(refl, "PerFrame", 128));
}

TEST(ValidateUBOSizeTest, 未找到UBO返回true) {
    shader_reflect::StageReflection refl{};
    EXPECT_TRUE(shader_reflect_debug::ValidateUBOSize(refl, "NonExistent", 64));
}

// ============================================================
// ValidateUBOBindings
// ============================================================

TEST(ValidateUBOBindingsTest, PBR反射数据无异常) {
    using namespace generated_shaders::reflect;
    EXPECT_EQ(shader_reflect_debug::ValidateUBOBindings(kpbr_frag_reflection, "PBR.frag"), 0u);
    EXPECT_EQ(shader_reflect_debug::ValidateUBOBindings(kpbr_vert_reflection, "PBR.vert"), 0u);
}

// ============================================================
// ValidateVertexInputs
// ============================================================

TEST(ValidateVertexInputsTest, PBR顶点输入合法) {
    using namespace generated_shaders::reflect;
    EXPECT_EQ(shader_reflect_debug::ValidateVertexInputs(kpbr_vert_reflection), 0u);
}

// ============================================================
// ExtractDescriptorBindings (Vulkan)
// ============================================================

TEST(ExtractDescriptorBindingsTest, PBR合并VertFrag) {
    using namespace generated_shaders::reflect;
    shader_reflect::ProgramReflection prog{kpbr_vert_reflection, kpbr_frag_reflection};

    std::vector<vk_reflect::DescriptorBinding> bindings;
    vk_reflect::ExtractDescriptorBindings(prog, bindings);

    EXPECT_GT(bindings.size(), 0u);
    // PerFrame UBO 应同时带有 VERTEX | FRAGMENT 标志
    bool found_perframe = false;
    for (const auto& b : bindings) {
        if (b.set == 0 && b.binding == 0) {
            found_perframe = true;
            EXPECT_TRUE(b.stage_flags & vk_reflect::VK_SHADER_STAGE_VERTEX_BIT);
            EXPECT_TRUE(b.stage_flags & vk_reflect::VK_SHADER_STAGE_FRAGMENT_BIT);
        }
    }
    EXPECT_TRUE(found_perframe);
}

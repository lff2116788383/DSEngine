/**
 * @file shader_embed_targets_test.cpp
 * @brief 三端共享着色器源的「多目标产物齐备」契约测试
 *
 * DSEngine 三个 RHI 后端（OpenGL / Vulkan / D3D11）不各自手写着色器，而是共享
 * engine/render/shaders/src 下同一份 GLSL 源，由 dse_shader_compiler 离线交叉
 * 编译成每个着色器一个 embed/<name>.gen.h，内含多目标产物：
 *   - k*_spv[]     SPIR-V 二进制      → Vulkan
 *   - k*_glsl430   桌面 GLSL 430      → OpenGL 桌面
 *   - k*_essl310   GL ES 310          → OpenGL ES / 移动
 *   - k*_hlsl      HLSL 源            → D3D11 (D3DCompile)
 *   - k*_dxbc[]    预编译 DXBC 字节码 → D3D11 备选
 *
 * 本测试断言：代表性着色器（含顶点/片元/计算 + gpu_driven 变体）的每个目标产物
 * 都非空。这锁住「单一源 → 全后端目标齐备」契约——若 codegen 回归导致某个后端
 * 的产物悄悄缺失（如 HLSL 没生成），任一后端将静默丢失该着色器，此测试会立即报错。
 */

#include <gtest/gtest.h>
#include <cstddef>

#include "engine/render/shaders/generated/embed/pbr_vert.gen.h"
#include "engine/render/shaders/generated/embed/pbr_frag.gen.h"
#include "engine/render/shaders/generated/embed/pbr_gpu_driven_vert.gen.h"
#include "engine/render/shaders/generated/embed/pbr_gpu_driven_frag.gen.h"
#include "engine/render/shaders/generated/embed/shadow_vert.gen.h"
#include "engine/render/shaders/generated/embed/shadow_gpu_driven_vert.gen.h"
#include "engine/render/shaders/generated/embed/postprocess_vert.gen.h"
#include "engine/render/shaders/generated/embed/postprocess_passthrough_frag.gen.h"
#include "engine/render/shaders/generated/embed/fxaa_frag.gen.h"
#include "engine/render/shaders/generated/embed/skybox_vert.gen.h"
#include "engine/render/shaders/generated/embed/skybox_frag.gen.h"
#include "engine/render/shaders/generated/embed/sprite_vert.gen.h"
#include "engine/render/shaders/generated/embed/sprite_frag.gen.h"
#include "engine/render/shaders/generated/embed/bloom_downsample_comp.gen.h"

using namespace dse::render::generated_shaders;

namespace {

// 校验单个着色器的全部后端目标产物非空。
// glsl430/essl310/hlsl 是 const char*（const char[] 亦可），用首字节判非空；
// spv/dxbc 是二进制数组，用 *_size 判非空。
#define EXPECT_ALL_BACKEND_TARGETS(name)                                              \
    do {                                                                              \
        EXPECT_GT(static_cast<size_t>(k##name##_spv_size), 0u)                        \
            << #name ": SPIR-V (Vulkan) 产物为空";                                     \
        EXPECT_GT(static_cast<size_t>(k##name##_dxbc_size), 0u)                       \
            << #name ": DXBC (D3D11) 产物为空";                                        \
        ASSERT_NE(k##name##_glsl430, nullptr) << #name ": GLSL430 指针为空";           \
        EXPECT_NE(k##name##_glsl430[0], '\0') << #name ": GLSL430 (GL 桌面) 产物为空"; \
        ASSERT_NE(k##name##_essl310, nullptr) << #name ": ESSL310 指针为空";           \
        EXPECT_NE(k##name##_essl310[0], '\0') << #name ": ESSL310 (GL ES) 产物为空";   \
        ASSERT_NE(k##name##_hlsl, nullptr) << #name ": HLSL 指针为空";                 \
        EXPECT_NE(k##name##_hlsl[0], '\0') << #name ": HLSL (D3D11) 产物为空";         \
    } while (0)

} // namespace

// 核心 PBR：三端主渲染着色器
TEST(ShaderEmbedTargetsTest, PBRAllThreeGoalsAreReady) {
    EXPECT_ALL_BACKEND_TARGETS(pbr_vert);
    EXPECT_ALL_BACKEND_TARGETS(pbr_frag);
}

// gpu_driven 变体：验证 @VARIANTS 机制对三端都产出（曾被误以为是 Vulkan 独有）
TEST(ShaderEmbedTargetsTest, GpuDrivenVariantWithAllThreeTargets) {
    EXPECT_ALL_BACKEND_TARGETS(pbr_gpu_driven_vert);
    EXPECT_ALL_BACKEND_TARGETS(pbr_gpu_driven_frag);
    EXPECT_ALL_BACKEND_TARGETS(shadow_gpu_driven_vert);
}

// 阴影 / 天空盒 / 精灵
TEST(ShaderEmbedTargetsTest, EmptyAllThreeGoalsAreReady) {
    EXPECT_ALL_BACKEND_TARGETS(shadow_vert);
    EXPECT_ALL_BACKEND_TARGETS(skybox_vert);
    EXPECT_ALL_BACKEND_TARGETS(skybox_frag);
    EXPECT_ALL_BACKEND_TARGETS(sprite_vert);
    EXPECT_ALL_BACKEND_TARGETS(sprite_frag);
}

// 后处理：全屏 passthrough（"copy"）+ FXAA
TEST(ShaderEmbedTargetsTest, AfterAllThreeGoalsAreReady) {
    EXPECT_ALL_BACKEND_TARGETS(postprocess_vert);
    EXPECT_ALL_BACKEND_TARGETS(postprocess_passthrough_frag);
    EXPECT_ALL_BACKEND_TARGETS(fxaa_frag);
}

// 计算着色器：bloom 降采样（三端均有 compute 目标）
TEST(ShaderEmbedTargetsTest, AllThreeGoalsAreReady) {
    EXPECT_ALL_BACKEND_TARGETS(bloom_downsample_comp);
}

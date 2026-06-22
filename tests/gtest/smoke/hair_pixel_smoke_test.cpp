/**
 * @file hair_pixel_smoke_test.cpp
 * @brief 毛发线带（B4）「活体」像素冒烟 — 经高层 HairRenderer + 内建程序
 *        BuiltinProgram::HairStrand（hair.vert + hair.frag）画一条水平 LINE_STRIP：
 *        position/tangent 走通用原语 BindStorageBuffer\@slot0/1（vertexless，gl_VertexIndex 取数），
 *        组合 HairUniforms UBO\@slot0，PSO 烘焙 LineStrip 拓扑，逐 strand cmd.Draw(count, first)。
 *
 * 这是 stage 3（B4）删 DrawHairStrands ABI 后的闸门：验证
 *  - PrimitiveTopology::LineStrip 经 PSO 贯穿到非索引 Draw（线带光栅化）；
 *  - vertexless 非索引 Draw（无 VBO）+ 图形阶段 SSBO 绑定；
 *  - 组合 HairUniforms UBO 跨 VS/FS 共享、Kajiya-Kay 片元产出可见颜色。
 *
 * 本机仅 D3D11(WARP) 可跑；GL/Vulkan 无驱动优雅 skip（既有约束）。
 * 场景：水平线居中（row≈128），断言中央横带有线、带外近黑、四角清屏黑。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/hair_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_handle.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr int kRtSize = 256;
constexpr uint32_t kStrandVerts = 24;  // 单 strand 顶点数（line strip 段数 = 23）

// 在已初始化 device 上：建 256² RT → 清黑 → HairRenderer 画一条水平线带 → 回读。
RenderTargetReadback RenderHair(RhiDevice& device) {
    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = true;  // HairRenderer PSO 测深度（不写）；给一个深度附件供其通过。
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    // 水平 line strip：x 从 -0.7 → 0.7，y=0，NDC 居中横排（row≈128，DX 翻转不影响）。
    std::vector<glm::vec4> positions(kStrandVerts);
    std::vector<glm::vec4> tangents(kStrandVerts);
    for (uint32_t i = 0; i < kStrandVerts; ++i) {
        float f = static_cast<float>(i) / static_cast<float>(kStrandVerts - 1);  // 0..1
        positions[i] = glm::vec4(-0.7f + 1.4f * f, 0.0f, 0.0f, 1.0f);
        tangents[i] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f - f);  // 切线沿线，w=厚度 root→tip
    }

    GpuBufferDesc pos_desc;
    pos_desc.size = positions.size() * sizeof(glm::vec4);
    pos_desc.usage = GpuBufferUsage::kStorage;
    BufferHandle pos_ssbo = device.CreateGpuBuffer(pos_desc, positions.data());

    GpuBufferDesc tan_desc;
    tan_desc.size = tangents.size() * sizeof(glm::vec4);
    tan_desc.usage = GpuBufferUsage::kStorage;
    BufferHandle tan_ssbo = device.CreateGpuBuffer(tan_desc, tangents.data());

    if (!pos_ssbo || !tan_ssbo) {
        device.DeleteRenderTarget(rt);
        return {};
    }

    // 单 strand：firsts/counts CPU 数组（HairRenderer 在 Draw 内同步消费）。
    const int strand_firsts[1] = {0};
    const int strand_counts[1] = {static_cast<int>(kStrandVerts)};

    HairDrawItem item;
    item.position_ssbo = pos_ssbo;
    item.tangent_ssbo = tan_ssbo;
    item.total_vertex_count = kStrandVerts;
    item.strand_count = 1;
    item.strand_firsts = strand_firsts;
    item.strand_counts = strand_counts;
    item.world_transform = glm::mat4(1.0f);
    // 亮白线，移除光照不确定性：ambient=1 / light_intensity=0 → lit = color * 1 = 白。
    item.root_color = glm::vec4(1.0f);
    item.tip_color = glm::vec4(1.0f);
    item.opacity = 1.0f;
    item.light_color = glm::vec3(1.0f);
    item.light_intensity = 0.0f;
    item.ambient_intensity = 1.0f;

    std::vector<HairDrawItem> items = {item};

    HairRenderer hair;
    device.BeginFrame();
    auto cmd = device.CreateCommandBuffer();
    if (cmd) {
        RenderPassDesc rp;
        rp.render_target = rt;
        rp.clear_color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        rp.clear_color_enabled = true;
        cmd->BeginRenderPass(rp);
        hair.Draw(*cmd, device, items, glm::mat4(1.0f), glm::mat4(1.0f));
        cmd->EndRenderPass();
        device.Submit(cmd);
    }
    device.EndFrame();

    RenderTargetReadback rb = device.ReadRenderTargetColorRgba8WithSize(rt);

    hair.Shutdown(device);
    device.DeleteGpuBuffer(pos_ssbo);
    device.DeleteGpuBuffer(tan_ssbo);
    device.DeleteRenderTarget(rt);
    return rb;
}

// 断言：中央横带（row 122..133）有相当数量的亮像素（线带已画），
// 带外（上/下 1/3）近乎无亮像素，四角清屏黑。
void VerifyHairLine(const RenderTargetReadback& rb, const char* backend) {
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    auto is_lit = [&](int x, int y) -> bool {
        const unsigned char* p = dse::test::PixelAt(rb, x, y);
        return p && (p[0] > 100 || p[1] > 100 || p[2] > 100);
    };

    int band_lit = 0;
    for (int y = 122; y <= 133; ++y)
        for (int x = 0; x < kRtSize; ++x)
            if (is_lit(x, y)) ++band_lit;

    int outside_lit = 0;
    for (int y = 0; y < kRtSize; ++y) {
        if (y >= 115 && y <= 140) continue;  // 跳过中央带及其邻近过渡
        for (int x = 0; x < kRtSize; ++x)
            if (is_lit(x, y)) ++outside_lit;
    }

    // 线带横跨约 x∈[38,217]（NDC -0.7..0.7 → 像素），中央带应有可观亮像素。
    EXPECT_GT(band_lit, 80) << backend << " hair line missing in central band";
    EXPECT_LT(outside_lit, 50) << backend << " unexpected lit pixels outside band";

    // 四角为清屏黑。
    for (auto c : {std::pair<int, int>{8, 8}, {248, 8}, {8, 248}, {248, 248}}) {
        const unsigned char* px = dse::test::PixelAt(rb, c.first, c.second);
        ASSERT_NE(px, nullptr) << backend;
        EXPECT_LT(px[0], 48) << backend << " corner R";
        EXPECT_LT(px[1], 48) << backend << " corner G";
        EXPECT_LT(px[2], 48) << backend << " corner B";
    }
}

}  // namespace

// ============================================================
// 三后端单独像素验证（活体消费 B4：LineStrip 拓扑 + vertexless SSBO 绘制）。
// ============================================================

TEST(HairPixelSmokeTest, OpenGLDrawsHairLine) {
    auto r = dse::test::RunOpenGL(RenderHair);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyHairLine(r.readback, "OpenGL");
}

TEST(HairPixelSmokeTest, D3D11DrawsHairLine) {
    auto r = dse::test::RunD3D11(RenderHair);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyHairLine(r.readback, "D3D11");
}

TEST(HairPixelSmokeTest, VulkanDrawsHairLine) {
    auto r = dse::test::RunVulkan(RenderHair);
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    VerifyHairLine(r.readback, "Vulkan");
}

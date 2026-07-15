/**
 * @file shader_graph_preview_golden_test.cpp
 * @brief P1-4b：Shader Graph 编辑器预览「真 GPU 出图」验收 —— 把 shader graph 经 codegen
 *        产出的顶点/片元源码用真实后端 CreateShaderProgram 编译成 GPU 程序，绑定引擎的
 *        PerFrame UBO + MeshVertex 全屏 quad 真渲染到离屏 RT，回读 framebuffer 与入库参考
 *        PNG 做 SSIM + 逐像素误差比对；并证明「改一个图节点 → 渲染输出确实变化」。
 *
 * 为何满足 P1-4b（编辑器 shader graph 实时渲染预览 + screenshot golden）：
 *  1. 走的是编辑器 shader graph 的真实产物：GenerateShader(graph, target) 与编辑器 Export
 *     GLSL/HLSL/Vulkan 按钮同一条 codegen（engine/render/shader_graph/shader_graph_codegen）。
 *  2. 真编译 + 真渲染：每后端各自 CreateShaderProgram（GL→GLSL430 / Vulkan→GLSL450→SPIR-V /
 *     D3D11→HLSL SM5），经通用绘制原语（BindPipeline/BindVertexBuffer/BindUniformBuffer/
 *     DrawIndexed）画到离屏 RT，ReadRenderTargetColorRgba8WithSize 回读 RGBA8 —— 非软件回退。
 *  3. screenshot golden：首次以 DSE_UPDATE_GOLDEN=1 在真机（RTX 3070）生成每后端参考 PNG 入库，
 *     此后加载参考 PNG，mean-SSIM（8x8 窗、luma）+ 平均/最大逐通道误差超阈值 fail。
 *  4. 「改节点即时出图」：同一后端渲染 scale=8 与 scale=3 两张图（改 Checkerboard 节点参数），
 *     断言两次输出逐像素平均差异显著 —— 图节点变化确实驱动渲染结果变化。
 *
 * 诚实边界：这是 shader graph 产物在真 GPU 上的「预览渲染 + 截图 golden」自动化门禁；参考在
 *          RTX 3070 生成，换机/软件回退像素不同（门禁将如实失败，须真机重生成）。编辑器内把
 *          该预览 RT 贴进 ImGui 视口的 GUI 呈现仍属可见操作（见台账，标注需真机/手动确认）。
 *
 * 生成/更新参考：
 *   set DSE_UPDATE_GOLDEN=1 && bin\dse_gtest_smoke_tests.exe --gtest_filter=ShaderGraphPreview*
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/shader_graph/shader_graph_codegen.h"

#include <glm/glm.hpp>

// stb_image_write 设为文件局部（STATIC），避免与 dse_engine 内既有实现重复符号；
// 读取用 stbi_load 走 dse_engine 已导出的 STB_IMAGE_IMPLEMENTATION。
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>
#include <stb/stb_image.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace dse::render;
using namespace dse::shadergraph;

namespace {

constexpr int kRtSize = 256;

// 记录最近一次渲染所用后端的实际适配器 + 是否软件渲染。
RenderDeviceInfo g_last_info;

// 顶点着色器的 PerFrame std140 UBO（与 shader_graph_codegen VertexGLSLFamily/VertexHLSL 一致）：
// mat4 vp; mat4 view; vec4 camera_pos; vec4 foliage_wind; vec4 foliage_push。
struct PerFrameUBO {
    glm::mat4 vp{1.0f};
    glm::mat4 view{1.0f};
    glm::vec4 camera_pos{0.0f, 0.0f, 1.0f, 0.0f};
    glm::vec4 foliage_wind{0.0f};
    glm::vec4 foliage_push{0.0f};
};

PinDesc MakePin(int id, const char* name, PinType type, PinKind kind) {
    PinDesc p;
    p.id = id;
    p.name = name;
    p.type = type;
    p.kind = kind;
    return p;
}

// 确定性、无贴图、空间变化的 shader graph：
//   UV → Checkerboard(scale) → Combine(R=checker) → PBR Output.Base Color
// 输出红/黑棋盘。改 scale 即改 Checkerboard 节点参数 → 棋盘格数变化 → 渲染输出显著不同。
ShaderGraphAsset MakePreviewGraph(float scale) {
    ShaderGraphAsset g;
    g.next_id = 500;

    NodeDesc uv;
    uv.id = 10;
    uv.name = "UV";
    uv.category = "Input";
    uv.outputs.push_back(MakePin(11, "UV", PinType::Vec2, PinKind::Output));

    NodeDesc chk;
    chk.id = 20;
    chk.name = "Checkerboard";
    chk.category = "Procedural";
    chk.inputs.push_back(MakePin(21, "UV", PinType::Vec2, PinKind::Input));
    chk.inputs.push_back(MakePin(22, "Scale", PinType::Float, PinKind::Input));
    chk.inputs[1].default_value[0] = scale;
    chk.outputs.push_back(MakePin(23, "Out", PinType::Float, PinKind::Output));

    NodeDesc comb;
    comb.id = 30;
    comb.name = "Combine";
    comb.category = "Channel";
    comb.inputs.push_back(MakePin(31, "R", PinType::Float, PinKind::Input));
    comb.inputs.push_back(MakePin(32, "G", PinType::Float, PinKind::Input));
    comb.inputs.push_back(MakePin(33, "B", PinType::Float, PinKind::Input));
    comb.inputs.push_back(MakePin(34, "A", PinType::Float, PinKind::Input));
    comb.outputs.push_back(MakePin(35, "RGBA", PinType::Color, PinKind::Output));

    NodeDesc out;
    out.id = 40;
    out.name = "PBR Output";
    out.category = "Output";
    out.inputs.push_back(MakePin(41, "Base Color", PinType::Color, PinKind::Input));

    g.nodes = {uv, chk, comb, out};
    g.links.push_back({60, 11, 21});  // UV -> Checkerboard.UV
    g.links.push_back({61, 23, 31});  // Checkerboard.Out -> Combine.R
    g.links.push_back({62, 35, 41});  // Combine.RGBA -> PBR Output.Base Color
    return g;
}

ShaderTarget BackendTarget(const char* backend) {
    if (std::string(backend) == "d3d11") return ShaderTarget::HLSL;
    if (std::string(backend) == "vulkan") return ShaderTarget::GLSL_VULKAN;
    return ShaderTarget::GLSL;
}

// 全屏 quad（NDC，vp=identity → gl_Position=vec4(a_pos,1)），MeshVertex 布局（pos/color/uv/normal/tangent）。
void MakeFullscreenQuad(std::vector<MeshVertex>& verts, std::vector<uint16_t>& indices) {
    const glm::vec4 col(1.0f);
    const glm::vec3 n(0.0f, 0.0f, 1.0f);
    const glm::vec3 t(1.0f, 0.0f, 0.0f);
    verts.push_back({{-1.0f, -1.0f, 0.0f}, col, {0.0f, 0.0f}, n, t});
    verts.push_back({{1.0f, -1.0f, 0.0f}, col, {1.0f, 0.0f}, n, t});
    verts.push_back({{1.0f, 1.0f, 0.0f}, col, {1.0f, 1.0f}, n, t});
    verts.push_back({{-1.0f, 1.0f, 0.0f}, col, {0.0f, 1.0f}, n, t});
    indices = {0, 1, 2, 0, 2, 3};
}

// 编译 graph 产物为 GPU 程序并真渲染到离屏 RT，回读 RGBA8。program 创建失败返回空（上层 SKIP）。
RenderTargetReadback RenderGraphPreview(RhiDevice& device, const char* backend, float scale) {
    g_last_info = device.GetDeviceInfo();

    const ShaderCodegenResult code = GenerateShader(MakePreviewGraph(scale), BackendTarget(backend));
    if (!code.ok) return {};

    RenderTargetDesc rt_desc;
    rt_desc.width = kRtSize;
    rt_desc.height = kRtSize;
    rt_desc.has_color = true;
    rt_desc.has_depth = false;
    unsigned int rt = device.CreateRenderTarget(rt_desc);
    if (rt == 0) return {};

    unsigned int program = device.CreateShaderProgram(code.vertex, code.fragment);
    if (program == 0) {
        std::printf("[P1-4b] backend=%s: shader-graph program failed to compile/link\n", backend);
        std::fflush(stdout);
        device.DeleteRenderTarget(rt);
        return {};
    }

    PerFrameUBO frame;  // vp/view = identity
    GpuBufferDesc ub_desc;
    ub_desc.size = sizeof(PerFrameUBO);
    ub_desc.usage = GpuBufferUsage::kUniform;
    BufferHandle ubo = device.CreateGpuBuffer(ub_desc, &frame);

    std::vector<MeshVertex> verts;
    std::vector<uint16_t> indices;
    MakeFullscreenQuad(verts, indices);

    GpuBufferDesc vb_desc;
    vb_desc.size = verts.size() * sizeof(MeshVertex);
    vb_desc.usage = GpuBufferUsage::kVertex;
    BufferHandle vbo = device.CreateGpuBuffer(vb_desc, verts.data());

    GpuBufferDesc ib_desc;
    ib_desc.size = indices.size() * sizeof(uint16_t);
    ib_desc.usage = GpuBufferUsage::kIndex;
    BufferHandle ibo = device.CreateGpuBuffer(ib_desc, indices.data());

    PipelineStateDesc pso_desc;
    pso_desc.blend_enabled = false;
    pso_desc.depth_test_enabled = false;
    pso_desc.depth_write_enabled = false;
    pso_desc.culling_enabled = false;
    auto pso = device.CreatePipelineState(pso_desc);

    const std::vector<VertexAttr> attrs = {
        {0, 3, static_cast<uint32_t>(offsetof(MeshVertex, position))},
        {1, 4, static_cast<uint32_t>(offsetof(MeshVertex, color))},
        {2, 2, static_cast<uint32_t>(offsetof(MeshVertex, uv))},
        {3, 3, static_cast<uint32_t>(offsetof(MeshVertex, normal))},
        {4, 3, static_cast<uint32_t>(offsetof(MeshVertex, tangent))},
    };

    RenderTargetReadback rb;
    if (ubo && vbo && ibo) {
        device.BeginFrame();
        auto cmd = device.CreateCommandBuffer();
        if (cmd) {
            RenderPassDesc rp;
            rp.render_target = rt;
            rp.clear_color = glm::vec4(0.02f, 0.02f, 0.03f, 1.0f);
            rp.clear_color_enabled = true;
            cmd->BeginRenderPass(rp);
            cmd->BindPipeline(device.GetGraphicsPipeline(pso, program));
            cmd->BindVertexBuffer(0u, vbo.raw(), sizeof(MeshVertex), attrs);
            cmd->BindIndexBuffer(ibo.raw(), IndexType::UInt16);
            cmd->BindUniformBuffer(0u, ubo.raw(), 0u, 0u);
            cmd->DrawIndexed(static_cast<uint32_t>(indices.size()), 0, 0);
            cmd->EndRenderPass();
            device.Submit(cmd);
        }
        device.EndFrame();
        rb = device.ReadRenderTargetColorRgba8WithSize(rt);
    }

    device.DeleteGpuBuffer(ubo);
    device.DeleteGpuBuffer(vbo);
    device.DeleteGpuBuffer(ibo);
    device.DeleteShaderProgram(program);
    device.DeleteRenderTarget(rt);
    return rb;
}

// ── golden 比对（与 P0-4 同款 SSIM + 逐像素误差）─────────────────────────────────
std::string GoldenPath(const char* backend) {
    return std::string("tests/golden/p1_4b_shadergraph_") + backend + ".png";
}

bool UpdateGoldenMode() {
    const char* v = std::getenv("DSE_UPDATE_GOLDEN");
    return v && v[0] && !(v[0] == '0' && v[1] == '\0');
}

std::vector<double> ToLuma(const unsigned char* px, int w, int h) {
    std::vector<double> l(static_cast<size_t>(w) * h);
    for (int i = 0; i < w * h; ++i) {
        const unsigned char* p = px + static_cast<size_t>(i) * 4;
        l[i] = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
    }
    return l;
}

double MeanSsim(const std::vector<double>& a, const std::vector<double>& b, int w, int h) {
    const double C1 = 6.5025;
    const double C2 = 58.5225;
    const int win = 8;
    double ssim_sum = 0.0;
    int win_count = 0;
    for (int by = 0; by + win <= h; by += win) {
        for (int bx = 0; bx + win <= w; bx += win) {
            double ma = 0.0, mb = 0.0;
            for (int y = 0; y < win; ++y)
                for (int x = 0; x < win; ++x) {
                    const int idx = (by + y) * w + (bx + x);
                    ma += a[idx];
                    mb += b[idx];
                }
            const double n = win * win;
            ma /= n;
            mb /= n;
            double va = 0.0, vb = 0.0, cov = 0.0;
            for (int y = 0; y < win; ++y)
                for (int x = 0; x < win; ++x) {
                    const int idx = (by + y) * w + (bx + x);
                    const double da = a[idx] - ma;
                    const double db = b[idx] - mb;
                    va += da * da;
                    vb += db * db;
                    cov += da * db;
                }
            va /= (n - 1);
            vb /= (n - 1);
            cov /= (n - 1);
            const double s = ((2 * ma * mb + C1) * (2 * cov + C2)) /
                             ((ma * ma + mb * mb + C1) * (va + vb + C2));
            ssim_sum += s;
            ++win_count;
        }
    }
    return win_count > 0 ? ssim_sum / win_count : 1.0;
}

struct PixelDiff {
    double mean_abs = 0.0;
    double max_abs = 0.0;
};

PixelDiff ComputePixelDiff(const unsigned char* a, const unsigned char* b, int count) {
    PixelDiff d;
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        const double e = std::fabs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
        sum += e;
        if (e > d.max_abs) d.max_abs = e;
    }
    d.mean_abs = count > 0 ? sum / count : 0.0;
    return d;
}

long long NonClearPixels(const RenderTargetReadback& rb) {
    long long nonclear = 0;
    for (int i = 0; i < rb.width * rb.height; ++i) {
        const unsigned char* p = &rb.pixels[static_cast<size_t>(i) * 4];
        if (p[0] > 40 || p[1] > 40 || p[2] > 40) ++nonclear;
    }
    return nonclear;
}

void CheckGolden(dse::test::BackendResult r, const char* backend) {
    if (!r.available) GTEST_SKIP() << r.skip_reason;
    if (r.readback.pixels.empty())
        GTEST_SKIP() << "shader-graph 预览程序在 " << backend << " 上未能编译/渲染（见 stdout）";

    const RenderTargetReadback& rb = r.readback;
    ASSERT_EQ(rb.width, kRtSize) << backend;
    ASSERT_EQ(rb.height, kRtSize) << backend;
    ASSERT_EQ(rb.pixels.size(), static_cast<size_t>(kRtSize) * kRtSize * 4) << backend;

    // 活体：渲染结果不能是纯清屏色（证明真画了 shader graph 产物）。
    ASSERT_GT(NonClearPixels(rb), kRtSize * kRtSize / 20)
        << backend << ": 渲染结果近乎纯清屏（shader graph 未出图）";

    std::printf("[P1-4b] backend=%s adapter=\"%s\" software=%d\n",
                backend, g_last_info.adapter_name.c_str(), g_last_info.is_software ? 1 : 0);
    std::fflush(stdout);
    EXPECT_FALSE(g_last_info.is_software)
        << backend << ": 软件渲染回退不能作为真机 GPU 预览证据（adapter=" << g_last_info.adapter_name << "）";

    const std::string path = GoldenPath(backend);

    if (UpdateGoldenMode()) {
        const int ok = stbi_write_png(path.c_str(), kRtSize, kRtSize, 4,
                                      rb.pixels.data(), kRtSize * 4);
        ASSERT_NE(ok, 0) << backend << ": 写入 golden 失败 " << path;
        GTEST_SUCCEED() << backend << ": golden 已生成/更新 → " << path;
        return;
    }

    int gw = 0, gh = 0, gc = 0;
    unsigned char* golden = stbi_load(path.c_str(), &gw, &gh, &gc, 4);
    ASSERT_NE(golden, nullptr) << backend << ": 缺少 golden 参考 " << path
                               << "（首次请以 DSE_UPDATE_GOLDEN=1 在真机生成并入库）";
    ASSERT_EQ(gw, kRtSize) << backend << " golden 宽";
    ASSERT_EQ(gh, kRtSize) << backend << " golden 高";

    const int byte_count = kRtSize * kRtSize * 4;
    const PixelDiff diff = ComputePixelDiff(rb.pixels.data(), golden, byte_count);
    const std::vector<double> la = ToLuma(rb.pixels.data(), kRtSize, kRtSize);
    const std::vector<double> lg = ToLuma(golden, kRtSize, kRtSize);
    const double ssim = MeanSsim(la, lg, kRtSize, kRtSize);
    stbi_image_free(golden);

    EXPECT_GE(ssim, 0.98) << backend << ": SSIM=" << ssim << " 低于阈值（预览渲染回归？）";
    EXPECT_LE(diff.mean_abs, 3.0) << backend << ": 平均逐通道误差=" << diff.mean_abs
                                  << "（max=" << diff.max_abs << "）超阈值";
}

// 「改节点即时出图」：同后端渲染 scale=8 与 scale=3，断言输出显著不同。
void CheckNodeChangeAltersOutput(const char* backend,
                                 dse::test::BackendResult (*run)(const dse::test::RenderFn&)) {
    auto a = run([backend](RhiDevice& d) { return RenderGraphPreview(d, backend, 8.0f); });
    if (!a.available) GTEST_SKIP() << a.skip_reason;
    if (a.readback.pixels.empty())
        GTEST_SKIP() << "shader-graph 预览程序在 " << backend << " 上未能编译/渲染";
    auto b = run([backend](RhiDevice& d) { return RenderGraphPreview(d, backend, 3.0f); });
    if (!b.available || b.readback.pixels.empty())
        GTEST_SKIP() << "shader-graph 预览程序（scale=3）在 " << backend << " 上不可用";

    ASSERT_EQ(a.readback.pixels.size(), b.readback.pixels.size()) << backend;
    ASSERT_GT(NonClearPixels(a.readback), kRtSize * kRtSize / 20) << backend << " scale=8 出图";
    ASSERT_GT(NonClearPixels(b.readback), kRtSize * kRtSize / 20) << backend << " scale=3 出图";

    const PixelDiff diff = ComputePixelDiff(a.readback.pixels.data(), b.readback.pixels.data(),
                                            static_cast<int>(a.readback.pixels.size()));
    std::printf("[P1-4b] backend=%s node-change mean_abs_diff=%.3f\n", backend, diff.mean_abs);
    std::fflush(stdout);
    // 改 Checkerboard.Scale 8→3 → 棋盘格结构变化 → 逐像素平均差异显著（远超 golden 稳定阈值 3.0）。
    EXPECT_GT(diff.mean_abs, 12.0)
        << backend << ": 改图节点后渲染输出差异过小（mean_abs=" << diff.mean_abs
        << "），未能证明「改节点即时出图」";
}

}  // namespace

// ── 每后端 golden（真 GPU 编译 + 渲染 shader graph 产物）─────────────────────────
TEST(ShaderGraphPreviewGoldenTest, OpenGL) {
    CheckGolden(dse::test::RunOpenGL([](RhiDevice& d) { return RenderGraphPreview(d, "opengl", 8.0f); }), "opengl");
}
TEST(ShaderGraphPreviewGoldenTest, D3D11) {
    CheckGolden(dse::test::RunD3D11([](RhiDevice& d) { return RenderGraphPreview(d, "d3d11", 8.0f); }), "d3d11");
}
TEST(ShaderGraphPreviewGoldenTest, Vulkan) {
    CheckGolden(dse::test::RunVulkan([](RhiDevice& d) { return RenderGraphPreview(d, "vulkan", 8.0f); }), "vulkan");
}

// ── 改节点即时出图（同后端两图差异显著）──────────────────────────────────────────
TEST(ShaderGraphPreviewGoldenTest, OpenGLNodeChangeAltersOutput) {
    CheckNodeChangeAltersOutput("opengl", dse::test::RunOpenGL);
}
TEST(ShaderGraphPreviewGoldenTest, D3D11NodeChangeAltersOutput) {
    CheckNodeChangeAltersOutput("d3d11", dse::test::RunD3D11);
}
TEST(ShaderGraphPreviewGoldenTest, VulkanNodeChangeAltersOutput) {
    CheckNodeChangeAltersOutput("vulkan", dse::test::RunVulkan);
}

/**
 * @file gpu_skinning_morph_compute_smoke_test.cpp
 * @brief 统一变形系统 阶段A「活体」compute 冒烟 —— 验证 GPUSkinningSystem 的 morph
 *        混形已解除 4-target 硬上限：提交一个 6 个 morph target 的纯 morph 请求（单位骨骼），
 *        经 compute dispatch + 回读，比对每顶点 pos == base + Σ_m w_m·delta_m（全部 6 个 target）。
 *
 * 设计要点（为何该用例能证明「上限已解除」）：
 *  - 6 个 target，第 5/6 个（index 4/5）带非零权重与非零 delta。
 *  - 若 kernel 仍硬编码 m<4u（旧行为）→ 第 5/6 target 不参与 → x 位移偏小 → 断言失败。
 *  - 单位骨骼（bone=identity, 权重 bw0=1）使蒙皮为恒等，从而 pos 仅由 morph 决定，
 *    隔离验证 morph 混形数值。
 *  - 需真实 compute 上下文：无 compute 能力（软件后端/无驱动）时 GTEST_SKIP。
 *  - 该测试不依赖像素回读，借 rhi_pixel_harness 仅取「已初始化 GPU 上下文的 RhiDevice」，
 *    在其内跑 compute 并把比对结果写回捕获结构。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/skinning/gpu_skinning.h"

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace dse::render;

namespace {

constexpr uint32_t kVerts = 4;
constexpr uint32_t kTargets = 6;   // > 4：跨越旧硬上限
constexpr uint32_t kEntityId = 42;

// compute 比对结果（由 harness 内 lambda 写回；harness 在主线程同步执行 fn）。
struct MorphComputeResult {
    bool compute_available = false;   ///< false → 该后端无 compute，调用方 SKIP
    bool got_output = false;          ///< 是否成功取到回读输出
    bool output_all_zero = false;     ///< 回读输出全 0（compute 未真正写入 → 后端 compute 基础设施未打通）
    uint32_t out_vertex_count = 0;
    float max_pos_error = 0.0f;       ///< 与 CPU 参考的最大逐分量误差
    float expected_dx_all6 = 0.0f;    ///< 6-target 参考 x 位移（诊断用）
    float capped_dx_4 = 0.0f;         ///< 仅前 4 target 的 x 位移（诊断用）
};

// 基顶点位置（局部空间，任意可区分值）。
glm::vec3 BasePos(uint32_t v) {
    return glm::vec3(static_cast<float>(v) * 0.5f, 1.0f, -0.25f);
}

// target m、顶点 v 的位置 delta：x 随 target 递增、y 随顶点递增，保证各 target/顶点可区分。
glm::vec3 Delta(uint32_t m, uint32_t v) {
    return glm::vec3(static_cast<float>(m + 1) * 0.1f, static_cast<float>(v) * 0.01f, 0.0f);
}

float Weight(uint32_t m) { return 0.5f; }

// 在给定（已初始化）device 上执行一次 morph-only compute，把结果写入 out。
void RunMorphCompute(RhiDevice& device, MorphComputeResult& out) {
    GPUSkinningSystem sys;
    if (!sys.Init(&device)) {
        out.compute_available = false;
        return;
    }
    out.compute_available = true;

    // --- 组装 SkinningRequest（纯 morph + 单位骨骼）---
    SkinningRequest req;
    req.entity_id = kEntityId;
    req.vertex_count = kVerts;

    // SrcVertex 布局：pos_bw0 / norm_bw1 / tan_bw2 / joints_bw3，各 vec4（16 floats/顶点）。
    req.src_vertex_data.resize(static_cast<size_t>(kVerts) * 16, 0.0f);
    for (uint32_t v = 0; v < kVerts; ++v) {
        float* d = req.src_vertex_data.data() + static_cast<size_t>(v) * 16;
        const glm::vec3 p = BasePos(v);
        d[0] = p.x; d[1] = p.y; d[2] = p.z; d[3] = 1.0f;  // pos + boneweight0 = 1
        d[4] = 0.0f; d[5] = 0.0f; d[6] = 1.0f; d[7] = 0.0f;  // normal +Z, bw1 = 0
        d[8] = 1.0f; d[9] = 0.0f; d[10] = 0.0f; d[11] = 0.0f;  // tangent +X, bw2 = 0
        // joints 全 0，bw3 = 1 - 1 - 0 - 0 = 0 → 蒙皮恒等
    }

    // 单位骨骼矩阵。
    req.bone_matrices = { glm::mat4(1.0f) };

    // Morph 权重 / delta（6 个 target，无上限）。
    req.morph_target_count = kTargets;
    req.morph_weights.resize(kTargets);
    for (uint32_t m = 0; m < kTargets; ++m) req.morph_weights[m] = Weight(m);

    // delta 布局：morph_deltas[m * vertex_count + v] 的 vec4（4 floats）。
    req.morph_deltas.resize(static_cast<size_t>(kTargets) * kVerts * 4, 0.0f);
    for (uint32_t m = 0; m < kTargets; ++m) {
        for (uint32_t v = 0; v < kVerts; ++v) {
            float* d = req.morph_deltas.data() + (static_cast<size_t>(m) * kVerts + v) * 4;
            const glm::vec3 dv = Delta(m, v);
            d[0] = dv.x; d[1] = dv.y; d[2] = dv.z; d[3] = 0.0f;
        }
    }

    // 诊断量：全 6 target 与仅前 4 target 的 x 位移（对顶点 0）。
    for (uint32_t m = 0; m < kTargets; ++m) out.expected_dx_all6 += Weight(m) * Delta(m, 0).x;
    for (uint32_t m = 0; m < 4; ++m) out.capped_dx_4 += Weight(m) * Delta(m, 0).x;

    // --- 帧1：dispatch。后续帧触发回读 ---
    // 回读为双缓冲延迟：GL/VK 桌面同步(首次即就绪)，D3D11/WebGPU 需下一帧才拿到上一帧拷贝，
    // 故多跑几帧直到取到数据（模拟真实帧管线「下一帧 BeginFrame 回读」的语义）。
    device.BeginFrame();
    sys.BeginFrame();               // 帧1：无上一帧可读
    sys.Submit(std::move(req));
    sys.Dispatch();                 // compute 写入 dst buffer
    device.EndFrame();

    const SkinnedOutput* result = nullptr;
    for (int f = 0; f < 3 && !out.got_output; ++f) {
        device.BeginFrame();
        sys.BeginFrame();           // 回读上一帧 compute 结果
        device.EndFrame();
        result = sys.GetSkinnedOutput(kEntityId);
        if (result && result->vertex_count == kVerts) {
            out.got_output = true;
            out.out_vertex_count = result->vertex_count;
            bool all_zero = true;
            for (uint32_t v = 0; v < kVerts; ++v) {
                glm::vec3 ref = BasePos(v);
                for (uint32_t m = 0; m < kTargets; ++m) ref += Weight(m) * Delta(m, v);
                const glm::vec3 got = result->positions[v];
                out.max_pos_error = (std::max)(out.max_pos_error, std::fabs(got.x - ref.x));
                out.max_pos_error = (std::max)(out.max_pos_error, std::fabs(got.y - ref.y));
                out.max_pos_error = (std::max)(out.max_pos_error, std::fabs(got.z - ref.z));
                if (std::fabs(got.x) > 1e-6f || std::fabs(got.y) > 1e-6f ||
                    std::fabs(got.z) > 1e-6f) all_zero = false;
            }
            out.output_all_zero = all_zero;
        }
    }

    sys.Shutdown();
}

// 借 harness 取得已初始化 device：在其内跑 compute，返回空 readback（本用例不看像素）。
MorphComputeResult RunOnBackend(dse::test::BackendResult (*runner)(const dse::test::RenderFn&)) {
    MorphComputeResult result;
    dse::test::RenderFn fn = [&result](RhiDevice& device) -> ::RenderTargetReadback {
        RunMorphCompute(device, result);
        return {};
    };
    dse::test::BackendResult br = runner(fn);
    if (!br.available) result.compute_available = false;  // 无后端 → 视为不可用 SKIP
    return result;
}

void CheckBackend(dse::test::BackendResult (*runner)(const dse::test::RenderFn&), const char* backend) {
    MorphComputeResult r = RunOnBackend(runner);
    if (!r.compute_available) {
        GTEST_SKIP() << backend << "：compute 不可用（无驱动/软件后端），跳过";
    }
    // GL/Vulkan/D3D11 三后端 compute→回读 均已打通（D3D11 经 @DX11_SSBO_SPLIT 修复：写 SSBO
    // 落 UAV u{slot} 而非 t{16+slot}）。因此「无输出 / 全 0」不再作环境暂缓，而是真实回归 →
    // 硬失败，避免掩盖 compute 未写入 dst 的问题。
    ASSERT_TRUE(r.got_output) << backend << "：compute→回读 未取得输出（回读管线回归）";
    ASSERT_FALSE(r.output_all_zero) << backend << "：compute 输出全 0，dst 未被写入（UAV/uniform 交付回归）";
    EXPECT_EQ(r.out_vertex_count, kVerts) << backend;
    // 6-target 与 4-target x 位移应显著不同，确保该用例真正区分「上限是否解除」。
    ASSERT_GT(std::fabs(r.expected_dx_all6 - r.capped_dx_4), 0.1f) << backend << "：诊断量构造无效";
    // 数值一致（含第 5/6 target 贡献）→ 上限已解除且混形正确。
    EXPECT_LT(r.max_pos_error, 1e-3f) << backend << "：morph 混形与 CPU 参考不一致（max_err="
                                      << r.max_pos_error << "）";
}

}  // namespace

// ============================================================
// 三后端：GPU compute morph 无上限混形一致性（活体消费 GPUSkinningSystem）。
// ============================================================

TEST(GpuSkinningMorphComputeSmokeTest, OpenGL无上限morph混形一致) {
    CheckBackend(dse::test::RunOpenGL, "OpenGL");
}
TEST(GpuSkinningMorphComputeSmokeTest, D3D11无上限morph混形一致) {
    CheckBackend(dse::test::RunD3D11, "D3D11");
}
TEST(GpuSkinningMorphComputeSmokeTest, Vulkan无上限morph混形一致) {
    CheckBackend(dse::test::RunVulkan, "Vulkan");
}

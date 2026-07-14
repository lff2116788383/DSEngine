/**
 * @file danim_gpu_skinning_frame_consistency_test.cpp
 * @brief P1-5：真实 .danim 资产驱动骨骼动画 → GPU skinning 逐帧与 CPU 参考数值一致。
 *
 * 证据链（为何该用例满足 P1-5 验收）：
 *  1. 真实资产：AssetManager 从仓库 data/ 加载真实 `two_bone.dskel` + `two_bone_idle_walk.danim`
 *     + `two_bone.dmesh`（顶点/法线/切线/4 骨索引/4 权重直取自 .dmesh，非合成、非单位骨骼）。
 *  2. 真实动画求值：用引擎实际的 gameplay3d::AnimatorSystem::Update（关键帧采样 + 层级传播 +
 *     inverse-bind → final_bone_matrices 调色板），在若干确定性时间点采样，得到逐帧 GPU 调色板。
 *  3. 真实 GPU skinning：把「同一顶点 + 同一动画调色板」提交给引擎实际的 GPUSkinningSystem
 *     compute（与 frame_pipeline_render.cpp 运行时提交布局逐字节一致），dispatch 后回读 GPU 位置。
 *  4. CPU 参考：镜像 skinning.comp / frame_pipeline_render.cpp 的 skin_like_compute 线性混合蒙皮
 *     （bw3 = 1 - bw0 - bw1 - bw2），逐帧逐顶点比对，阈值内一致。
 *  5. 活体校验：断言动画调色板逐帧确实变化（.danim 真在驱动运动），且蒙皮确实位移顶点
 *     （调色板非恒等作用于顶点）——排除「静态/恒等」的空洞证据。
 *  6. 三后端（OpenGL / D3D11 / Vulkan）各跑一遍；无 compute 能力（软件后端/无驱动）时 SKIP。
 *
 * 注：动画采样为纯 CPU、与 device 无关，故先一次性采样出逐帧调色板与 CPU 参考位置，再在各后端
 *     harness 内做 GPU compute + 回读比对，保证三后端消费完全相同的真实动画数据。
 */

#include <gtest/gtest.h>

#include "rhi_pixel_harness.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/skinning/gpu_skinning.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef DSE_ENABLE_3D

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/assets/asset_manager.h"
#include "modules/gameplay_3d/animation/animator_system.h"

using namespace dse::render;

namespace {

constexpr uint32_t kFrames = 6;                 // 逐帧采样点数
constexpr uint32_t kEntityId = 777;
const char* kDskel = "animation/minimal_rig/two_bone.dskel";
const char* kDanim = "animation/minimal_rig/two_bone_idle_walk.danim";
const char* kDmesh = "animation/minimal_rig/two_bone.dmesh";

// 从真实 .dmesh 解码出的一个顶点（4 骨索引 + 4 权重，与运行时 RuntimeVertex 布局一致）。
struct SkinVertex {
    glm::vec3 pos;              // 绑定空间位置
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec4 weights{0.0f};    // 4 权重（存储前 3，bw3 派生）
    glm::ivec4 joints{0};       // 4 骨索引
};

// .dmesh 运行时布局镜像（见 engine/assets/compiler/raw_scene_data.h + importer.cpp::RuntimeVertex）。
#pragma pack(push, 1)
struct DmeshHeaderL {
    char magic[4];
    uint32_t version;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t submesh_count;
    uint32_t attribute_mask;
    uint64_t vertex_data_offset;
    uint64_t index_data_offset;
    uint64_t submesh_data_offset;
};
#pragma pack(pop)

// RuntimeVertex 字段字节偏移（自然对齐、无填充；4 字节成员）：
//   position@0 normal@12 texcoord@24 weights@32 joints@48 tangent@64 [color@80]
// v1 步长 80B（20 floats），v2 步长 96B（含 color）。
bool DecodeDmesh(const std::vector<uint8_t>& data, std::vector<SkinVertex>& out) {
    if (data.size() < sizeof(DmeshHeaderL)) return false;
    DmeshHeaderL h{};
    std::memcpy(&h, data.data(), sizeof(h));
    if (h.magic[0] != 'D' || h.magic[1] != 'S' || h.magic[2] != 'E' || h.magic[3] != 'M') return false;
    if (h.vertex_count == 0) return false;
    const size_t stride = (h.version >= 2) ? 96 : 80;
    const size_t need = static_cast<size_t>(h.vertex_data_offset) +
                        static_cast<size_t>(h.vertex_count) * stride;
    if (data.size() < need) return false;

    out.clear();
    out.reserve(h.vertex_count);
    const uint8_t* vbase = data.data() + h.vertex_data_offset;
    for (uint32_t i = 0; i < h.vertex_count; ++i) {
        const uint8_t* rec = vbase + static_cast<size_t>(i) * stride;
        SkinVertex v;
        std::memcpy(&v.pos, rec + 0, sizeof(glm::vec3));
        std::memcpy(&v.normal, rec + 12, sizeof(glm::vec3));
        std::memcpy(&v.weights, rec + 32, sizeof(glm::vec4));
        std::memcpy(&v.joints, rec + 48, sizeof(glm::ivec4));
        glm::vec4 tan4(1.0f, 0.0f, 0.0f, 1.0f);
        std::memcpy(&tan4, rec + 64, sizeof(glm::vec4));
        v.tangent = glm::vec3(tan4);
        out.push_back(v);
    }
    return true;
}

// 一次性从真实资产采样出的动画数据（device 无关）。
struct AnimSample {
    bool loaded = false;
    uint32_t bone_count = 0;
    float duration = 0.0f;
    std::vector<SkinVertex> verts;
    std::vector<std::vector<glm::mat4>> palettes;  // [frame][bone] 真实动画调色板
    std::vector<std::vector<glm::vec3>> cpu_pos;   // [frame][vertex] CPU 参考蒙皮位置
    float pose_motion = 0.0f;      // 调色板逐帧最大变化（证明动画是活体）
    float skin_displacement = 0.0f; // 蒙皮后位置相对绑定位置最大位移（证明调色板真作用）
};

// 镜像 skinning.comp / skin_like_compute：sm = Σ bone[ji]*bwi（bw3 = 1-bw0-bw1-bw2），pos = sm·(p,1)。
glm::vec3 CpuSkin(const SkinVertex& v, const std::vector<glm::mat4>& bones) {
    const float bw0 = v.weights.x, bw1 = v.weights.y, bw2 = v.weights.z;
    const float bw3 = 1.0f - bw0 - bw1 - bw2;
    const int n = static_cast<int>(bones.size());
    auto B = [&](int i) -> glm::mat4 { return (i >= 0 && i < n) ? bones[i] : glm::mat4(1.0f); };
    const glm::mat4 sm = B(v.joints.x) * bw0 + B(v.joints.y) * bw1
                       + B(v.joints.z) * bw2 + B(v.joints.w) * bw3;
    return glm::vec3(sm * glm::vec4(v.pos, 1.0f));
}

// 一次性用真实 AnimatorSystem 采样逐帧调色板 + 构造绑定到真实骨骼的顶点 + CPU 参考。
AnimSample BuildAnimSample() {
    using namespace dse;
    AnimSample s;

    AssetManager mgr;
    mgr.ConfigureDataRoot("data");   // ctest WORKING_DIRECTORY = CMAKE_SOURCE_DIR
    gameplay3d::AnimatorSystem::SetAssetManager(&mgr);

    World world;
    auto entity = world.CreateEntity();
    auto& anim = world.registry().emplace<Animator3DComponent>(entity);
    anim.enabled = true;
    anim.dskel_path = kDskel;
    anim.danim_path = kDanim;
    anim.speed = 1.0f;
    anim.loop = true;
    anim.use_anim_tree = false;

    // 首帧：构建骨骼缓存 + 取骨骼数/时长。
    anim.current_time = 0.0f;
    gameplay3d::AnimatorSystem::Update(world, 0.0f);
    s.bone_count = anim.skel_cache.bone_count;
    s.duration = anim.cached_duration_;
    if (s.bone_count == 0 || anim.final_bone_matrices.empty()) {
        gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
        return s;  // loaded=false → 资产缺失/解析失败
    }

    // 加载 + 解码真实 .dmesh（两骨蒙皮网格）：位置/法线/切线/4 骨索引/4 权重直取自资产。
    auto dmesh = mgr.LoadDmesh(kDmesh);
    if (!dmesh || !DecodeDmesh(dmesh->GetData(), s.verts) || s.verts.empty()) {
        gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
        return s;  // loaded=false → .dmesh 缺失/解析失败
    }
    // 真实 .dmesh 的骨索引必须落在骨骼调色板范围内（否则 GPU 读越界 → 无效证据）。
    const int bc = static_cast<int>(s.bone_count);
    for (const SkinVertex& v : s.verts) {
        for (int k = 0; k < 4; ++k) {
            if (v.joints[k] < 0 || v.joints[k] >= bc) {
                gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
                return s;  // loaded=false → 资产骨索引越界
            }
        }
    }

    // 逐帧采样真实动画调色板 + CPU 参考位置。
    const float span = (s.duration > 1e-4f) ? s.duration : 1.0f;
    s.palettes.resize(kFrames);
    s.cpu_pos.resize(kFrames);
    for (uint32_t f = 0; f < kFrames; ++f) {
        const float frac = static_cast<float>(f) / static_cast<float>(kFrames);  // 0..(1-1/N)
        anim.current_time = frac * span;
        gameplay3d::AnimatorSystem::Update(world, 0.0f);
        s.palettes[f].assign(anim.final_bone_matrices.begin(),
                             anim.final_bone_matrices.begin() + s.bone_count);
        s.cpu_pos[f].resize(s.verts.size());
        for (size_t vi = 0; vi < s.verts.size(); ++vi) {
            s.cpu_pos[f][vi] = CpuSkin(s.verts[vi], s.palettes[f]);
            const float d = glm::length(s.cpu_pos[f][vi] - s.verts[vi].pos);
            s.skin_displacement = (std::max)(s.skin_displacement, d);
        }
    }

    // 动画活体：调色板逐帧变化。
    for (uint32_t f = 1; f < kFrames; ++f) {
        for (uint32_t bi = 0; bi < s.bone_count; ++bi) {
            const glm::mat4& m0 = s.palettes[0][bi];
            const glm::mat4& mf = s.palettes[f][bi];
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    s.pose_motion = (std::max)(s.pose_motion, std::fabs(mf[c][r] - m0[c][r]));
        }
    }

    gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
    s.loaded = true;
    return s;
}

// GPU 侧逐帧比对结果。
struct GpuConsistencyResult {
    bool compute_available = false;
    uint32_t frames_with_output = 0;
    float max_pos_error = 0.0f;
    bool any_all_zero = false;
};

// 用某一帧的调色板构造与运行时逐字节一致的 SkinningRequest（model=identity → 调色板直接为骨骼矩阵）。
SkinningRequest BuildRequest(const AnimSample& s, uint32_t frame) {
    const uint32_t vcount = static_cast<uint32_t>(s.verts.size());
    SkinningRequest req;
    req.entity_id = kEntityId;
    req.vertex_count = vcount;
    req.bone_matrices = s.palettes[frame];
    req.src_vertex_data.resize(static_cast<size_t>(vcount) * 16, 0.0f);
    for (uint32_t i = 0; i < vcount; ++i) {
        const SkinVertex& v = s.verts[i];
        float* d = req.src_vertex_data.data() + static_cast<size_t>(i) * 16;
        d[0]  = v.pos.x;     d[1]  = v.pos.y;     d[2]  = v.pos.z;     d[3]  = v.weights.x;
        d[4]  = v.normal.x;  d[5]  = v.normal.y;  d[6]  = v.normal.z;  d[7]  = v.weights.y;
        d[8]  = v.tangent.x; d[9]  = v.tangent.y; d[10] = v.tangent.z; d[11] = v.weights.z;
        d[12] = static_cast<float>(v.joints.x); d[13] = static_cast<float>(v.joints.y);
        d[14] = static_cast<float>(v.joints.z); d[15] = static_cast<float>(v.joints.w);
    }
    return req;
}

// 在给定（已初始化）device 上取某一帧调色板的 GPU 蒙皮回读。
// GPUSkinningSystem 双缓冲、回读延迟依后端而异（BeginFrame→ReadBackPrevFrame 读的是"上一次
// Dispatch"的结果）；为消除后端相关的回读时序差，反复用【同一】真实调色板 dispatch 若干次，
// 使回读稳定收敛到该帧结果后再取（数据仍为真实动画帧 palettes[frame]，不掺假）。
bool GpuSkinFrame(RhiDevice& device, GPUSkinningSystem& sys,
                  const AnimSample& s, uint32_t frame,
                  std::vector<glm::vec3>& out_pos) {
    const uint32_t vcount = static_cast<uint32_t>(s.verts.size());
    bool got = false;
    constexpr int kSettleIters = 6;
    for (int iter = 0; iter <= kSettleIters; ++iter) {
        device.BeginFrame();
        sys.BeginFrame();  // ReadBackPrevFrame：反映上一次 Dispatch（迭代稳定后即为本帧）
        const SkinnedOutput* r = sys.GetSkinnedOutput(kEntityId);
        if (r && r->vertex_count == vcount) {
            out_pos.assign(r->positions.begin(), r->positions.end());
            got = true;
        }
        if (iter < kSettleIters) {
            SkinningRequest req = BuildRequest(s, frame);
            sys.Submit(std::move(req));
            sys.Dispatch();
        }
        device.EndFrame();
    }
    return got;
}

void RunGpuConsistency(RhiDevice& device, const AnimSample& s, GpuConsistencyResult& out) {
    GPUSkinningSystem sys;
    if (!sys.Init(&device)) { out.compute_available = false; return; }
    out.compute_available = true;

    for (uint32_t f = 0; f < kFrames; ++f) {
        std::vector<glm::vec3> gpu_pos;
        if (!GpuSkinFrame(device, sys, s, f, gpu_pos)) continue;
        ++out.frames_with_output;
        bool all_zero = true;
        for (size_t vi = 0; vi < s.verts.size(); ++vi) {
            const glm::vec3 g = gpu_pos[vi];
            const glm::vec3 c = s.cpu_pos[f][vi];
            out.max_pos_error = (std::max)(out.max_pos_error, std::fabs(g.x - c.x));
            out.max_pos_error = (std::max)(out.max_pos_error, std::fabs(g.y - c.y));
            out.max_pos_error = (std::max)(out.max_pos_error, std::fabs(g.z - c.z));
            if (std::fabs(g.x) > 1e-6f || std::fabs(g.y) > 1e-6f || std::fabs(g.z) > 1e-6f)
                all_zero = false;
        }
        if (all_zero) out.any_all_zero = true;
    }
    sys.Shutdown();
}

GpuConsistencyResult RunOnBackend(dse::test::BackendResult (*runner)(const dse::test::RenderFn&),
                                  const AnimSample& s) {
    GpuConsistencyResult result;
    dse::test::RenderFn fn = [&](RhiDevice& device) -> ::RenderTargetReadback {
        RunGpuConsistency(device, s, result);
        return {};
    };
    dse::test::BackendResult br = runner(fn);
    if (!br.available) result.compute_available = false;
    return result;
}

void CheckBackend(dse::test::BackendResult (*runner)(const dse::test::RenderFn&), const char* backend) {
    const AnimSample s = BuildAnimSample();
    ASSERT_TRUE(s.loaded) << backend << "：真实 .dskel/.danim 加载或解析失败（bone_count=0）";
    ASSERT_GE(s.bone_count, 1u) << backend;
    // 活体证据：动画调色板逐帧变化 + 蒙皮真位移顶点（排除静态/恒等空洞证据）。
    ASSERT_GT(s.pose_motion, 1e-3f) << backend << "：.danim 未驱动骨骼运动（调色板逐帧不变）";
    ASSERT_GT(s.skin_displacement, 1e-3f) << backend << "：蒙皮调色板未真正位移顶点";

    GpuConsistencyResult r = RunOnBackend(runner, s);
    if (!r.compute_available) {
        GTEST_SKIP() << backend << "：GPU compute 不可用（无驱动/软件后端），跳过";
    }
    ASSERT_EQ(r.frames_with_output, kFrames) << backend << "：部分帧 GPU compute 回读失败";
    ASSERT_FALSE(r.any_all_zero) << backend << "：某帧 GPU 输出全 0（dst 未写入回归）";
    // 逐帧逐顶点 GPU vs CPU 参考数值一致。
    EXPECT_LT(r.max_pos_error, 2e-3f) << backend << "：GPU skinning 与 CPU 参考逐帧不一致（max_err="
                                      << r.max_pos_error << "）";
}

}  // namespace

// ============================================================
// 三后端：真实 .danim 驱动 GPU skinning 逐帧 vs CPU 参考一致（真机 GPU 回读）。
// ============================================================

TEST(DanimGpuSkinningFrameConsistencyTest, OpenGL逐帧一致) {
    CheckBackend(dse::test::RunOpenGL, "OpenGL");
}
TEST(DanimGpuSkinningFrameConsistencyTest, D3D11逐帧一致) {
    CheckBackend(dse::test::RunD3D11, "D3D11");
}
TEST(DanimGpuSkinningFrameConsistencyTest, Vulkan逐帧一致) {
    CheckBackend(dse::test::RunVulkan, "Vulkan");
}

#endif  // DSE_ENABLE_3D

/**
 * @file webgpu_selftest_harness.h
 * @brief WebGPU 后端自检 harness（friend of WebGPURhiDevice，编译期门控）。
 *
 * 整文件由 DSE_WEBGPU_SELFTEST 门控；默认关闭 → 生产构建零编入、零运行开销。
 * 持全部 21 项自检的状态与 Record* / Kick* 方法；经 friend 访问设备私有内部（dev_->）。
 */
#ifndef DSE_WEBGPU_SELFTEST_HARNESS_H
#define DSE_WEBGPU_SELFTEST_HARNESS_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU) && defined(DSE_WEBGPU_SELFTEST)

#include "engine/render/rhi/rhi_device.h"

#include <webgpu/webgpu.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>

namespace dse {
namespace render {

class WebGPURhiDevice;

/// 编译期门控的 WebGPU 自检 harness：录制 21 项离屏自检并异步回读校验。
class WebGpuSelfTestHarness {
public:
    explicit WebGpuSelfTestHarness(WebGPURhiDevice* dev) : dev_(dev) {}

    /// 在 device EndFrame 的活动 frame_encoder_ 上录制全部 pending 自检（每会话各一次）。
    void RecordPending();
    /// 帧提交后对本帧已录制的自检发起异步 map 回读校验。
    void KickPendingReadbacks();
    /// bring-up 自检（首帧 backbuffer 三角形，证录制链路）。
    void RunBringUp();
    /// 析构：释放自检回读缓冲（多数已在 kick 时转移所有权）。
    ~WebGpuSelfTestHarness();

private:
    WebGPURhiDevice* dev_ = nullptr;

    // 本帧录制标记（RecordPending 置位 → KickPendingReadbacks 读取）
    bool ct_recorded_ = false;
    bool gc_recorded_ = false;
    bool sk_recorded_ = false;
    bool si_recorded_ = false;
    bool hz_recorded_ = false;
    bool hzp_recorded_ = false;
    bool cb_recorded_ = false;
    bool hc_recorded_ = false;
    bool mf_recorded_ = false;
    bool dg_recorded_ = false;
    bool hr_recorded_ = false;
    bool bl_recorded_ = false;
    bool gr_recorded_ = false;
    bool t41_recorded_ = false;
    bool t42_recorded_ = false;
    bool t43_recorded_ = false;
    bool t44_recorded_ = false;
    bool t51_recorded_ = false;
    bool t52_recorded_ = false;
    bool t53_recorded_ = false;
    bool t54_recorded_ = false;
    bool t55_recorded_ = false;

    // --- B2 bring-up 自检资源（验证整条录制链路；引擎 WGSL 内容就绪后自动不再触发）---
    bool selftest_init_ = false;
    unsigned int selftest_program_ = 0;
    unsigned int selftest_pso_ = 0;
    unsigned int selftest_vbo_ = 0;
    unsigned int selftest_tex_ = 0;
    unsigned int selftest_ubo_ = 0;

    // --- B3a compute 自检（每会话一次：dispatch 写 SSBO + indirect args → copy 到回读缓冲 → 异步回读校验）---
    bool compute_selftest_done_ = false;
    bool RecordComputeSelfTest();        ///< 在 frame_encoder_ 上录制 compute dispatch + copy（须在无 render/compute pass 时调用）
    void KickComputeSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int ct_shader_ = 0;
    unsigned int ct_params_ = 0;      ///< 参数 UBO（group1 b0）
    unsigned int ct_out_ = 0;         ///< 输出 SSBO（storage，group3 b0）
    unsigned int ct_draw_ = 0;        ///< indirect args SSBO（storage|indirect，group3 b1）
    WGPUBuffer ct_rb_out_ = nullptr;  ///< 输出回读缓冲（MapRead|CopyDst）
    WGPUBuffer ct_rb_draw_ = nullptr; ///< indirect 回读缓冲

    void EnsureSelfTestResources();
    void RunBringUpSelfTest();

    // --- B3b-2 GPU-driven 剔除自检（每会话一次：WGSL 视锥剔除 compute 写 per-instance indirect
    //   draw command → 真 wgpuRenderPassEncoderDrawIndexedIndirect 渲到离屏 RT → 回读 SSBO+像素
    //   校验「被剔实例 instance_count=0/无像素，可见实例 instance_count=1/有像素」。离屏隔离，
    //   不碰 demo backbuffer/golden；不翻转全局能力位。验证真 compute→SSBO→indirect-draw→像素链路）---
    bool gpu_cull_selftest_done_ = false;
    bool RecordGpuCullSelfTest();        ///< 在 frame_encoder_ 上录制 cull dispatch + 离屏 indirect 绘制 + copy（须在无 render/compute pass 时调用）
    void KickGpuCullSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int gc_cull_shader_ = 0;    ///< 视锥剔除 compute shader
    unsigned int gc_aabb_ssbo_ = 0;      ///< 实例 AABB（storage，group3 b0）
    unsigned int gc_draw_ssbo_ = 0;      ///< per-instance indirect draw commands（storage|indirect，group3 b1）
    unsigned int gc_params_ubo_ = 0;     ///< 剔除参数 UBO（6 视锥面 + 实例数，group1 b0）
    WGPUBuffer gc_rb_draw_ = nullptr;    ///< draw commands 回读缓冲（MapRead|CopyDst）
    WGPUBuffer gc_rb_pixels_ = nullptr;  ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）
    // 录制期创建、提交后随回读 ctx 释放的瞬态渲染资源（被命令缓冲引用至 GPU 执行完成）。
    WGPUTexture gc_rt_tex_ = nullptr;
    WGPUTextureView gc_rt_view_ = nullptr;
    WGPURenderPipeline gc_pipeline_ = nullptr;
    WGPUShaderModule gc_render_module_ = nullptr;
    WGPUBuffer gc_vbo_ = nullptr;
    WGPUBuffer gc_ibo_ = nullptr;

    // --- B3b-3 GPU 蒙皮 compute 真链路自检（每会话一次：手译自 skinning.comp 的 WGSL 蒙皮 compute
    //   经骨骼矩阵调色板把绑定空间顶点变形写入 dst SSBO → 该 SSBO 直接作顶点缓冲被真绘制消费渲到
    //   离屏 RT → 回读 dst SSBO（逐顶点校验蒙皮后坐标==CPU 预期）+ 像素（蒙皮位移后的 quad 落在
    //   预期屏幕区域）双重校验。离屏隔离，不碰 demo backbuffer/golden；不翻转全局能力位。
    //   验证真 compute(蒙皮)→SSBO(变形顶点)→draw(顶点拉取)→像素 端到端链路）---
    bool skinning_selftest_done_ = false;
    bool RecordSkinningSelfTest();        ///< 在 frame_encoder_ 上录制蒙皮 dispatch + 离屏绘制 + copy（须在无 render/compute pass 时调用）
    void KickSkinningSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int sk_shader_ = 0;       ///< 蒙皮 compute shader
    unsigned int sk_params_ubo_ = 0;   ///< 参数 UBO（total_vertices/instance_count，group1 b0）
    unsigned int sk_src_ssbo_ = 0;     ///< 源顶点（绑定空间 + 骨骼权重/索引，group3 b0）
    unsigned int sk_dst_ssbo_ = 0;     ///< 蒙皮后顶点（storage|vertex，group3 b1 / 绘制顶点缓冲）
    unsigned int sk_bone_ssbo_ = 0;    ///< 骨骼矩阵调色板（group3 b2）
    unsigned int sk_morph_ssbo_ = 0;   ///< morph delta（本自检 morph_target_count=0，仅占位，group3 b3）
    unsigned int sk_inst_ssbo_ = 0;    ///< 实例信息（group3 b4）
    WGPUBuffer sk_rb_dst_ = nullptr;   ///< 蒙皮后顶点回读缓冲（MapRead|CopyDst）
    WGPUBuffer sk_rb_pixels_ = nullptr;///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）
    // 录制期创建、提交后随回读 ctx 释放的瞬态渲染资源（被命令缓冲引用至 GPU 执行完成）。
    WGPUTexture sk_rt_tex_ = nullptr;
    WGPUTextureView sk_rt_view_ = nullptr;
    WGPURenderPipeline sk_pipeline_ = nullptr;
    WGPUShaderModule sk_render_module_ = nullptr;
    WGPUBuffer sk_ibo_ = nullptr;

    // --- B3b-4 storage-image compute 真链路自检（每会话一次：手写 WGSL compute 经
    //   `texture_storage_2d<rgba8unorm, write>` 把已知渐变（r=x/(N-1), g=y/(N-1)）逐像素 textureStore
    //   进 storage 纹理 → copy 纹理→回读缓冲 → 逐像素校验。验证 compute 写 storage image 端到端
    //   原语（Hi-Z 金字塔 / bloom 下采样的前置能力）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool storage_image_selftest_done_ = false;
    bool RecordStorageImageSelfTest();        ///< 录制 storage-image 写 compute + copy 纹理→回读缓冲
    void KickStorageImageSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int si_shader_ = 0;       ///< storage-image 写 compute shader
    unsigned int si_params_ubo_ = 0;   ///< 参数 UBO（dim，group1 b0）
    unsigned int si_image_ = 0;        ///< compute 写目标 storage 纹理（group2 b0）
    WGPUBuffer si_rb_pixels_ = nullptr;///< storage 纹理像素回读缓冲（MapRead|CopyDst）

    // --- B3b-5 Hi-Z 下采样核心 compute 真链路自检（每会话一次：两趟 r32float compute —— ①生成趟
    //   经 `texture_storage_2d<r32float, write>` 写已知渐变到 src；②下采样趟用 textureLoad 读 src 采样
    //   纹理 + 取 2×2 max 写 dst storage 纹理 → copy dst→回读缓冲 → 逐像素校验 == CPU 预期 max。
    //   验证 compute 读采样纹理(textureLoad) + 写 r32float storage 的读后写链路（Hi-Z 金字塔逐级
    //   下采样的核心原语）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool hiz_selftest_done_ = false;
    bool RecordHiZDownsampleSelfTest();        ///< 录制 ①生成趟 + ②下采样趟 compute + copy dst→回读缓冲
    void KickHiZDownsampleSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int hz_gen_shader_ = 0;   ///< 生成趟 compute shader（textureStore 渐变到 src）
    unsigned int hz_down_shader_ = 0;  ///< 下采样趟 compute shader（textureLoad src + 2×2 max → dst）
    unsigned int hz_gen_ubo_ = 0;      ///< 生成趟参数 UBO（src_dim，group1 b0）
    unsigned int hz_down_ubo_ = 0;     ///< 下采样趟参数 UBO（src_dim/dst_dim，group1 b0）
    unsigned int hz_src_tex_ = 0;      ///< src r32float 纹理（生成趟 storage 写 / 下采样趟采样读）
    unsigned int hz_dst_tex_ = 0;      ///< dst r32float 纹理（下采样趟 storage 写 / copy 源）
    WGPUBuffer hz_rb_pixels_ = nullptr;///< dst 纹理像素回读缓冲（MapRead|CopyDst）

    // --- B3b-6 Hi-Z storage-image 金字塔 compute 真链路自检（每会话一次：单张 R32Float mip 链纹理，
    //   ①生成趟 textureStore 已知渐变到 mip0；②逐级下采样趟用 textureLoad 读 mip[k-1] 采样视图 + 取
    //   2×2 max 写 mip[k] storage 视图（per-mip 显式视图绑定）；copy 各级 mip→回读缓冲，逐级逐像素
    //   校验 == CPU 预期递归 max。验证 per-mip 视图绑定 + 多级 storage 金字塔构建（GPU-driven Hi-Z
    //   遮挡剔除金字塔的核心原语，Task 4 前置）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool hizpyr_selftest_done_ = false;
    bool RecordHiZPyramidSelfTest();        ///< 录制 ①生成趟 + ②逐级下采样趟 compute + copy 各级 mip→回读缓冲
    void KickHiZPyramidSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int hzp_gen_shader_ = 0;       ///< 生成趟 compute shader（textureStore 渐变到 mip0）
    unsigned int hzp_down_shader_ = 0;      ///< 下采样趟 compute shader（textureLoad mip[k-1] + 2×2 max → mip[k]）
    unsigned int hzp_gen_ubo_ = 0;          ///< 生成趟参数 UBO（mip0 dim，group1 b0）
    std::vector<unsigned int> hzp_down_ubos_;  ///< 各下采样级参数 UBO（src_dim/dst_dim，group1 b0）
    unsigned int hzp_tex_ = 0;              ///< 单张 R32Float mip 链纹理（mip0 生成 + 逐级下采样 + copy 源）
    WGPUBuffer hzp_rb_pixels_ = nullptr;    ///< 各级 mip 像素回读缓冲（MapRead|CopyDst）

    // --- B3b-8 命名 uniform + compute 采样器绑定 真链路自检（每会话一次：手写 WGSL compute 经
    //   SetComputeUniform*（命名 i32/f32/vec2i/vec4/mat4，group1 保留 binding 命名块）+
    //   SetComputeTextureSampler（group2 b0，textureLoad 读已知渐变纹理）读入参数与纹理 →
    //   结果写 SSBO → copy SSBO→回读缓冲 → 逐元素校验。验证引擎 Hi-Z/GPU cull 真实 compute API 面
    //   （命名 uniform 块布局 + 句柄采样绑定）。离屏隔离，不翻转能力位、不碰 demo golden）---
    bool compute_bind_selftest_done_ = false;
    bool RecordComputeBindSelfTest();        ///< 录制 命名 uniform + 采样器绑定 compute + copy SSBO→回读缓冲
    void KickComputeBindSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int cb_shader_ = 0;             ///< compute shader（读命名 uniform 块 + textureLoad 采样纹理）
    unsigned int cb_tex_ = 0;                ///< 已知渐变 rgba8unorm 采样纹理（SetComputeTextureSampler 绑定）
    unsigned int cb_out_ = 0;                ///< 结果 SSBO（group3 b0）
    WGPUBuffer cb_rb_out_ = nullptr;         ///< 结果回读缓冲（MapRead|CopyDst）

    // --- B3b-9 Hi-Z 遮挡剔除真链路自检（每会话一次：手译引擎 HiZCullPass compute 为 WGSL —— AABB
    //   SSBO（group3 b0）8 角经命名 uniform u_view_projection 投影 → NDC/UV → off-screen 拒绝 →
    //   屏幕像素跨度选 mip → 5 tab Hi-Z（句柄采样，textureLoad-at-mip）max 深度遮挡判定 → 写可见性
    //   SSBO（group3 b1）→ copy 回读逐元素校验 == CPU 预期 [1,0,0,1]。证明该消费方着色器 WebGPU 可用。
    //   离屏隔离，不翻转能力位、不碰 demo golden）---
    bool hizcull_selftest_done_ = false;
    bool RecordHiZCullSelfTest();          ///< 录制 Hi-Z 剔除 compute + copy 可见性 SSBO→回读缓冲
    void KickHiZCullSelfTestReadback();    ///< 提交后发起异步 map 回读校验
    unsigned int hc_shader_ = 0;           ///< Hi-Z 剔除 compute shader（手译 HiZCullPass）
    unsigned int hc_aabb_ = 0;             ///< AABB SSBO（group3 b0）
    unsigned int hc_vis_ = 0;              ///< 可见性 SSBO（group3 b1）
    unsigned int hc_hiz_tex_ = 0;          ///< Hi-Z r32float 纹理（SetComputeTextureSampler 绑定）
    WGPUBuffer hc_rb_out_ = nullptr;       ///< 可见性回读缓冲（MapRead|CopyDst）

    // --- B3b-10 形变目标（morph）真链路自检（每会话一次：手译引擎 MorphTargetSystem compute 为 WGSL ——
    //   base 顶点 SSBO（group3 b0）+ delta SSBO（b1）+ weights SSBO（b2）经命名 uniform 顶点/目标数 →
    //   Σ weight·delta → normalize 法线 → 写形变顶点 SSBO（b3）→ copy 回读逐顶点校验 == CPU 预期。
    //   离屏隔离，不翻转能力位、不碰 demo golden）---
    bool morph_selftest_done_ = false;
    bool RecordMorphSelfTest();            ///< 录制 morph compute + copy 形变顶点 SSBO→回读缓冲
    void KickMorphSelfTestReadback();      ///< 提交后发起异步 map 回读校验
    unsigned int mf_shader_ = 0;           ///< morph compute shader（手译 MorphTargetSystem）
    unsigned int mf_base_ = 0;             ///< base 顶点 SSBO（group3 b0）
    unsigned int mf_delta_ = 0;            ///< morph delta SSBO（group3 b1）
    unsigned int mf_weight_ = 0;           ///< weights SSBO（group3 b2）
    unsigned int mf_out_ = 0;              ///< 形变顶点输出 SSBO（group3 b3）
    WGPUBuffer mf_rb_out_ = nullptr;       ///< 形变顶点回读缓冲（MapRead|CopyDst）

    // --- B3b-11 DDGI 探针更新核心真链路自检（每会话一次：手译引擎 DDGISystem probe-update compute 核心为
    //   WGSL —— probe SSBO（group3 b0）+ 3×RSM 句柄采样（group2 b2/b3/b4，textureLoad）+ 14 命名 uniform →
    //   octahedral 方向 + VPL 累积间接辐照度 → 归一化×0.01 → 写 irradiance/visibility storage image（group2
    //   b0/b1）+ float SSBO 调试输出（group3 b1）→ copy 回读逐 texel 校验 == CPU 预期。离屏隔离、不翻能力位。
    //   注：DDGI 翻转前另需消费方适配 storage/sampler 绑定槽错开 + temporal imageLoad 的 read-write storage）---
    bool ddgi_selftest_done_ = false;
    bool RecordDDGISelfTest();             ///< 录制 DDGI probe-update compute + copy 调试 SSBO→回读缓冲
    void KickDDGISelfTestReadback();       ///< 提交后发起异步 map 回读校验
    unsigned int dg_shader_ = 0;           ///< DDGI probe-update compute shader（手译 DDGISystem 核心）
    unsigned int dg_probe_ = 0;            ///< 探针状态 SSBO（group3 b0）
    unsigned int dg_dbg_ = 0;              ///< 每 texel 调试输出 SSBO（group3 b1，irr.rgb+权重）
    unsigned int dg_irr_tex_ = 0;          ///< irradiance storage image（group2 b0）
    unsigned int dg_vis_tex_ = 0;          ///< visibility storage image（group2 b1）
    unsigned int dg_rsm_pos_ = 0;          ///< RSM 位置采样纹理（group2 b2）
    unsigned int dg_rsm_nrm_ = 0;          ///< RSM 法线采样纹理（group2 b3）
    unsigned int dg_rsm_flux_ = 0;         ///< RSM 通量采样纹理（group2 b4）
    WGPUBuffer dg_rb_out_ = nullptr;       ///< 调试输出回读缓冲（MapRead|CopyDst）

    // --- B3b-12 头发物理 hair 全 4 趟真链路自检（每会话一次：直接取引擎真 compute 源
    //   hair_compute_shaders.h::kHair*SourceWGSL，按 HairInstance::Simulate 同序跑全 4 趟
    //   integrate→local_shape→length→tangent（同一 compute pass 内逐趟 dispatch，趟间隐式同步）：
    //   ①integrate（4×SSBO group3 b0..3 + 12 命名 uniform）根顶点固定 + velocity·(1-damping)+重力·dt²；
    //   ②local_shape（cur/rest/strand + 3 uniform）按 local/global stiffness 拉回 rest；③length（cur/rest/
    //   strand + 1 uniform）逐段长度约束；④tangent（cur/tangent/strand + 3 uniform）相邻差分写切线。
    //   末趟后 copy pos_cur+tangent 回读，逐分量校验 == C++ 同序复算预期。离屏隔离、不翻能力位。---
    bool hair_selftest_done_ = false;
    bool RecordHairSelfTest();             ///< 录制 hair 全 4 趟 compute + copy pos_cur/tangent→回读缓冲
    void KickHairSelfTestReadback();       ///< 提交后发起异步 map 回读校验
    unsigned int hr_shader_ = 0;           ///< pass1 integrate compute（kHairIntegrateSourceWGSL）
    unsigned int hr_local_ = 0;            ///< pass2 local_shape compute（kHairLocalShapeSourceWGSL）
    unsigned int hr_length_ = 0;           ///< pass3 length compute（kHairLengthConstraintSourceWGSL）
    unsigned int hr_tangent_ = 0;          ///< pass4 tangent compute（kHairUpdateTangentSourceWGSL）
    unsigned int hr_cur_ = 0;              ///< pos_cur SSBO（group3 b0，读写）
    unsigned int hr_prev_ = 0;             ///< pos_prev SSBO（integrate b1，读写）
    unsigned int hr_rest_ = 0;             ///< pos_rest SSBO（integrate b2 / local·length b1）
    unsigned int hr_tan_ = 0;              ///< tangent SSBO（tangent b1，读写）
    unsigned int hr_strand_ = 0;           ///< strand_info SSBO（{offset,count}）
    WGPUBuffer hr_rb_out_ = nullptr;       ///< pos_cur+tangent 回读缓冲（MapRead|CopyDst）

    // --- B3b-13 bloom 双滤波 compute 真链路自检（每会话一次：手译引擎 BloomRenderer 真 compute
    //   （bloom_downsample.comp / bloom_upsample.comp，GLSL 450）核心为 WGSL —— ①gen compute 写已知
    //   公式渐变进 src8/usrc4/ubase4 rgba16f；②下采样 13-tap 加权（src8→down4）；③上采样 3×3 tent +
    //   按 blend 累加（usrc4+ubase4→up4）→ copy down4/up4 回读半精解码逐 texel 逐通道校验 == CPU 预期。
    //   离屏隔离、不翻能力位。注：上采样消费方真翻转前需 ping-pong（in-place imageLoad rgba16f 不支持））---
    bool bloom_selftest_done_ = false;
    bool RecordBloomSelfTest();            ///< 录制 gen + 下采样 + 上采样 compute + copy down4/up4→回读缓冲
    void KickBloomSelfTestReadback();      ///< 提交后发起异步 map 回读校验
    unsigned int bl_gen_shader_  = 0;      ///< 公式渐变生成 compute（kind 选公式，写 rgba16f storage）
    unsigned int bl_down_shader_ = 0;      ///< 下采样 13-tap compute（手译 bloom_downsample.comp）
    unsigned int bl_up_shader_   = 0;      ///< 上采样 3×3 tent + 累加 compute（手译 bloom_upsample.comp）
    unsigned int bl_src8_   = 0;           ///< 下采样源 rgba16f 8×8（gen 写 + 采样读）
    unsigned int bl_down4_  = 0;           ///< 下采样输出 rgba16f 4×4（storage 写 + copy 源）
    unsigned int bl_usrc4_  = 0;           ///< 上采样源 rgba16f 4×4（gen 写 + 采样读）
    unsigned int bl_ubase4_ = 0;           ///< 上采样 base rgba16f 4×4（gen 写 + 采样读，替代 in-place imageLoad）
    unsigned int bl_up4_    = 0;           ///< 上采样输出 rgba16f 4×4（storage 写 + copy 源）
    WGPUBuffer bl_rb_out_ = nullptr;       ///< down4+up4 回读缓冲（MapRead|CopyDst）

    // --- B3b-14 草地风场（grass wind）compute 正确性真链路自检（每会话一次：grass_system.cpp 的
    //   grass 风场 WGSL 在 modules 层，engine 层不能 include modules 头，故此处**内联一份镜像**
    //   （与 grass_system.cpp::kGrassWindComputeSourceWGSL 逐句一致，注释标明）—— 造已知实例 SSBO
    //   （group3 b0：pos.xy + phase + height）+ 命名 uniform（time/wind_dir/strength/...）→ 每实例算
    //   h*fade 折叠风偏 → 写列主序 mat4 输出 SSBO（group3 b1）→ pass 后 copy 回读，C++ 复算同公式
    //   （镜像 CPU BuildWindMatrix）逐元素 abs(diff)<1e-4 校验。离屏隔离、每会话一次、不翻能力位。---
    bool grass_selftest_done_ = false;
    bool RecordGrassSelfTest();            ///< 录制 grass 风场 compute + copy 输出 mat4 SSBO→回读缓冲
    void KickGrassSelfTestReadback();      ///< 提交后发起异步 map 回读校验
    unsigned int gr_shader_ = 0;           ///< grass 风场 compute（内联镜像 kGrassWindComputeSourceWGSL）
    unsigned int gr_in_ = 0;               ///< 实例输入 SSBO（group3 b0：vec4 pos.xy/phase/height）
    unsigned int gr_out_ = 0;              ///< 风矩阵输出 SSBO（group3 b1：每实例 mat4）
    WGPUBuffer gr_rb_out_ = nullptr;       ///< 风矩阵回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 1 离屏自检（MultiDrawIndexedIndirect 真链路：预置 [1,0,1,0] indirect cmds →
    //   经引擎-facing CmdBeginRenderPass + CmdBind* + MultiDrawIndexedIndirect 渲到 64×64 离屏 RT →
    //   copyTextureToBuffer → 异步回读半精解码校验「可见象限有色、被剔象限为黑」。离屏隔离、不翻能力位）---
    bool t41_mdi_selftest_done_ = false;
    bool RecordMultiDrawIndirectSelfTest();        ///< 在 frame_encoder_ 上录制引擎-facing 间接绘制 + copy（须在无 render/compute pass 时调用）
    void KickMultiDrawIndirectSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int t41_rt_      = 0;          ///< 离屏 RT（引擎 CreateRenderTarget，RGBA16Float + CopySrc）
    unsigned int t41_program_ = 0;          ///< 内建 WGSL 程序（pos.xy@loc0 + color.rgb@loc1）
    unsigned int t41_pso_     = 0;          ///< PSO（无深度/无剔除/三角列表）
    unsigned int t41_vbo_     = 0;          ///< 4 象限 quad 顶点缓冲（pos.xy + color.rgb，stride 20）
    unsigned int t41_ibo_     = 0;          ///< 4 象限索引缓冲（每象限 6 索引，UInt32）
    unsigned int t41_indirect_ = 0;         ///< 预置 4 条 indirect cmd（instance_count=[1,0,1,0]）
    WGPUBuffer   t41_rb_pixels_ = nullptr;  ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 2 离屏自检（CreateMegaVAO → UpdateMegaVBO/IBO 上传 4 象限 BatchVertex(92B) 几何 →
    //   BindMegaVAO 设 92B draw state → CmdDrawIndexed 渲到 64×64 离屏 RT → copy 回读半精解码校验
    //   4 象限各自颜色就位（即 92B 布局 pos@0/color@12 解析正确）。离屏隔离、不翻能力位）---
    bool t42_mega_selftest_done_ = false;
    bool RecordMegaVaoSelfTest();          ///< 录制引擎-facing Mega VAO 绑定 + 索引绘制 + copy
    void KickMegaVaoSelfTestReadback();    ///< 提交后发起异步 map 回读校验
    unsigned int t42_rt_      = 0;         ///< 离屏 RT（RGBA16Float + CopySrc）
    unsigned int t42_program_ = 0;         ///< BatchVertex 92B 布局 WGSL 程序（pos@loc0 + color@loc1）
    unsigned int t42_pso_     = 0;         ///< PSO（无深度/无剔除/三角列表）
    VertexArrayHandle t42_vao_{};          ///< 被测 Mega VAO 句柄
    BufferHandle t42_vbo_{};               ///< Mega VBO 句柄
    BufferHandle t42_ibo_{};               ///< Mega IBO 句柄
    WGPUBuffer   t42_rb_pixels_ = nullptr; ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 3 离屏自检（SetupGPUDrivenPBRShader 激活 PBR 程序+绑 PerFrame/PerScene UBO →
    //   BindGpuBuffer 实例 SSBO(b5,2 个 model 平移左右)+材质 SSBO(b9,红/绿 albedo) → BindMegaVAO 设 92B
    //   draw state → BindGPUDrivenTextures(白 albedo) → MultiDrawIndexedIndirect(1 cmd,instanceCount=2)
    //   渲到 64×64 离屏 RT → copy 回读半精解码校验左半红、右半绿。离屏隔离、不翻能力位）---
    bool t43_pbr_selftest_done_ = false;
    bool RecordGpuDrivenPBRSelfTest();        ///< 录制引擎-facing GPU-driven PBR indirect 绘制 + copy
    void KickGpuDrivenPBRSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int t43_rt_      = 0;            ///< 离屏 RT（RGBA16Float + 深度，CopySrc）
    VertexArrayHandle t43_vao_{};             ///< 单 quad Mega VAO
    BufferHandle t43_vbo_{};                  ///< Mega VBO
    BufferHandle t43_ibo_{};                  ///< Mega IBO
    BufferHandle t43_inst_ssbo_{};            ///< 实例 SSBO（2×GPUInstanceData，b5）
    BufferHandle t43_mat_ssbo_{};             ///< 材质 SSBO（2×GPUMaterialData，b9）
    BufferHandle t43_indirect_{};             ///< 1 条 indirect cmd（instanceCount=2）
    unsigned int t43_albedo_tex_ = 0;         ///< 白 albedo 纹理（验证纹理采样链路）
    WGPUBuffer   t43_rb_pixels_ = nullptr;    ///< 离屏 RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 4 Subtask 4 离屏自检（经 CreateHiZTexture 真资源建 mip 链 → 引擎-facing
    //   SetComputeTextureImageMip 写 mip0 占位深度 + 逐级 2×2 max 下采样建金字塔 → HiZCullPass WGSL
    //   经 SetComputeTextureSampler(GetHiZGpuTexture) + GetHiZMipCount 采样判遮挡 → 回读可见性 SSBO
    //   校验近物可见/远物（被金字塔最大深度遮挡）剔除。离屏隔离、不翻 SupportsIndirectDraw()）---
    bool t44_hiz_selftest_done_ = false;
    bool RecordGpuDrivenHiZCullSelfTest();        ///< 录制 Hi-Z 资源建链 + 下采样 + 遮挡剔除 dispatch + copy
    void KickGpuDrivenHiZCullSelfTestReadback();  ///< 提交后发起异步 map 回读校验
    unsigned int t44_hiz_handle_ = 0;             ///< CreateHiZTexture 返回的 hiz 句柄
    unsigned int t44_gen_shader_ = 0;             ///< 写 mip0 占位深度的 compute
    unsigned int t44_down_shader_ = 0;            ///< 逐级 2×2 max 下采样 compute
    unsigned int t44_cull_shader_ = 0;            ///< HiZCullPass 手译 WGSL（采样金字塔判遮挡）
    BufferHandle t44_gen_ubo_{};                  ///< 生成趟 params（dim）
    std::vector<unsigned int> t44_down_ubos_;     ///< 各级下采样 params（src_dim/dst_dim）
    BufferHandle t44_aabb_{};                     ///< AABB SSBO（group3 b0）
    BufferHandle t44_vis_{};                      ///< 可见性 SSBO（group3 b1）
    WGPUBuffer   t44_rb_out_ = nullptr;           ///< 可见性回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 1 离屏自检（CSM 方向光阴影深度图采样：①阴影深度趟把中心遮挡 quad(z=0.3)
    //   渲入 32×32 Depth32 atlas；②前向趟把 atlas 作 texture_depth_2d 经 textureLoad 3×3 PCF 采样比较
    //   （receiverDepth=0.6）渲到 64×64 RGBA16Float RT → copy 回读校验 中心受遮挡为暗、四角受光为亮。
    //   证明「阴影 pass 写 atlas → 前向 pass 采样」的跨 pass 深度图采样能力，离屏隔离、不翻能力位）---
    bool t51_csm_selftest_done_ = false;
    bool RecordCSMShadowSelfTest();          ///< 录制 阴影深度趟 + 前向采样趟 + copy 颜色→回读缓冲
    void KickCSMShadowSelfTestReadback();    ///< 提交后发起异步 map 回读校验
    unsigned int t51_shadow_rt_   = 0;       ///< shadow atlas RT（含 Depth32 深度附件，TextureBinding 可采样）
    unsigned int t51_color_rt_    = 0;       ///< 离屏 color RT（RGBA16Float + CopySrc）
    unsigned int t51_occ_program_ = 0;       ///< 遮挡 quad 程序（写深度，pos.xyz@loc0）
    unsigned int t51_occ_pso_     = 0;       ///< 遮挡 PSO（depth test/write on、cull none）
    unsigned int t51_recv_program_ = 0;      ///< 前向接收程序（采样 atlas，pos.xy@loc0 + uv@loc1）
    unsigned int t51_recv_pso_    = 0;       ///< 前向 PSO（无深度/无剔除/blend off）
    unsigned int t51_occ_vbo_     = 0;       ///< 遮挡 quad 顶点缓冲（pos.xyz，stride 12）
    unsigned int t51_occ_ibo_     = 0;       ///< 遮挡 quad 索引缓冲（6 索引，UInt32）
    unsigned int t51_recv_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy + uv，stride 16）
    unsigned int t51_recv_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t51_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 2 离屏自检（延迟着色：①几何趟把中心 quad 渲入 64×64×3 MRT gbuffer
    //   （albedo/normal/position 三附件）；②全屏光照趟 textureLoad 3 张 gbuffer 做延迟光照渲到 64×64
    //   RGBA16Float RT → copy 回读校验 中心几何受光为红、四角空像素为黑。证明「几何趟写 MRT gbuffer →
    //   光照趟采样 gbuffer」的延迟着色能力，离屏隔离、不翻能力位，逻辑同 deferred_lighting.frag）---
    bool t52_deferred_selftest_done_ = false;
    bool RecordDeferredSelfTest();           ///< 录制 几何趟（MRT gbuffer）+ 光照趟 + copy 颜色→回读缓冲
    void KickDeferredSelfTestReadback();     ///< 提交后发起异步 map 回读校验
    unsigned int t52_gbuffer_rt_  = 0;       ///< gbuffer RT（3 个 RGBA16Float 颜色附件 albedo/normal/position）
    unsigned int t52_color_rt_    = 0;       ///< 离屏 color RT（RGBA16Float + CopySrc）
    unsigned int t52_geom_program_ = 0;      ///< 几何程序（写 MRT 3 附件，pos.xy@loc0）
    unsigned int t52_geom_pso_    = 0;       ///< 几何 PSO（无深度/无剔除/blend off）
    unsigned int t52_light_program_ = 0;     ///< 延迟光照程序（textureLoad 3 张 gbuffer，pos.xy@loc0）
    unsigned int t52_light_pso_   = 0;       ///< 光照 PSO（无深度/无剔除/blend off）
    unsigned int t52_geom_vbo_    = 0;       ///< 几何 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t52_geom_ibo_    = 0;       ///< 几何 quad 索引缓冲（6 索引，UInt32）
    unsigned int t52_light_vbo_   = 0;       ///< 全屏 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t52_light_ibo_   = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t52_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 3 离屏自检（HDR auto-exposure 亮度归约 + ACES tonemap：①渲已知 HDR(4,2,1)
    //   到 8×8 场景 RT；②归约趟 textureLoad 整张算平均 log 亮度写 1×1 lum RT；③lum_adapt 趟 0.18/avgLum
    //   曝光写 1×1 exposure RT；④tonemap 趟 ACES(hdr*exposure)+gamma 渲 64×64 RT → 回读 C++ 同公式复算
    //   逐通道校验。逻辑同 lum_compute/lum_adapt/tonemapping.frag，离屏隔离、不翻能力位）---
    bool t53_hdr_selftest_done_ = false;
    bool RecordHDRSelfTest();                ///< 录制 场景趟 + 归约趟 + lum_adapt 趟 + tonemap 趟 + copy
    void KickHDRSelfTestReadback();          ///< 提交后发起异步 map 回读校验
    unsigned int t53_scene_rt_    = 0;       ///< HDR 场景 RT（8×8 RGBA16Float）
    unsigned int t53_lum_rt_      = 0;       ///< 平均 log 亮度 RT（1×1 RGBA16Float）
    unsigned int t53_exposure_rt_ = 0;       ///< 自动曝光 RT（1×1 RGBA16Float）
    unsigned int t53_color_rt_    = 0;       ///< 离屏 color RT（64×64 RGBA16Float + CopySrc）
    unsigned int t53_pso_         = 0;       ///< 共享 PSO（无深度/无剔除/blend off）
    unsigned int t53_scene_program_  = 0;    ///< HDR 场景程序（输出常量 (4,2,1)）
    unsigned int t53_reduce_program_ = 0;    ///< 亮度归约程序（textureLoad 整张算平均 log 亮度）
    unsigned int t53_adapt_program_  = 0;    ///< lum_adapt 程序（0.18/avgLum 曝光）
    unsigned int t53_tonemap_program_ = 0;   ///< tonemap 程序（ACES(hdr*exposure)+gamma）
    unsigned int t53_quad_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t53_quad_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t53_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 4 离屏自检（IBL：①BRDF LUT 趟 GGX split-sum 积分渲 64×64 LUT RT；②irradiance
    //   趟渲常量辐照度到 1×1 RT；③prefilter 趟渲常量预滤波镜面到 1×1 RT；④PBR 环境项趟绑 LUT/irr/pref
    //   三纹理按 split-sum 合成 ambient 渲 64×64 RT → 回读 C++ 同算法复算逐通道校验。逻辑同 LearnOpenGL
    //   IBL，离屏隔离、不翻能力位）---
    bool t54_ibl_selftest_done_ = false;
    bool RecordIBLSelfTest();                ///< 录制 BRDF LUT 趟 + irradiance 趟 + prefilter 趟 + PBR 趟 + copy
    void KickIBLSelfTestReadback();          ///< 提交后发起异步 map 回读校验
    unsigned int t54_brdf_rt_     = 0;       ///< BRDF LUT RT（64×64 RGBA16Float）
    unsigned int t54_irr_rt_      = 0;       ///< 辐照度 RT（1×1 RGBA16Float）
    unsigned int t54_pref_rt_     = 0;       ///< 预滤波镜面 RT（1×1 RGBA16Float）
    unsigned int t54_color_rt_    = 0;       ///< 离屏 color RT（64×64 RGBA16Float + CopySrc）
    unsigned int t54_pso_         = 0;       ///< 共享 PSO（无深度/无剔除/blend off）
    unsigned int t54_brdf_program_ = 0;      ///< BRDF LUT 程序（GGX split-sum 积分）
    unsigned int t54_irr_program_  = 0;      ///< 辐照度程序（输出常量辐照度）
    unsigned int t54_pref_program_ = 0;      ///< 预滤波镜面程序（输出常量预滤波色）
    unsigned int t54_pbr_program_  = 0;      ///< PBR 环境项程序（split-sum 合成）
    unsigned int t54_quad_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy + uv，stride 16）
    unsigned int t54_quad_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t54_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）

    // --- Task 5 Subtask 5 离屏自检（WBOIT：①几何趟把两层半透明片元按 WBOIT 权重 shader 内解析累加写
    //   accum/reveal 2 附件 MRT；②resolve 趟绑 accum/reveal 两纹理做 accum.rgb/max(accum.a,eps)、1-reveal
    //   合成渲 64×64 RT → 回读 C++ 同公式复算逐通道校验。证明 accum/reveal MRT + resolve OIT 混合能力，
    //   离屏隔离、不翻能力位）---
    bool t55_wboit_selftest_done_ = false;
    bool RecordWBOITSelfTest();              ///< 录制 几何趟（accum/reveal MRT）+ resolve 趟 + copy
    void KickWBOITSelfTestReadback();        ///< 提交后发起异步 map 回读校验
    unsigned int t55_mrt_rt_      = 0;       ///< accum/reveal MRT（2 个 RGBA16Float 颜色附件）
    unsigned int t55_color_rt_    = 0;       ///< 离屏 color RT（64×64 RGBA16Float + CopySrc）
    unsigned int t55_geom_pso_    = 0;       ///< 几何 PSO（无深度/无剔除/blend off）
    unsigned int t55_resolve_pso_ = 0;       ///< resolve PSO（无深度/无剔除/blend off）
    unsigned int t55_geom_program_    = 0;   ///< 几何程序（WBOIT 权重解析累加写 MRT）
    unsigned int t55_resolve_program_ = 0;   ///< resolve 程序（accum/reveal 合成）
    unsigned int t55_quad_vbo_    = 0;       ///< 全屏 quad 顶点缓冲（pos.xy，stride 8）
    unsigned int t55_quad_ibo_    = 0;       ///< 全屏 quad 索引缓冲（6 索引，UInt32）
    WGPUBuffer   t55_rb_pixels_   = nullptr; ///< color RT 像素回读缓冲（MapRead|CopyDst）
};

} // namespace render
} // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU && DSE_WEBGPU_SELFTEST
#endif  // DSE_WEBGPU_SELFTEST_HARNESS_H

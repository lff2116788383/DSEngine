#include "engine/render/passes/atmosphere_sky_pass.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/platform/screen.h"
#include <glm/gtc/constants.hpp>
#include <functional>
#include <cmath>

namespace dse {
namespace render {

void AtmosphereSkyPass::Setup(RenderGraph& graph) {
    auto scene_color = graph.DeclareResource("scene_color");
    auto prez_depth  = graph.DeclareResource("prez_depth");
    auto atm_sky     = graph.DeclareResource("atmosphere_sky");

    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, scene_color);
    graph.PassRead(pass, prez_depth);
    graph.PassWrite(pass, atm_sky);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void AtmosphereSkyPass::Execute(CommandBuffer& cmd_buffer) {
    const auto& snap = *ctx_.snapshot;
    active_ = snap.atmosphere_sky.valid;
    if (!active_) return;
    if (!snap.camera_3d.valid) return;

    const auto& atm = snap.atmosphere_sky;

    // 确保 transmittance LUT 可用
    EnsureTransmittanceLUT(atm.transmittance_lut_width, atm.transmittance_lut_height);

    // 检测参数变化 → 重算 LUT
    size_t current_hash = ComputeParamsHash();
    if (!lut_valid_ || current_hash != prev_params_hash_) {
        RenderTransmittanceLUT(cmd_buffer);
        prev_params_hash_ = current_hash;
        lut_valid_ = true;
    }

    // 获取 scene depth 用于天空 mask (仅渲染 far 像素)
    const unsigned int depth_tex = ctx_.rhi_device->GetRenderTargetDepthTexture(ctx_.render_targets.prez);
    if (depth_tex == 0) return;

    // 太阳方向
    glm::vec3 sun_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    const float sun_len2 = glm::dot(atm.sun_direction, atm.sun_direction);
    if (sun_len2 > 1e-8f) {
        sun_dir = glm::normalize(atm.sun_direction);
    }

    // 相机参数 (Camera-Relative: cam_pos = 0)
    float near_p = snap.camera_3d.near_clip;
    float far_p  = snap.camera_3d.far_clip;
    float fov_y  = snap.camera_3d.fov;
    const int screen_w = Screen::width();
    const int screen_h = Screen::height();
    float aspect = screen_h > 0 ? static_cast<float>(screen_w) / static_cast<float>(screen_h) : 1.0f;
    float tan_fov_y = std::tan(glm::radians(fov_y) * 0.5f);

    glm::vec3 cam_right = snap.camera_3d.right;
    glm::vec3 cam_up    = snap.camera_3d.up;
    glm::vec3 cam_fwd   = snap.camera_3d.forward;

    // params 布局（36 float）:
    // [0-2]    sun_dir.xyz
    // [3-5]    rayleigh_coeff.xyz
    // [6]      rayleigh_scale_height
    // [7]      mie_coeff
    // [8]      mie_scale_height
    // [9]      mie_g
    // [10]     planet_radius
    // [11]     atmosphere_height
    // [12-14]  sun_intensity.xyz
    // [15]     sun_disk_angle (radians)
    // [16]     near
    // [17]     far
    // [18]     tan_fov_y
    // [19]     aspect
    // [20-22]  cam_right.xyz
    // [23-25]  cam_up.xyz
    // [26-28]  cam_fwd.xyz
    // [29-31]  ozone_coeff.xyz
    // [32]     ozone_center_h
    // [33]     ozone_width
    // [34]     sky_view_steps
    // [35]     reserved

    unsigned int scene_tex = ctx_.rhi_device->GetRenderTargetColorTexture(ctx_.render_targets.scene);

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({ctx_.render_targets.scene, glm::vec4(0.0f), false});
    cmd_buffer.DrawPostProcess(PostProcessRequest{"atmosphere_sky", scene_tex, {
        sun_dir.x, sun_dir.y, sun_dir.z,
        atm.rayleigh_coeff.x, atm.rayleigh_coeff.y, atm.rayleigh_coeff.z,
        atm.rayleigh_scale_height,
        atm.mie_coeff,
        atm.mie_scale_height,
        atm.mie_g,
        atm.planet_radius,
        atm.atmosphere_height,
        atm.sun_intensity.x, atm.sun_intensity.y, atm.sun_intensity.z,
        glm::radians(atm.sun_disk_angle),
        near_p, far_p,
        tan_fov_y, aspect,
        cam_right.x, cam_right.y, cam_right.z,
        cam_up.x, cam_up.y, cam_up.z,
        cam_fwd.x, cam_fwd.y, cam_fwd.z,
        atm.ozone_coeff.x, atm.ozone_coeff.y, atm.ozone_coeff.z,
        atm.ozone_center_h, atm.ozone_width,
        static_cast<float>(atm.sky_view_steps),
        0.0f // reserved
    }}.Tex(2, depth_tex).Tex(3, transmittance_lut_));
    cmd_buffer.EndRenderPass();
}

void AtmosphereSkyPass::EnsureTransmittanceLUT(int width, int height) {
    if (transmittance_lut_ != 0 && lut_width_ == width && lut_height_ == height)
        return;

    // 如果尺寸变化或未初始化，重建 RT
    if (transmittance_rt_ != 0) {
        ctx_.rhi_device->DeleteRenderTarget(transmittance_rt_);
    }
    // 创建 RT 用于 LUT
    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.has_color = true;
    desc.has_depth = false;
    transmittance_rt_ = ctx_.rhi_device->CreateRenderTarget(desc);
    transmittance_lut_ = ctx_.rhi_device->GetRenderTargetColorTexture(transmittance_rt_);
    lut_width_ = width;
    lut_height_ = height;
    lut_valid_ = false;
}

void AtmosphereSkyPass::RenderTransmittanceLUT(CommandBuffer& cmd_buffer) {
    if (transmittance_rt_ == 0) return;

    const auto& atm = ctx_.snapshot->atmosphere_sky;

    // params 布局（8 float）:
    // [0] planet_radius
    // [1] atmosphere_height
    // [2-4] rayleigh_coeff.xyz
    // [5] rayleigh_scale_height
    // [6] mie_coeff
    // [7] mie_scale_height

    cmd_buffer.SetPipelineState(ctx_.pipeline_states.composite);
    cmd_buffer.BeginRenderPass({transmittance_rt_, glm::vec4(0.0f), true});
    // 已迁到 PostProcessRenderer：无源纹理效果（仅由 8 标量 UBO 程序化生成 LUT）。
    post_process_renderer_.Draw(cmd_buffer, *ctx_.rhi_device,
        PostProcessRequest{"atmosphere_transmittance_lut", 0, {
            atm.planet_radius,
            atm.atmosphere_height,
            atm.rayleigh_coeff.x, atm.rayleigh_coeff.y, atm.rayleigh_coeff.z,
            atm.rayleigh_scale_height,
            atm.mie_coeff,
            atm.mie_scale_height
        }});
    cmd_buffer.EndRenderPass();
}

size_t AtmosphereSkyPass::ComputeParamsHash() const {
    const auto& atm = ctx_.snapshot->atmosphere_sky;
    // 简单 hash：把关键参数组合
    size_t h = 0;
    auto hash_f = [&h](float v) {
        h ^= std::hash<float>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
    };
    hash_f(atm.planet_radius);
    hash_f(atm.atmosphere_height);
    hash_f(atm.rayleigh_coeff.x);
    hash_f(atm.rayleigh_coeff.y);
    hash_f(atm.rayleigh_coeff.z);
    hash_f(atm.rayleigh_scale_height);
    hash_f(atm.mie_coeff);
    hash_f(atm.mie_scale_height);
    hash_f(atm.ozone_coeff.x);
    hash_f(atm.ozone_coeff.y);
    hash_f(atm.ozone_coeff.z);
    return h;
}

} // namespace render
} // namespace dse

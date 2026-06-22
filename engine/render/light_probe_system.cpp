/**
 * @file light_probe_system.cpp
 * @brief Light Probe SH Bake 系统实现
 */

#include "engine/render/light_probe_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/platform/screen.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace dse {
namespace render {

// cubemap 6 面方向与 up 向量
static const glm::vec3 kFaceDirections[6] = {
    { 1, 0, 0}, {-1, 0, 0},  // +X, -X
    { 0, 1, 0}, { 0,-1, 0},  // +Y, -Y
    { 0, 0, 1}, { 0, 0,-1},  // +Z, -Z
};
static const glm::vec3 kFaceUps[6] = {
    {0,-1, 0}, {0,-1, 0},
    {0, 0, 1}, {0, 0,-1},
    {0,-1, 0}, {0,-1, 0},
};

// SH L2 基函数系数
static constexpr float kSH_Y00  = 0.282095f;    // 1 / (2*sqrt(pi))
static constexpr float kSH_Y1n  = 0.488603f;    // sqrt(3) / (2*sqrt(pi))
static constexpr float kSH_Y2n0 = 1.092548f;    // sqrt(15) / (2*sqrt(pi))
static constexpr float kSH_Y20  = 0.315392f;    // sqrt(5) / (4*sqrt(pi))
static constexpr float kSH_Y2p2 = 0.546274f;    // sqrt(15) / (4*sqrt(pi))

void LightProbeSystem::Init(RhiDevice* rhi_device) {
    if (initialized_ || !rhi_device) return;

    RenderTargetDesc desc;
    desc.width = face_resolution_;
    desc.height = face_resolution_;
    desc.has_color = true;
    desc.has_depth = true;
    desc.cube_map = false;  // 单面 RT，逐面渲染后回读
    cubemap_rt_ = rhi_device->CreateRenderTarget(desc);

    initialized_ = true;
    DEBUG_LOG_INFO("[LightProbeSystem] Initialized, face_res={}, RT={}", face_resolution_, cubemap_rt_);
}

void LightProbeSystem::Shutdown() {
    baked_probes_.clear();
    cubemap_rt_ = 0;
    initialized_ = false;
}

// ============================================================================
// 对 RGBA8 像素积分单面 SH
// ============================================================================
void LightProbeSystem::IntegrateFaceSH(const unsigned char* rgba8, int width, int height,
                                        int face_index, SHL2& out_sh) {
    const float inv_w = 1.0f / static_cast<float>(width);
    const float inv_h = 1.0f / static_cast<float>(height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 像素中心 UV → [-1,1]
            float u = (static_cast<float>(x) + 0.5f) * inv_w * 2.0f - 1.0f;
            float v = (static_cast<float>(y) + 0.5f) * inv_h * 2.0f - 1.0f;

            // cubemap 面方向 → 世界方向
            glm::vec3 dir;
            switch (face_index) {
                case 0: dir = glm::vec3( 1, -v, -u); break; // +X
                case 1: dir = glm::vec3(-1, -v,  u); break; // -X
                case 2: dir = glm::vec3( u,  1,  v); break; // +Y
                case 3: dir = glm::vec3( u, -1, -v); break; // -Y
                case 4: dir = glm::vec3( u, -v,  1); break; // +Z
                case 5: dir = glm::vec3(-u, -v, -1); break; // -Z
                default: dir = glm::vec3(0, 0, 1); break;
            }
            dir = glm::normalize(dir);

            // 立体角权重（cubemap texel 对应的立体角近似）
            float tmp = 1.0f + u * u + v * v;
            float d_omega = 4.0f * inv_w * inv_h / (std::sqrt(tmp) * tmp);

            // 线性颜色（从 sRGB 近似解码 gamma 2.2）
            int idx = (y * width + x) * 4;
            glm::vec3 color(
                std::pow(rgba8[idx + 0] / 255.0f, 2.2f),
                std::pow(rgba8[idx + 1] / 255.0f, 2.2f),
                std::pow(rgba8[idx + 2] / 255.0f, 2.2f)
            );
            color *= d_omega;

            // 累加 SH 基函数
            float nx = dir.x, ny = dir.y, nz = dir.z;
            out_sh.coeffs[0] += color * kSH_Y00;
            out_sh.coeffs[1] += color * kSH_Y1n * ny;
            out_sh.coeffs[2] += color * kSH_Y1n * nz;
            out_sh.coeffs[3] += color * kSH_Y1n * nx;
            out_sh.coeffs[4] += color * kSH_Y2n0 * nx * ny;
            out_sh.coeffs[5] += color * kSH_Y2n0 * ny * nz;
            out_sh.coeffs[6] += color * kSH_Y20  * (3.0f * nz * nz - 1.0f);
            out_sh.coeffs[7] += color * kSH_Y2n0 * nx * nz;
            out_sh.coeffs[8] += color * kSH_Y2p2 * (nx * nx - ny * ny);
        }
    }
}

// ============================================================================
// 对单个位置渲染 6 面并积分 SH
// ============================================================================
SHL2 LightProbeSystem::BakeSHAtPosition(const glm::vec3& position, int face_resolution,
                                         RhiDevice* rhi_device, unsigned int cubemap_rt,
                                         RenderPassContext& ctx,
                                         SkyboxRenderer* skybox_renderer) {
    SHL2 sh;
    if (!rhi_device || cubemap_rt == 0) return sh;

    const glm::mat4 proj = rhi_device->GetProjectionCorrection() *
                           glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 500.0f);

    for (int face = 0; face < 6; ++face) {
        glm::mat4 view = glm::lookAt(position, position + kFaceDirections[face], kFaceUps[face]);

        // 每面独立 CommandBuffer，提交后立即回读
        auto face_cmd = rhi_device->CreateCommandBuffer();
        face_cmd->BeginRenderPass({cubemap_rt, glm::vec4(0.0f), true});
        face_cmd->SetCamera(view, proj);

        // 渲染天空盒（通用绘制原语，自带天空盒 PSO）
        if (skybox_renderer) {
            auto skybox_view = ctx.world->registry().view<dse::SkyboxComponent>();
            for (auto sky_entity : skybox_view) {
                auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
                if (skybox.enabled && skybox.cubemap_handle != 0) {
                    skybox_renderer->Draw(*face_cmd, *rhi_device, skybox.cubemap_handle, view, proj);
                }
                break;
            }
        }

        // 渲染 3D 网格（通过 render_meshes 回调）
        face_cmd->BindPipeline(ctx.pipeline_states.mesh);
        if (ctx.render_meshes) {
            ctx.render_meshes(*ctx.world, *face_cmd);
        }
        face_cmd->EndRenderPass();

        rhi_device->Submit(face_cmd);
        auto readback = rhi_device->ReadRenderTargetColorRgba8WithSize(cubemap_rt);
        if (!readback.pixels.empty()) {
            IntegrateFaceSH(readback.pixels.data(), readback.width, readback.height, face, sh);
        }
    }

    // 归一化（6 面总立体角 = 4π, 单位球面积）
    float norm = (4.0f * 3.14159265f) / (6.0f * face_resolution * face_resolution);
    for (int i = 0; i < 9; ++i) {
        sh.coeffs[i] *= norm;
    }

    return sh;
}

// ============================================================================
// 对所有 needs_rebake 的 probe 执行 bake
// ============================================================================
void LightProbeSystem::BakePendingProbes(World& world, RhiDevice* rhi_device,
                                          RenderPassContext& ctx) {
    if (!initialized_) return;

    auto probe_view = world.registry().view<TransformComponent, dse::LightProbeComponent>();
    bool any_baked = false;

    for (auto entity : probe_view) {
        auto& probe = probe_view.get<dse::LightProbeComponent>(entity);
        if (!probe.enabled || !probe.needs_rebake) continue;

        auto& transform = probe_view.get<TransformComponent>(entity);
        SHL2 sh = BakeSHAtPosition(transform.position, face_resolution_,
                                    rhi_device, cubemap_rt_, ctx, &skybox_renderer_);

        // 写回 ECS 组件
        for (int i = 0; i < 9; ++i) {
            probe.sh_coefficients[i] = sh.coeffs[i];
        }
        probe.needs_rebake = false;

        // 更新/添加到缓存列表
        bool found = false;
        for (auto& bp : baked_probes_) {
            if (glm::distance(bp.position, transform.position) < 0.01f) {
                bp.sh = sh;
                bp.influence_radius = probe.influence_radius;
                found = true;
                break;
            }
        }
        if (!found) {
            baked_probes_.push_back({transform.position, probe.influence_radius, sh});
        }

        any_baked = true;
        DEBUG_LOG_INFO("[LightProbeSystem] Baked probe at ({:.1f},{:.1f},{:.1f})",
                       transform.position.x, transform.position.y, transform.position.z);
    }

    if (any_baked) {
        DEBUG_LOG_INFO("[LightProbeSystem] Total baked probes: {}", baked_probes_.size());
    }
}

// ============================================================================
// 运行时 SH 查询
// ============================================================================
void LightProbeSystem::UpdateGlobalSH(World& world, RhiDevice* rhi_device,
                                       const glm::vec3& camera_position) {
    if (!rhi_device) return;

    // 收集所有 enabled 的 probe
    auto probe_view = world.registry().view<TransformComponent, dse::LightProbeComponent>();
    float best_dist = std::numeric_limits<float>::max();
    float second_dist = std::numeric_limits<float>::max();
    const dse::LightProbeComponent* best_probe = nullptr;
    const dse::LightProbeComponent* second_probe = nullptr;

    for (auto entity : probe_view) {
        auto& probe = probe_view.get<dse::LightProbeComponent>(entity);
        if (!probe.enabled) continue;

        auto& transform = probe_view.get<TransformComponent>(entity);
        float dist = glm::distance(transform.position, camera_position);

        if (dist < best_dist) {
            second_dist = best_dist;
            second_probe = best_probe;
            best_dist = dist;
            best_probe = &probe;
        } else if (dist < second_dist) {
            second_dist = dist;
            second_probe = &probe;
        }
    }

    if (!best_probe) {
        // 无 probe，禁用 SH
        glm::vec4 zero_sh[9] = {};
        rhi_device->SetGlobalLightProbeSH(zero_sh, false);
        return;
    }

    // 计算最终 SH（可选距离混合）
    glm::vec4 final_sh[9] = {};

    if (second_probe && best_dist < best_probe->influence_radius &&
        second_dist < second_probe->influence_radius) {
        // 两个 probe 距离加权混合
        float total = best_dist + second_dist;
        if (total < 0.001f) total = 0.001f;
        float w1 = 1.0f - best_dist / total;
        float w2 = 1.0f - second_dist / total;
        float wsum = w1 + w2;
        w1 /= wsum;
        w2 /= wsum;

        for (int i = 0; i < 9; ++i) {
            glm::vec3 blended = best_probe->sh_coefficients[i] * w1 +
                                second_probe->sh_coefficients[i] * w2;
            final_sh[i] = glm::vec4(blended, 0.0f);
        }
    } else {
        // 单 probe
        for (int i = 0; i < 9; ++i) {
            final_sh[i] = glm::vec4(best_probe->sh_coefficients[i], 0.0f);
        }
    }

    rhi_device->SetGlobalLightProbeSH(final_sh, true);
}

} // namespace render
} // namespace dse

/**
 * @file reflection_probe_system.cpp
 * @brief Reflection Probe 系统实现 — cubemap bake + 预滤波 + BRDF LUT
 */

#include "engine/render/reflection_probe_system.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <limits>
#include <fstream>
#include <filesystem>

namespace dse {
namespace render {

// cubemap 6 面方向与 up 向量
static const glm::vec3 kFaceDirections[6] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1},
};
static const glm::vec3 kFaceUps[6] = {
    {0,-1, 0}, {0,-1, 0},
    {0, 0, 1}, {0, 0,-1},
    {0,-1, 0}, {0,-1, 0},
};

static constexpr float PI = 3.14159265358979f;

// ============================================================================
// BRDF Integration LUT（CPU 端 Monte Carlo 积分）
// ============================================================================

static float RadicalInverse_VdC(unsigned int bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return static_cast<float>(bits) * 2.3283064365386963e-10f;
}

static glm::vec2 Hammersley(unsigned int i, unsigned int N) {
    return glm::vec2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
}

static glm::vec3 ImportanceSampleGGX(glm::vec2 Xi, glm::vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0f * PI * Xi.x;
    float cos_theta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);

    glm::vec3 H(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);

    glm::vec3 up = std::abs(N.z) < 0.999f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
    glm::vec3 tangent = glm::normalize(glm::cross(up, N));
    glm::vec3 bitangent = glm::cross(N, tangent);

    return glm::normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

static float GeometrySchlickGGX_IBL(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

static float GeometrySmith_IBL(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX_IBL(NdotV, roughness) * GeometrySchlickGGX_IBL(NdotL, roughness);
}

static glm::vec2 IntegrateBRDF(float NdotV, float roughness) {
    glm::vec3 V(std::sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    glm::vec3 N(0, 0, 1);

    float A = 0.0f, B = 0.0f;
    const unsigned int SAMPLE_COUNT = 256u;

    for (unsigned int i = 0u; i < SAMPLE_COUNT; ++i) {
        glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);

        float NdotL = std::max(L.z, 0.0f);
        float NdotH = std::max(H.z, 0.0f);
        float VdotH = std::max(glm::dot(V, H), 0.0f);

        if (NdotL > 0.0f) {
            float G = GeometrySmith_IBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = std::pow(1.0f - VdotH, 5.0f);
            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    return glm::vec2(A, B) / static_cast<float>(SAMPLE_COUNT);
}

static constexpr int kBrdfLutSize = 128;
static constexpr const char* kBrdfLutCachePath = "data/brdf_lut.cache";

void ReflectionProbeSystem::GenerateBRDFLUT(RhiDevice* rhi_device) {
    const int lut_size = kBrdfLutSize;
    const size_t data_bytes = lut_size * lut_size * 4;
    std::vector<unsigned char> pixels(data_bytes);

    // 尝试从磁盘缓存加载
    bool loaded_from_cache = false;
    {
        std::ifstream fin(kBrdfLutCachePath, std::ios::binary);
        if (fin.is_open()) {
            int cached_size = 0;
            fin.read(reinterpret_cast<char*>(&cached_size), sizeof(cached_size));
            if (cached_size == lut_size) {
                fin.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(data_bytes));
                if (fin.good()) {
                    loaded_from_cache = true;
                    DEBUG_LOG_INFO("[ReflectionProbeSystem] BRDF LUT loaded from cache");
                }
            }
        }
    }

    if (!loaded_from_cache) {
        for (int y = 0; y < lut_size; ++y) {
            for (int x = 0; x < lut_size; ++x) {
                float NdotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(lut_size);
                float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(lut_size);
                NdotV = std::max(NdotV, 0.001f);

                glm::vec2 brdf = IntegrateBRDF(NdotV, roughness);

                int idx = (y * lut_size + x) * 4;
                pixels[idx + 0] = static_cast<unsigned char>(std::min(brdf.x * 255.0f, 255.0f));
                pixels[idx + 1] = static_cast<unsigned char>(std::min(brdf.y * 255.0f, 255.0f));
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 255;
            }
        }

        // 写入磁盘缓存
        try {
            std::filesystem::create_directories(std::filesystem::path(kBrdfLutCachePath).parent_path());
            std::ofstream fout(kBrdfLutCachePath, std::ios::binary);
            if (fout.is_open()) {
                fout.write(reinterpret_cast<const char*>(&lut_size), sizeof(lut_size));
                fout.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(data_bytes));
                DEBUG_LOG_INFO("[ReflectionProbeSystem] BRDF LUT cached to disk");
            }
        } catch (...) {}
    }

    brdf_lut_handle_ = rhi_device->CreateTexture2D(lut_size, lut_size, pixels.data(), true);
    DEBUG_LOG_INFO("[ReflectionProbeSystem] BRDF LUT generated: {}x{}, handle={}{}",
                   lut_size, lut_size, brdf_lut_handle_,
                   loaded_from_cache ? " (cached)" : " (computed)");
}

// ============================================================================
// Init / Shutdown
// ============================================================================

void ReflectionProbeSystem::Init(RhiDevice* rhi_device) {
    if (initialized_ || !rhi_device) return;

    GenerateBRDFLUT(rhi_device);

    RenderTargetDesc desc;
    desc.width = bake_resolution_;
    desc.height = bake_resolution_;
    desc.has_color = true;
    desc.has_depth = true;
    bake_rt_ = rhi_device->CreateRenderTarget(desc);

    initialized_ = true;
    DEBUG_LOG_INFO("[ReflectionProbeSystem] Initialized, bake_res={}", bake_resolution_);
}

void ReflectionProbeSystem::Shutdown(RhiDevice* rhi_device) {
    if (brdf_lut_handle_ != 0 && rhi_device) {
        rhi_device->DeleteTexture(brdf_lut_handle_);
        brdf_lut_handle_ = 0;
    }
    for (auto& entry : baked_cubemaps_) {
        if (entry.prefiltered_cubemap != 0 && rhi_device) {
            rhi_device->DeleteTexture(entry.prefiltered_cubemap);
        }
    }
    baked_cubemaps_.clear();
    bake_rt_ = 0;
    initialized_ = false;
}

// ============================================================================
// Bake
// ============================================================================

void ReflectionProbeSystem::BakePendingProbes(World& world, RhiDevice* rhi_device,
                                               RenderPassContext& ctx) {
    if (!initialized_) return;

    auto probe_view = world.registry().view<TransformComponent, dse::ReflectionProbeComponent>();

    for (auto entity : probe_view) {
        auto& probe = probe_view.get<dse::ReflectionProbeComponent>(entity);
        if (!probe.enabled || !probe.needs_rebake) continue;

        auto& transform = probe_view.get<TransformComponent>(entity);
        const int res = probe.resolution > 0 ? probe.resolution : bake_resolution_;

        // 渲染 6 面并回读
        const glm::mat4 proj = rhi_device->GetProjectionCorrection() *
                               glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 500.0f);

        std::vector<unsigned char> face_pixels[6];
        int face_w = 0, face_h = 0;

        for (int face = 0; face < 6; ++face) {
            glm::mat4 view = glm::lookAt(transform.position,
                                          transform.position + kFaceDirections[face],
                                          kFaceUps[face]);
            auto face_cmd = rhi_device->CreateCommandBuffer();
            face_cmd->BeginRenderPass({bake_rt_, glm::vec4(0.0f), true});
            face_cmd->SetCamera(view, proj);
            face_cmd->SetPipelineState(ctx.pipeline_states.mesh);

            auto skybox_view = ctx.world->registry().view<dse::SkyboxComponent>();
            for (auto sky_entity : skybox_view) {
                auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
                if (skybox.enabled && skybox.cubemap_handle != 0) {
                    face_cmd->DrawSkybox(skybox.cubemap_handle);
                }
                break;
            }

            if (ctx.render_meshes) {
                ctx.render_meshes(*ctx.world, *face_cmd);
            }
            face_cmd->EndRenderPass();

            rhi_device->Submit(face_cmd);
            auto readback = rhi_device->ReadRenderTargetColorRgba8WithSize(bake_rt_);
            face_pixels[face] = std::move(readback.pixels);
            face_w = readback.width;
            face_h = readback.height;
        }

        // 创建 cubemap 纹理
        if (face_w > 0 && face_h > 0) {
            const unsigned char* faces[6] = {
                face_pixels[0].data(), face_pixels[1].data(),
                face_pixels[2].data(), face_pixels[3].data(),
                face_pixels[4].data(), face_pixels[5].data()
            };
            unsigned int cubemap = rhi_device->CreateTextureCube(face_w, face_h, faces, true);
            probe.cubemap_handle = cubemap;
            probe.needs_rebake = false;

            // 缓存条目
            bool found = false;
            for (auto& entry : baked_cubemaps_) {
                if (glm::distance(entry.position, transform.position) < 0.01f) {
                    if (entry.prefiltered_cubemap != 0) {
                        rhi_device->DeleteTexture(entry.prefiltered_cubemap);
                    }
                    entry.prefiltered_cubemap = cubemap;
                    entry.influence_radius = probe.influence_radius;
                    found = true;
                    break;
                }
            }
            if (!found) {
                baked_cubemaps_.push_back({transform.position, probe.influence_radius, cubemap});
            }

            DEBUG_LOG_INFO("[ReflectionProbeSystem] Baked probe at ({:.1f},{:.1f},{:.1f}), cubemap={}",
                           transform.position.x, transform.position.y, transform.position.z, cubemap);
        }
    }
}

// ============================================================================
// 运行时查询
// ============================================================================

unsigned int ReflectionProbeSystem::QueryNearestProbeCubemap(World& world,
                                                              const glm::vec3& position) const {
    auto probe_view = world.registry().view<TransformComponent, dse::ReflectionProbeComponent>();
    float best_dist = std::numeric_limits<float>::max();
    unsigned int best_cubemap = 0;

    for (auto entity : probe_view) {
        auto& probe = probe_view.get<dse::ReflectionProbeComponent>(entity);
        if (!probe.enabled || probe.cubemap_handle == 0) continue;

        auto& transform = probe_view.get<TransformComponent>(entity);
        float dist = glm::distance(transform.position, position);
        if (dist < probe.influence_radius && dist < best_dist) {
            best_dist = dist;
            best_cubemap = probe.cubemap_handle;
        }
    }

    return best_cubemap;
}

} // namespace render
} // namespace dse

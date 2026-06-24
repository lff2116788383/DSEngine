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
#include <array>
#include <cmath>
#include <cstring>
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

// ============================================================================
// CPU 立方体采样 + 预滤波（IBL prefiltered env，无 GPU 依赖）
// ============================================================================

// 由 face(0..5,顺序 +X,-X,+Y,-Y,+Z,-Z) 与归一化 uv∈[0,1] 还原采样方向（OpenGL 立方
// 体约定的逆映射）。
static glm::vec3 CubeDirFromFaceUV(int face, float u, float v) {
    float sc = 2.0f * u - 1.0f;
    float tc = 2.0f * v - 1.0f;
    glm::vec3 dir;
    switch (face) {
        case 0: dir = glm::vec3( 1.0f,  -tc,  -sc); break;  // +X
        case 1: dir = glm::vec3(-1.0f,  -tc,   sc); break;  // -X
        case 2: dir = glm::vec3(  sc,  1.0f,   tc); break;  // +Y
        case 3: dir = glm::vec3(  sc, -1.0f,  -tc); break;  // -Y
        case 4: dir = glm::vec3(  sc,  -tc,  1.0f); break;  // +Z
        default:dir = glm::vec3( -sc,  -tc, -1.0f); break;  // -Z
    }
    return glm::normalize(dir);
}

// 方向 → face + uv（OpenGL 立方体约定），用于 CPU 端采样 base 面。
static void CubeFaceUVFromDir(const glm::vec3& d, int& face, float& u, float& v) {
    float ax = std::abs(d.x), ay = std::abs(d.y), az = std::abs(d.z);
    float sc, tc, ma;
    if (ax >= ay && ax >= az) {
        ma = ax;
        if (d.x > 0.0f) { face = 0; sc = -d.z; tc = -d.y; }
        else            { face = 1; sc =  d.z; tc = -d.y; }
    } else if (ay >= ax && ay >= az) {
        ma = ay;
        if (d.y > 0.0f) { face = 2; sc =  d.x; tc =  d.z; }
        else            { face = 3; sc =  d.x; tc = -d.z; }
    } else {
        ma = az;
        if (d.z > 0.0f) { face = 4; sc =  d.x; tc = -d.y; }
        else            { face = 5; sc = -d.x; tc = -d.y; }
    }
    ma = std::max(ma, 1e-6f);
    u = (sc / ma + 1.0f) * 0.5f;
    v = (tc / ma + 1.0f) * 0.5f;
}

// 双线性采样 base 立方体的某个方向，返回线性 [0,1] RGB。
static glm::vec3 SampleCubeBilinear(const unsigned char* const faces[6], int res, const glm::vec3& dir) {
    int face;
    float u, v;
    CubeFaceUVFromDir(dir, face, u, v);
    float fx = u * static_cast<float>(res) - 0.5f;
    float fy = v * static_cast<float>(res) - 0.5f;
    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    float tx = fx - static_cast<float>(x0);
    float ty = fy - static_cast<float>(y0);
    auto clampi = [res](int c) { return std::min(std::max(c, 0), res - 1); };
    auto texel = [&](int x, int y) -> glm::vec3 {
        size_t idx = (static_cast<size_t>(clampi(y)) * res + clampi(x)) * 4;
        const unsigned char* p = faces[face] + idx;
        return glm::vec3(p[0], p[1], p[2]) * (1.0f / 255.0f);
    };
    glm::vec3 c00 = texel(x0, y0), c10 = texel(x0 + 1, y0);
    glm::vec3 c01 = texel(x0, y0 + 1), c11 = texel(x0 + 1, y0 + 1);
    return glm::mix(glm::mix(c00, c10, tx), glm::mix(c01, c11, tx), ty);
}

// 对方向 N 在 base 立方体上做 GGX 重要性采样卷积（split-sum prefilter）。
static glm::vec3 PrefilterDirection(const unsigned char* const faces[6], int res,
                                    const glm::vec3& N, float roughness) {
    if (roughness <= 0.0f) {
        return SampleCubeBilinear(faces, res, N);
    }
    const glm::vec3 R = N;
    const glm::vec3 V = N;
    const unsigned int SAMPLE_COUNT = 64u;
    glm::vec3 prefiltered(0.0f);
    float total_weight = 0.0f;
    for (unsigned int i = 0u; i < SAMPLE_COUNT; ++i) {
        glm::vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        glm::vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
        float NdotL = std::max(glm::dot(N, L), 0.0f);
        if (NdotL > 0.0f) {
            prefiltered += SampleCubeBilinear(faces, res, L) * NdotL;
            total_weight += NdotL;
        }
    }
    return total_weight > 0.0f ? prefiltered / total_weight : SampleCubeBilinear(faces, res, R);
}

ReflectionProbeSystem::PrefilteredCube
ReflectionProbeSystem::ComputePrefilteredCube(const unsigned char* const faces[6], int res) {
    PrefilteredCube out;
    if (res <= 0 || !faces) return out;
    out.base_resolution = res;
    const int num_mips = 1 + static_cast<int>(std::floor(std::log2(static_cast<double>(res))));
    out.num_mips = num_mips;
    out.mips.resize(static_cast<size_t>(num_mips));

    for (int mip = 0; mip < num_mips; ++mip) {
        const int mres = std::max(1, res >> mip);
        const float roughness = std::min(1.0f, static_cast<float>(mip) / kMaxReflectionLod);
        auto& mip_faces = out.mips[static_cast<size_t>(mip)];
        for (int f = 0; f < 6; ++f) {
            mip_faces[f].resize(static_cast<size_t>(mres) * mres * 4);
        }
        if (mip == 0) {
            // mip0 = 锐利 base（roughness 0），直接拷贝
            for (int f = 0; f < 6; ++f) {
                std::memcpy(mip_faces[f].data(), faces[f], static_cast<size_t>(res) * res * 4);
            }
            continue;
        }
        for (int f = 0; f < 6; ++f) {
            for (int y = 0; y < mres; ++y) {
                for (int x = 0; x < mres; ++x) {
                    float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(mres);
                    float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(mres);
                    glm::vec3 N = CubeDirFromFaceUV(f, u, v);
                    glm::vec3 c = PrefilterDirection(faces, res, N, roughness);
                    size_t idx = (static_cast<size_t>(y) * mres + x) * 4;
                    mip_faces[f][idx + 0] = static_cast<unsigned char>(std::min(c.r, 1.0f) * 255.0f + 0.5f);
                    mip_faces[f][idx + 1] = static_cast<unsigned char>(std::min(c.g, 1.0f) * 255.0f + 0.5f);
                    mip_faces[f][idx + 2] = static_cast<unsigned char>(std::min(c.b, 1.0f) * 255.0f + 0.5f);
                    mip_faces[f][idx + 3] = 255;
                }
            }
        }
    }
    return out;
}

unsigned int ReflectionProbeSystem::PrefilterAndUploadCubemap(RhiDevice* rhi_device,
                                                              const unsigned char* const faces[6], int res) {
    if (!rhi_device || res <= 0) return 0;
    PrefilteredCube data = ComputePrefilteredCube(faces, res);
    if (data.num_mips <= 0) return 0;

    std::vector<CubeMipLevel> levels(static_cast<size_t>(data.num_mips));
    for (int mip = 0; mip < data.num_mips; ++mip) {
        const int mres = std::max(1, res >> mip);
        CubeMipLevel& lvl = levels[static_cast<size_t>(mip)];
        lvl.width = mres;
        lvl.height = mres;
        for (int f = 0; f < 6; ++f) {
            lvl.faces[f] = data.mips[static_cast<size_t>(mip)][f].data();
        }
    }
    return rhi_device->CreateTextureCubeWithMips(levels, true);
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
            const dse::render::FrameContext face_frame{view, proj};

            // 渲染天空盒（通用绘制原语，自带天空盒 PSO）
            auto skybox_view = ctx.world->registry().view<dse::SkyboxComponent>();
            for (auto sky_entity : skybox_view) {
                auto& skybox = skybox_view.get<dse::SkyboxComponent>(sky_entity);
                if (skybox.enabled && skybox.cubemap_handle != 0) {
                    skybox_renderer_.Draw(*face_cmd, *rhi_device, skybox.cubemap_handle, view, proj);
                }
                break;
            }

            face_cmd->BindPipeline(ctx.pipeline_states.mesh);
            if (ctx.render_meshes) {
                ctx.render_meshes(*ctx.world, *face_cmd, face_frame);
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
            // A3：CPU 预滤波 base 6 面 → 上传带 mip 链的 prefiltered env cubemap，运行时
            // PBR 以 textureLod(roughness*MAX_LOD) 采样。立方体 mip 采样 ES3.0/WebGL2
            // 原生支持，无 compute/SSBO 需求。
            unsigned int cubemap = PrefilterAndUploadCubemap(rhi_device, faces, face_w);
            if (cubemap == 0) {
                cubemap = rhi_device->CreateTextureCube(face_w, face_h, faces, true);
            }
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

/**
 * @file gl_shader_manager.cpp
 * @brief GLShaderManager 实现 - 着色器管理器
 */

#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/ubo_types.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
#include <cstdio>
#include <cstring>

// 内嵌的 GLSL 330 着色器源码
#include "embed/pbr_vert.gen.h"
#include "embed/pbr_frag.gen.h"
#include "embed/skybox_vert.gen.h"
#include "embed/skybox_frag.gen.h"
#include "embed/particle_vert.gen.h"
#include "embed/particle_frag.gen.h"
#include "embed/postprocess_vert.gen.h"
#include "embed/postprocess_passthrough_frag.gen.h"
#include "embed/bloom_extract_frag.gen.h"
#include "embed/bloom_downsample_frag.gen.h"
#include "embed/bloom_upsample_frag.gen.h"
#include "embed/bloom_blur_h_frag.gen.h"
#include "embed/bloom_blur_v_frag.gen.h"
#include "embed/fxaa_frag.gen.h"
#include "embed/tonemapping_frag.gen.h"
#include "embed/color_grading_frag.gen.h"
#include "embed/edge_detect_frag.gen.h"
#include "embed/bloom_composite_ssao_ae_frag.gen.h"
#include "embed/ssao_apply_frag.gen.h"
#include "embed/ssao_frag.gen.h"
#include "embed/ssao_blur_frag.gen.h"
#include "embed/contact_shadow_frag.gen.h"
#include "embed/dof_frag.gen.h"
#include "embed/motion_vector_frag.gen.h"
#include "embed/motion_blur_frag.gen.h"
#include "embed/ssr_frag.gen.h"
#include "embed/taa_resolve_frag.gen.h"
#include "embed/deferred_lighting_frag.gen.h"
#include "embed/light_shaft_frag.gen.h"
#include "embed/volumetric_fog_frag.gen.h"
#include "embed/decal_frag.gen.h"
#include "embed/water_frag.gen.h"
#include "embed/wboit_composite_frag.gen.h"
#include "embed/lum_compute_frag.gen.h"
#include "embed/lum_adapt_frag.gen.h"
#include "embed/gbuffer_frag.gen.h"
#include "embed/sprite_vert.gen.h"
#include "embed/sprite_frag.gen.h"
#include "embed/shadow_vert.gen.h"
#include "embed/shadow_frag.gen.h"

// Reflection metadata for automated binding
#include "embed/pbr_vert_reflect.gen.h"
#include "embed/pbr_frag_reflect.gen.h"
#include "embed/shadow_vert_reflect.gen.h"
#include "embed/shadow_frag_reflect.gen.h"
#include "embed/sprite_vert_reflect.gen.h"
#include "embed/sprite_frag_reflect.gen.h"
#include "embed/gbuffer_frag_reflect.gen.h"
#include "engine/render/shader_reflection.h"

namespace dse {
namespace render {

// ============================================================
// 着色器编译和管理
// ============================================================

unsigned int GLShaderManager::CompileProgram(const char* vertex_src, const char* fragment_src) {
    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
    glCompileShader(vertex_shader);
    int vertex_compiled = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_compiled);
    if (vertex_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(vertex_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL vertex shader compile failed: {}", log_buffer);
    }

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
    glCompileShader(fragment_shader);
    int fragment_compiled = 0;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_compiled);
    if (fragment_compiled == GL_FALSE) {
        char log_buffer[1024];
        glGetShaderInfoLog(fragment_shader, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL fragment shader compile failed: {}", log_buffer);
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    int linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char log_buffer[1024];
        glGetProgramInfoLog(program, sizeof(log_buffer), nullptr, log_buffer);
        DEBUG_LOG_ERROR("OpenGL shader link failed: {}", log_buffer);
    }
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

void GLShaderManager::DeleteProgram(unsigned int handle) {
    if (handle == 0) return;
    glDeleteProgram(handle);
    programs_destroyed_ += 1;
}

// ============================================================
// GL 3.3 UBO Fallback：从 SSBO GLSL 430 生成 GLSL 330 UBO 变体
//
// 所有操作均按 block/struct 名称定位，不依赖 spirv-cross 自动分配的
// OpVariable ID（如 _1904、_1996），因此 shader 改动后 ID 漂移时仍稳定。
// ============================================================

// 按名称移除一个 SSBO block 声明（ID 无关）
static bool RemoveSSBOBlock(std::string& src, const char* block_name) {
    const std::string marker = std::string("buffer ") + block_name + "\n";
    const auto blk = src.find(marker);
    if (blk == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] block not found: %s\n", block_name);
        return false;
    }
    const auto layout_start = src.rfind("layout(", blk);
    if (layout_start == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] layout( before block not found: %s\n", block_name);
        return false;
    }
    const auto close = src.find("\n}", blk);
    if (close == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] closing } not found: %s\n", block_name);
        return false;
    }
    auto end = src.find('\n', close + 2); // 跳过 "} _NNN;"
    if (end == std::string::npos) end = src.size();
    else end++;                            // 包含换行
    while (end < src.size() && src[end] == '\n') end++; // 消耗尾部空行
    src.erase(layout_start, end - layout_start);
    return true;
}

// 将 SSBO 块声明原地转换为固定大小 UBO 块（ID 无关）
static bool TransformSSBOToUBO(std::string& src, const char* ssbo_name, const char* ubo_name,
                                const char* array_field, int max_count) {
    const std::string old_decl = std::string("std430) readonly buffer ") + ssbo_name;
    const std::string new_decl = std::string("std140) uniform ") + ubo_name;
    auto pos = src.find(old_decl);
    if (pos == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] SSBO decl not found: %s\n", ssbo_name);
        return false;
    }
    src.replace(pos, old_decl.size(), new_decl);

    const std::string old_arr = std::string(array_field) + "[];";
    const std::string new_arr = std::string(array_field) + "[" + std::to_string(max_count) + "];";
    pos = src.find(old_arr);
    if (pos == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] array field not found: %s\n", array_field);
        return false;
    }
    src.replace(pos, old_arr.size(), new_arr);
    return true;
}

// 动态提取 UBO 块的 spirv-cross 实例名（如 "_2008"），随 shader 变动而变动
static std::string ExtractInstanceName(const std::string& src, const char* ubo_name) {
    const std::string marker = std::string("uniform ") + ubo_name + "\n";
    const auto blk = src.find(marker);
    if (blk == std::string::npos) return {};
    const auto close = src.find("\n} ", blk); // "\n} _NNN;"
    if (close == std::string::npos) return {};
    const auto name_start = close + 3;        // 跳过 "\n} "
    const auto semi = src.find(';', name_start);
    if (semi == std::string::npos) return {};
    return src.substr(name_start, semi - name_start);
}

// 将 Clustered Forward+ 点光源循环替换为暴力遍历（ID 无关）
// 定位依据：结构标记 "int cl_tx" 和 "for (uint ci = 0u;"，不依赖任何 _NNN ID
static bool ReplacePointLoopCluster(std::string& src, const std::string& point_inst) {
    const auto preamble = src.find("    int cl_tx = int(gl_FragCoord.x)");
    if (preamble == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] cl_tx preamble not found\n");
        return false;
    }
    const auto for_kw = src.find("    for (uint ci = 0u;", preamble);
    if (for_kw == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] point cluster for-loop not found\n");
        return false;
    }
    const auto body = src.find("    {\n", for_kw); // for 循环体开始 "    {\n"
    if (body == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] point loop body { not found\n");
        return false;
    }
    // 循环体内第一个 "        }\n" 是 if-guard 的闭合括号
    const auto guard_end = src.find("        }\n", body + 5);
    if (guard_end == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] point loop guard } not found\n");
        return false;
    }
    const auto remove_end = guard_end + 10; // "        }\n" = 10 chars
    const std::string new_loop =
        "    for (int i = 0; i < " + point_inst + ".u_point_light_count; i++)\n    {\n";
    src.replace(preamble, remove_end - preamble, new_loop);
    return true;
}

// 将聚光灯循环头替换为暴力遍历（ID 无关）
// 定位依据：结构标记 "for (uint si = 0u;"，不依赖任何 _NNN ID
static bool ReplaceSpotLoopHeader(std::string& src, const std::string& spot_inst) {
    const auto for_kw = src.find("    for (uint si = 0u;");
    if (for_kw == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] spot cluster for-loop not found\n");
        return false;
    }
    const auto body = src.find("    {\n", for_kw);
    if (body == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] spot loop body { not found\n");
        return false;
    }
    const auto guard_end = src.find("        }\n", body + 5);
    if (guard_end == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] spot loop guard } not found\n");
        return false;
    }
    const auto remove_end = guard_end + 10;
    const std::string new_loop =
        "    for (int i_1 = 0; i_1 < " + spot_inst + ".u_spot_light_count; i_1++)\n    {\n";
    src.replace(for_kw, remove_end - for_kw, new_loop);
    return true;
}

std::string GLShaderManager::GenerateUBOGLSL() {
    using namespace dse::render::generated_shaders;
    std::string src = kpbr_frag_glsl330;

    // 1. 版本降级（#version 430 → #version 330）
    {
        const auto pos = src.find("#version 430");
        if (pos == std::string::npos)
            fprintf(stderr, "[GenerateUBOGLSL] #version 430 not found\n");
        else
            src.replace(pos, 12, "#version 330");
    }

    // 2. 移除 ClusterInfoEntry 结构体（按名称定位，ID 无关）
    {
        const auto s = src.find("struct ClusterInfoEntry\n");
        if (s == std::string::npos) {
            fprintf(stderr, "[GenerateUBOGLSL] struct ClusterInfoEntry not found\n");
        } else {
            auto e = src.find("};\n", s);
            if (e == std::string::npos) {
                fprintf(stderr, "[GenerateUBOGLSL] ClusterInfoEntry end not found\n");
            } else {
                e += 3;
                if (e < src.size() && src[e] == '\n') e++;
                src.erase(s, e - s);
            }
        }
    }

    // 3 & 4. 移除 ClusterInfoSSBO + LightIndexSSBO（按名称定位，ID 无关）
    RemoveSSBOBlock(src, "ClusterInfoSSBO");
    RemoveSSBOBlock(src, "LightIndexSSBO");

    // 5 & 6. PointLightSSBO + SpotLightSSBO → 固定大小 UBO（按名称定位，ID 无关）
    TransformSSBOToUBO(src, "PointLightSSBO", "PointLightUBO", "u_point_lights[]", kMaxUBOLights);
    TransformSSBOToUBO(src, "SpotLightSSBO",  "SpotLightUBO",  "u_spot_lights[]",  kMaxUBOLights);

    // 7. 动态提取实例名（随 shader 变动自动适应，不再硬编码 _2008/_2190）
    const std::string point_inst = ExtractInstanceName(src, "PointLightUBO");
    const std::string spot_inst  = ExtractInstanceName(src, "SpotLightUBO");
    if (point_inst.empty())
        fprintf(stderr, "[GenerateUBOGLSL] PointLightUBO instance name not found\n");
    if (spot_inst.empty())
        fprintf(stderr, "[GenerateUBOGLSL] SpotLightUBO instance name not found\n");

    // 8 & 9. 替换 Clustered Forward+ 循环为暴力遍历（按结构标记定位，ID 无关）
    ReplacePointLoopCluster(src, point_inst);
    ReplaceSpotLoopHeader(src, spot_inst);

    return src;
}

// ============================================================
// 内置 PBR 着色器
// ============================================================

void GLShaderManager::InitBuiltinPBRShader() {
    using namespace dse::render::generated_shaders;
    if (!supports_ssbo_) {
        static std::string ubo_frag_src = GenerateUBOGLSL();
        pbr_shader_handle_ = CompileProgram(kpbr_vert_glsl330, ubo_frag_src.c_str());
    } else {
        pbr_shader_handle_ = CompileProgram(kpbr_vert_glsl330, kpbr_frag_glsl330);
    }
    programs_created_ += 1;
    CachePBRLocations();
}

// UBO name → engine UBOBindingPoint 映射表（用于 reflection 自动绑定）
static UBOBindingPoint MapUBONameToBindingPoint(const char* name) {
    if (std::strcmp(name, "PerFrame") == 0)       return UBOBindingPoint::PerFrame;
    if (std::strcmp(name, "PerScene") == 0)       return UBOBindingPoint::PerScene;
    if (std::strcmp(name, "PerMaterial") == 0)    return UBOBindingPoint::PerMaterial;
    if (std::strcmp(name, "PointLightUBO") == 0)  return UBOBindingPoint::PointLights;
    if (std::strcmp(name, "SpotLightUBO") == 0)   return UBOBindingPoint::SpotLights;
    if (std::strcmp(name, "SpotLightData") == 0)  return UBOBindingPoint::SpotLightData;
    if (std::strcmp(name, "BoneMatrices") == 0)   return UBOBindingPoint::BoneMatrices;
    if (std::strcmp(name, "MorphWeights") == 0)   return UBOBindingPoint::MorphWeights;
    if (std::strcmp(name, "LightProbeData") == 0) return UBOBindingPoint::LightProbeData;
    return static_cast<UBOBindingPoint>(0xFF); // unknown — skipped
}

// 从 reflection 数据自动绑定单个 stage 的所有 UBO block
static void BindUBOsFromReflection(unsigned int prog,
                                    const shader_reflect::StageReflection& refl) {
    for (uint32_t i = 0; i < refl.uniform_buffer_count; ++i) {
        const auto& ubo = refl.uniform_buffers[i];
        UBOBindingPoint bp = MapUBONameToBindingPoint(ubo.name);
        if (static_cast<uint32_t>(bp) == 0xFF) continue;
        unsigned int idx = glGetUniformBlockIndex(prog, ubo.name);
        if (idx != GL_INVALID_INDEX)
            glUniformBlockBinding(prog, idx, static_cast<unsigned int>(bp));
    }
}

void GLShaderManager::CachePBRLocations() {
    auto& loc = pbr_locations_;
    unsigned int h = pbr_shader_handle_;

    // --- UBO block 绑定（reflection 驱动）---
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(h, kpbr_vert_reflection);
    BindUBOsFromReflection(h, kpbr_frag_reflection);

    // 缓存 block index 到 locations struct（向后兼容）
    loc.per_frame_block_index = glGetUniformBlockIndex(h, "PerFrame");
    loc.per_scene_block_index = glGetUniformBlockIndex(h, "PerScene");
    loc.per_material_block_index = glGetUniformBlockIndex(h, "PerMaterial");
    if (!supports_ssbo_) {
        loc.point_lights_block_index = glGetUniformBlockIndex(h, "PointLightUBO");
        loc.spot_lights_block_index = glGetUniformBlockIndex(h, "SpotLightUBO");
    } else {
        loc.point_lights_block_index = GL_INVALID_INDEX;
        loc.spot_lights_block_index  = GL_INVALID_INDEX;
    }
    loc.spot_light_data_block_index = glGetUniformBlockIndex(h, "SpotLightData");
    loc.bone_matrices_block_index = glGetUniformBlockIndex(h, "BoneMatrices");
    loc.morph_weights_block_index = glGetUniformBlockIndex(h, "MorphWeights");
    loc.light_probe_data_block_index = glGetUniformBlockIndex(h, "LightProbeData");

    // --- 纹理 unit 自动分配（reflection 驱动，一次性绑定）---
    {
        std::vector<gl_reflect::TextureUnitEntry> tex_entries;
        gl_reflect::ComputeFlatTextureUnits(kpbr_frag_reflection, tex_entries);

        // glUseProgram 必须在 BindSamplersOnce 之前调用
        glUseProgram(h);
        gl_reflect::BindSamplersOnce(h, tex_entries, glGetUniformLocation, glUniform1i);

        // 从计算结果填充 PBRTextureSlots（draw executor 使用）
        auto& slots = pbr_texture_slots_;
        for (const auto& e : tex_entries) {
            int u = static_cast<int>(e.unit);
            if (std::strcmp(e.name, "u_texture") == 0)              slots.albedo = u;
            else if (std::strcmp(e.name, "u_normal_map") == 0)      slots.normal = u;
            else if (std::strcmp(e.name, "u_metallic_roughness_map") == 0) slots.metallic_roughness = u;
            else if (std::strcmp(e.name, "u_emissive_map") == 0)    slots.emissive = u;
            else if (std::strcmp(e.name, "u_occlusion_map") == 0)   slots.occlusion = u;
            else if (std::strcmp(e.name, "u_shadow_maps") == 0)     slots.shadow_base = u;
            else if (std::strcmp(e.name, "u_spot_shadow_maps") == 0) slots.spot_shadow_base = u;
            else if (std::strcmp(e.name, "u_reflection_cubemap") == 0) slots.reflection_cubemap = u;
            else if (std::strcmp(e.name, "u_brdf_lut") == 0)        slots.brdf_lut = u;
            else if (std::strcmp(e.name, "u_splat_weight_map") == 0) slots.splat_weight = u;
            else if (std::strcmp(e.name, "u_splat_layer0") == 0)    slots.splat_layer_base = u;
            else if (std::strcmp(e.name, "u_point_shadow_maps") == 0) slots.point_shadow_base = u;
        }

#ifndef NDEBUG
        // Debug 校验：纹理 slot 无重叠
        shader_reflect_debug::ValidateTextureSlotOverlaps(tex_entries);
        shader_reflect_debug::ValidateUBOBindings(kpbr_frag_reflection, "PBR.frag");
        shader_reflect_debug::ValidateVertexInputs(kpbr_vert_reflection);
#endif
    }

    // --- 缓存 sampler location（向后兼容 draw executor）---
    loc.texture = glGetUniformLocation(h, "u_texture");
    loc.normal_map = glGetUniformLocation(h, "u_normal_map");
    loc.metallic_roughness_map = glGetUniformLocation(h, "u_metallic_roughness_map");
    loc.emissive_map = glGetUniformLocation(h, "u_emissive_map");
    loc.occlusion_map = glGetUniformLocation(h, "u_occlusion_map");
    for (int i = 0; i < 3; ++i) {
        std::string sm_name = "u_shadow_maps[" + std::to_string(i) + "]";
        loc.shadow_map[i] = glGetUniformLocation(h, sm_name.c_str());
    }
    for (int i = 0; i < 4; ++i) {
        std::string name = "u_point_shadow_maps[" + std::to_string(i) + "]";
        loc.point_shadow_map[i] = glGetUniformLocation(h, name.c_str());
    }
    for (int i = 0; i < 4; ++i) {
        std::string name = "u_spot_shadow_maps[" + std::to_string(i) + "]";
        loc.spot_shadow_map[i] = glGetUniformLocation(h, name.c_str());
    }

    // --- Terrain splatmap ---
    loc.splat_weight_map = glGetUniformLocation(h, "u_splat_weight_map");
    loc.splat_layer[0] = glGetUniformLocation(h, "u_splat_layer0");
    loc.splat_layer[1] = glGetUniformLocation(h, "u_splat_layer1");
    loc.splat_layer[2] = glGetUniformLocation(h, "u_splat_layer2");
    loc.splat_layer[3] = glGetUniformLocation(h, "u_splat_layer3");
    loc.splat_enabled = glGetUniformLocation(h, "u_splat_enabled");
    loc.splat_tiling = glGetUniformLocation(h, "u_splat_tiling");

    // --- 逐对象 uniform（从 push constants 展平）-----
    loc.model = glGetUniformLocation(h, "u_model");
    loc.skinned = glGetUniformLocation(h, "u_skinned");
    loc.morph_enabled = glGetUniformLocation(h, "u_morph_enabled");
    loc.use_instancing = glGetUniformLocation(h, "u_use_instancing");
}

// NOTE: Old hand-written PBR shader (~500 lines) removed.
// Now uses offline-compiled GLSL 330 strings (embed/*.gen.h).
// See git history for the old version.

// ============================================================
// 天空盒着色器
// ============================================================

void GLShaderManager::InitSkyboxShader() {
    if (skybox_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    skybox_shader_handle_ = CompileProgram(kskybox_vert_glsl330, kskybox_frag_glsl330);
    programs_created_ += 1;

    skybox_locations_.vp = glGetUniformLocation(skybox_shader_handle_, "u_vp");
    skybox_locations_.tex = glGetUniformLocation(skybox_shader_handle_, "skybox");
}

// ============================================================
// 粒子着色器
// ============================================================

void GLShaderManager::InitParticleShader() {
    if (particle_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    particle_shader_handle_ = CompileProgram(kparticle_vert_glsl330, kparticle_frag_glsl330);
    programs_created_ += 1;

    // Particle shader uses PerFrame UBO for vp/view matrices
    unsigned int pf_idx = glGetUniformBlockIndex(particle_shader_handle_, "PerFrame");
    if (pf_idx != GL_INVALID_INDEX) {
        glUniformBlockBinding(particle_shader_handle_, pf_idx,
                              static_cast<unsigned int>(UBOBindingPoint::PerFrame));
    }
    particle_locations_.per_frame_block_index = pf_idx;
    particle_locations_.texture = glGetUniformLocation(particle_shader_handle_, "u_texture");
}

// ============================================================
// GBuffer 着色器（延迟渲染几何通道）
// ============================================================

void GLShaderManager::InitGBufferShader() {
    if (gbuffer_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;

    gbuffer_shader_handle_ = CompileProgram(kpbr_vert_glsl330, kgbuffer_frag_glsl330);
    programs_created_ += 1;

    // GBuffer UBO 绑定（reflection 驱動）
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(gbuffer_shader_handle_, kpbr_vert_reflection);
    BindUBOsFromReflection(gbuffer_shader_handle_, kgbuffer_frag_reflection);

    // GBuffer 纹理 sampler 一次性绑定
    {
        using namespace dse::render::gl_reflect;
        std::vector<TextureUnitEntry> entries;
        ComputeFlatTextureUnits(kgbuffer_frag_reflection, entries);
        glUseProgram(gbuffer_shader_handle_);
        BindSamplersOnce(gbuffer_shader_handle_, entries, glGetUniformLocation, glUniform1i);
        glUseProgram(0);
    }
}

// ============================================================
// 精灵着色器
// ============================================================

void GLShaderManager::InitSpriteShader() {
    if (sprite_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    sprite_shader_handle_ = CompileProgram(ksprite_vert_glsl330, ksprite_frag_glsl330);
    programs_created_ += 1;

    // Sprite UBO 绑定（reflection 驱动）
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(sprite_shader_handle_, ksprite_vert_reflection);
    BindUBOsFromReflection(sprite_shader_handle_, ksprite_frag_reflection);
}

// ============================================================
// 阴影深度着色器
// ============================================================

void GLShaderManager::InitShadowShader() {
    if (shadow_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    shadow_shader_handle_ = CompileProgram(kshadow_vert_glsl330, kshadow_frag_glsl330);
    programs_created_ += 1;

    // Shadow UBO 绑定（reflection 驱动）
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(shadow_shader_handle_, kshadow_vert_reflection);
    BindUBOsFromReflection(shadow_shader_handle_, kshadow_frag_reflection);
}

// ============================================================
// Post-process shader cache
// ============================================================

unsigned int GLShaderManager::GetOrCreatePostProcessShader(const std::string& effect_name,
                                                            const char* vs_src,
                                                            const std::string& fs_src) {
    auto it = pp_shaders_.find(effect_name);
    if (it != pp_shaders_.end()) {
        return it->second;
    }

    const char* fs_c_str = fs_src.c_str();
    unsigned int shader = CompileProgram(vs_src, fs_c_str);
    programs_created_ += 1;
    pp_shaders_[effect_name] = shader;
    return shader;
}

bool GLShaderManager::HasPostProcessShader(const std::string& effect_name) const {
    return pp_shaders_.find(effect_name) != pp_shaders_.end();
}

unsigned int GLShaderManager::GetOrCreateGenPPShader(const std::string& effect_name) {
    std::string key = "gen_" + effect_name;
    auto it = pp_shaders_.find(key);
    if (it != pp_shaders_.end()) return it->second;

    using namespace dse::render::generated_shaders;
    const char* fs = nullptr;
    if      (effect_name == "fxaa")             fs = kfxaa_frag_glsl330;
    else if (effect_name == "bloom_extract")     fs = kbloom_extract_frag_glsl330;
    else if (effect_name == "bloom_downsample")  fs = kbloom_downsample_frag_glsl330;
    else if (effect_name == "bloom_upsample")    fs = kbloom_upsample_frag_glsl330;
    else if (effect_name == "tonemapping")         fs = ktonemapping_frag_glsl330;
    else if (effect_name == "color_grading")       fs = kcolor_grading_frag_glsl330;
    else if (effect_name == "edge_detect")          fs = kedge_detect_frag_glsl330;
    else if (effect_name == "postprocess_passthrough") fs = kpostprocess_passthrough_frag_glsl330;
    else if (effect_name == "bloom_composite")     fs = kbloom_composite_ssao_ae_frag_glsl330;
    else if (effect_name == "ssao_apply")           fs = kssao_apply_frag_glsl330;
    else if (effect_name == "ssao")                 fs = kssao_frag_glsl330;
    else if (effect_name == "ssao_blur")             fs = kssao_blur_frag_glsl330;
    else if (effect_name == "contact_shadow")        fs = kcontact_shadow_frag_glsl330;
    else if (effect_name == "dof")                   fs = kdof_frag_glsl330;
    else if (effect_name == "motion_vector")         fs = kmotion_vector_frag_glsl330;
    else if (effect_name == "motion_blur")           fs = kmotion_blur_frag_glsl330;
    else if (effect_name == "ssr")                   fs = kssr_frag_glsl330;
    else if (effect_name == "taa_resolve")           fs = ktaa_resolve_frag_glsl330;
    else if (effect_name == "deferred_lighting")     fs = kdeferred_lighting_frag_glsl330;
    else if (effect_name == "light_shaft")           fs = klight_shaft_frag_glsl330;
    else if (effect_name == "volumetric_fog")        fs = kvolumetric_fog_frag_glsl330;
    else if (effect_name == "decal")                 fs = kdecal_frag_glsl330;
    else if (effect_name == "water")                 fs = kwater_frag_glsl330;
    else if (effect_name == "wboit_composite")       fs = kwboit_composite_frag_glsl330;
    else if (effect_name == "lum_compute")           fs = klum_compute_frag_glsl330;
    else if (effect_name == "lum_adapt")             fs = klum_adapt_frag_glsl330;
    else if (effect_name == "bloom_blur_h")          fs = kbloom_blur_h_frag_glsl330;
    else if (effect_name == "bloom_blur_v")          fs = kbloom_blur_v_frag_glsl330;
    else return 0;

    unsigned int shader = CompileProgram(kpostprocess_vert_glsl330, fs);
    if (shader == 0) {
        DEBUG_LOG_ERROR("Failed to compile gen.h PP shader: {}", effect_name);
        return 0;
    }
    programs_created_ += 1;
    pp_shaders_[key] = shader;
    return shader;
}

// ============================================================
// Cleanup
// ============================================================

void GLShaderManager::Shutdown() {
    if (pbr_shader_handle_ != 0) {
        glDeleteProgram(pbr_shader_handle_);
        programs_destroyed_ += 1;
        pbr_shader_handle_ = 0;
    }
    if (skybox_shader_handle_ != 0) {
        glDeleteProgram(skybox_shader_handle_);
        programs_destroyed_ += 1;
        skybox_shader_handle_ = 0;
    }
    if (particle_shader_handle_ != 0) {
        glDeleteProgram(particle_shader_handle_);
        programs_destroyed_ += 1;
        particle_shader_handle_ = 0;
    }
    for (auto& [name, handle] : pp_shaders_) {
        if (handle != 0) {
            glDeleteProgram(handle);
            programs_destroyed_ += 1;
        }
    }
    pp_shaders_.clear();
}

} // namespace render
} // namespace dse

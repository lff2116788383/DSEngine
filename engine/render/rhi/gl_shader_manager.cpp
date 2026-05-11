/**
 * @file gl_shader_manager.cpp
 * @brief GLShaderManager 实现 - 着色器管理器
 */

#include "engine/render/rhi/gl_shader_manager.h"
#include "engine/render/rhi/ubo_types.h"
#include "engine/base/debug.h"
#include <glad/gl.h>
#include <cstdio>

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
#include "embed/bloom_composite_frag.gen.h"

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
// 内置 PBR 着色器
// ============================================================

void GLShaderManager::InitBuiltinPBRShader() {
    using namespace dse::render::generated_shaders;
    pbr_shader_handle_ = CompileProgram(kpbr_vert_glsl330, kpbr_frag_glsl330);
    programs_created_ += 1;
    CachePBRLocations();
}

// helper: 绑定 UBO block 到指定 binding point
static void BindUBOBlock(unsigned int prog, const char* block_name,
                         UBOBindingPoint bp, unsigned int& out_index) {
    out_index = glGetUniformBlockIndex(prog, block_name);
    if (out_index != GL_INVALID_INDEX) {
        glUniformBlockBinding(prog, out_index, static_cast<unsigned int>(bp));
    }
}

void GLShaderManager::CachePBRLocations() {
    auto& loc = pbr_locations_;
    unsigned int h = pbr_shader_handle_;

    // --- UBO block 绑定 ---
    BindUBOBlock(h, "PerFrame",       UBOBindingPoint::PerFrame,      loc.per_frame_block_index);
    BindUBOBlock(h, "PerScene",       UBOBindingPoint::PerScene,      loc.per_scene_block_index);
    BindUBOBlock(h, "PerMaterial",    UBOBindingPoint::PerMaterial,   loc.per_material_block_index);
    BindUBOBlock(h, "PointLights",    UBOBindingPoint::PointLights,   loc.point_lights_block_index);
    BindUBOBlock(h, "SpotLights",     UBOBindingPoint::SpotLights,    loc.spot_lights_block_index);
    BindUBOBlock(h, "SpotLightData",  UBOBindingPoint::SpotLightData, loc.spot_light_data_block_index);
    BindUBOBlock(h, "BoneMatrices",   UBOBindingPoint::BoneMatrices,  loc.bone_matrices_block_index);
    BindUBOBlock(h, "MorphWeights",   UBOBindingPoint::MorphWeights,  loc.morph_weights_block_index);

    // --- 纹理采样器 uniform location ---
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

    // --- 逐对象 uniform（从 push constants 展平）-----
    loc.model = glGetUniformLocation(h, "u_model");
    loc.skinned = glGetUniformLocation(h, "u_skinned");
    loc.morph_enabled = glGetUniformLocation(h, "u_morph_enabled");
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

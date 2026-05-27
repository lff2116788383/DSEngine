/**
 * @file gl_draw_executor_postprocess.cpp
 * @brief GLDrawExecutor - post-process drawing (split from gl_draw_executor.cpp)
 *
 * 采用声明式绑定表驱动，每个效果通过 PPEffectEntry 描述参数布局。
 * 新增后处理效果只需在 kEffectTable 中追加一条记录。
 */

#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/opengl/gl_loader.h"
#include "engine/base/debug.h"

#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <unordered_map>
#include <iterator>  // std::size

namespace dse {
namespace render {

// ============================================================
// 声明式 Uniform 绑定描述
// ============================================================

enum class UType : uint8_t { Float, Float2, Float3, Int, Mat4 };

struct PPUniformEntry {
    const char* name;
    UType       type;
    int         param_offset;  // params[offset] 起始索引，-1 表示特殊来源
};

// 通用 uniform 绑定循环
static void ApplyBindings(unsigned int prog,
                           const PPUniformEntry* entries, int count,
                           const std::vector<float>& params) {
    for (int i = 0; i < count; ++i) {
        const auto& e = entries[i];
        if (e.param_offset < 0) continue;
        int loc = glGetUniformLocation(prog, e.name);
        if (loc < 0) continue;
        switch (e.type) {
        case UType::Float:
            glUniform1f(loc, params[e.param_offset]);
            break;
        case UType::Float2:
            glUniform2f(loc, params[e.param_offset], params[e.param_offset + 1]);
            break;
        case UType::Float3:
            glUniform3f(loc, params[e.param_offset], params[e.param_offset + 1], params[e.param_offset + 2]);
            break;
        case UType::Int:
            glUniform1i(loc, static_cast<int>(params[e.param_offset]));
            break;
        case UType::Mat4:
            glUniformMatrix4fv(loc, 1, GL_FALSE, &params[e.param_offset]);
            break;
        }
    }
}

// ============================================================
// 各效果绑定表（static constexpr 数组）
// ============================================================

static constexpr PPUniformEntry kFxaaBindings[] = {
    {"_27.u_resolution", UType::Float2, 0},
};
static constexpr PPUniformEntry kEdgeDetectBindings[] = {
    {"_28.u_thickness",         UType::Float, 0},
    {"_28.u_depth_threshold",   UType::Float, 1},
    {"_28.u_normal_threshold",  UType::Float, 2},
    {"_28.u_outline_r",         UType::Float, 3},
    {"_28.u_outline_g",         UType::Float, 4},
    {"_28.u_outline_b",         UType::Float, 5},
    {"_28.u_near",              UType::Float, 6},
    {"_28.u_far",               UType::Float, 7},
    {"_28.u_screen_w",          UType::Float, 8},
    {"_28.u_screen_h",          UType::Float, 9},
};
static constexpr PPUniformEntry kSsaoBindings[] = {
    {"_27.u_radius",      UType::Float,  0},
    {"_27.u_bias",        UType::Float,  1},
    {"_27.u_near",        UType::Float,  2},
    {"_27.u_far",         UType::Float,  3},
    {"_27.u_screen_size", UType::Float2, 4},
};
static constexpr PPUniformEntry kContactShadowBindings[] = {
    {"_23.u_light_dir",   UType::Float3, 0},
    {"_23.u_near",        UType::Float,  3},
    {"_23.u_far",         UType::Float,  4},
    {"_23.u_screen_size", UType::Float2, 5},
    {"_23.u_strength",    UType::Float,  7},
    {"_23.u_num_steps",   UType::Int,    8},
    {"_23.u_step_size",   UType::Float,  9},
};
static constexpr PPUniformEntry kDofBindings[] = {
    {"_20.focus_distance", UType::Float, 0},
    {"_20.focus_range",    UType::Float, 1},
    {"_20.bokeh_radius",   UType::Float, 2},
    {"_20.near_plane",     UType::Float, 3},
    {"_20.far_plane",      UType::Float, 4},
    {"_20.screen_w",       UType::Float, 5},
    {"_20.screen_h",       UType::Float, 6},
};
static constexpr PPUniformEntry kMotionVectorBindings[] = {
    {"_46.screen_w", UType::Float, 0},
    {"_46.screen_h", UType::Float, 1},
    {"_46.reproj",   UType::Mat4,  2},
};
static constexpr PPUniformEntry kMotionBlurBindings[] = {
    {"_23.intensity",   UType::Float, 0},
    {"_23.num_samples", UType::Float, 1},
    {"_23.screen_w",    UType::Float, 2},
    {"_23.screen_h",    UType::Float, 3},
};
static constexpr PPUniformEntry kSsrBindings[] = {
    {"_28.max_distance", UType::Float, 0},
    {"_28.thickness",    UType::Float, 1},
    {"_28.step_size",    UType::Float, 2},
    {"_28.max_steps",    UType::Int,   3},
    {"_28.near_plane",   UType::Float, 4},
    {"_28.far_plane",    UType::Float, 5},
    {"_28.screen_w",     UType::Float, 6},
    {"_28.screen_h",     UType::Float, 7},
};
static constexpr PPUniformEntry kTaaBindings[] = {
    {"_36.u_blend_factor", UType::Float, 0},
    {"_36.u_jitter_x",    UType::Float, 1},
    {"_36.u_jitter_y",    UType::Float, 2},
    {"_36.u_frame_index", UType::Int,   3},
    {"_36.u_screen_w",    UType::Float, 4},
    {"_36.u_screen_h",    UType::Float, 5},
};
static constexpr PPUniformEntry kDeferredLightBindings[] = {
    {"_58.u_light_dir",       UType::Float3, 0},
    {"_58.u_light_color",     UType::Float3, 3},
    {"_58.u_light_intensity", UType::Float,  6},
    {"_58.u_ambient",         UType::Float,  7},
};
static constexpr PPUniformEntry kLightShaftBindings[] = {
    {"_25.u_sun_x",       UType::Float, 0},
    {"_25.u_sun_y",       UType::Float, 1},
    {"_25.u_light_r",     UType::Float, 2},
    {"_25.u_light_g",     UType::Float, 3},
    {"_25.u_light_b",     UType::Float, 4},
    {"_25.u_density",     UType::Float, 5},
    {"_25.u_weight",      UType::Float, 6},
    {"_25.u_decay",       UType::Float, 7},
    {"_25.u_exposure",    UType::Float, 8},
    {"_25.u_num_samples", UType::Float, 9},
    {"_25.u_intensity",   UType::Float, 10},
};
static constexpr PPUniformEntry kVolumetricFogBindings[] = {
    {"_20.u_fog_r",        UType::Float, 0},  {"_20.u_fog_g",        UType::Float, 1},
    {"_20.u_fog_b",        UType::Float, 2},  {"_20.u_fog_density",  UType::Float, 3},
    {"_20.u_height_falloff", UType::Float, 4}, {"_20.u_height_offset", UType::Float, 5},
    {"_20.u_fog_start",    UType::Float, 6},  {"_20.u_fog_end",      UType::Float, 7},
    {"_20.u_fog_steps",    UType::Float, 8},  {"_20.u_sun_scatter",  UType::Float, 9},
    {"_20.u_sun_dir_x",   UType::Float, 10}, {"_20.u_sun_dir_y",   UType::Float, 11},
    {"_20.u_sun_dir_z",   UType::Float, 12}, {"_20.u_cam_pos_x",   UType::Float, 13},
    {"_20.u_cam_pos_y",   UType::Float, 14}, {"_20.u_cam_pos_z",   UType::Float, 15},
    {"_20.u_near",         UType::Float, 16}, {"_20.u_far",          UType::Float, 17},
    {"_20.u_right_x",     UType::Float, 18}, {"_20.u_right_y",     UType::Float, 19},
    {"_20.u_right_z",     UType::Float, 20}, {"_20.u_up_x",        UType::Float, 21},
    {"_20.u_up_y",        UType::Float, 22}, {"_20.u_up_z",        UType::Float, 23},
    {"_20.u_fwd_x",       UType::Float, 24}, {"_20.u_fwd_y",       UType::Float, 25},
    {"_20.u_fwd_z",       UType::Float, 26}, {"_20.u_tan_fov_y",   UType::Float, 27},
    {"_20.u_aspect",      UType::Float, 28},
};
static constexpr PPUniformEntry kVolumetricCloudBindings[] = {
    {"_286.u_cloud_bottom",     UType::Float, 0},  {"_286.u_cloud_top",       UType::Float, 1},
    {"_286.u_coverage",         UType::Float, 2},  {"_286.u_density",         UType::Float, 3},
    {"_286.u_shape_scale",      UType::Float, 4},  {"_286.u_detail_scale",    UType::Float, 5},
    {"_286.u_detail_strength",  UType::Float, 6},  {"_286.u_erosion",         UType::Float, 7},
    {"_286.u_wind_offset_x",    UType::Float, 8},  {"_286.u_wind_offset_z",   UType::Float, 9},
    {"_286.u_silver_intensity", UType::Float, 10}, {"_286.u_powder_strength", UType::Float, 11},
    {"_286.u_ambient_strength", UType::Float, 12}, {"_286.u_sun_dir_x",      UType::Float, 13},
    {"_286.u_sun_dir_y",        UType::Float, 14}, {"_286.u_sun_dir_z",      UType::Float, 15},
    {"_286.u_cam_pos_x",        UType::Float, 16}, {"_286.u_cam_pos_y",      UType::Float, 17},
    {"_286.u_cam_pos_z",        UType::Float, 18}, {"_286.u_near",           UType::Float, 19},
    {"_286.u_far",              UType::Float, 20}, {"_286.u_right_x",        UType::Float, 21},
    {"_286.u_right_y",          UType::Float, 22}, {"_286.u_right_z",        UType::Float, 23},
    {"_286.u_up_x",             UType::Float, 24}, {"_286.u_up_y",           UType::Float, 25},
    {"_286.u_up_z",             UType::Float, 26}, {"_286.u_fwd_x",          UType::Float, 27},
    {"_286.u_fwd_y",            UType::Float, 28}, {"_286.u_fwd_z",          UType::Float, 29},
};
static constexpr PPUniformEntry kAtmTransmittanceLutBindings[] = {
    {"_17.u_planet_radius",     UType::Float, 0},
    {"_17.u_atmosphere_height", UType::Float, 1},
    {"_17.u_rayleigh_r",        UType::Float, 2},
    {"_17.u_rayleigh_g",        UType::Float, 3},
    {"_17.u_rayleigh_b",        UType::Float, 4},
    {"_17.u_rayleigh_scale_h",  UType::Float, 5},
    {"_17.u_mie_coeff",         UType::Float, 6},
    {"_17.u_mie_scale_h",       UType::Float, 7},
};
static constexpr PPUniformEntry kAtmSkyBindings[] = {
    {"_74.u_sun_dir_x",      UType::Float, 0},  {"_74.u_sun_dir_y",      UType::Float, 1},
    {"_74.u_sun_dir_z",      UType::Float, 2},  {"_74.u_rayleigh_r",     UType::Float, 3},
    {"_74.u_rayleigh_g",     UType::Float, 4},  {"_74.u_rayleigh_b",     UType::Float, 5},
    {"_74.u_rayleigh_scale_h", UType::Float, 6}, {"_74.u_mie_coeff",     UType::Float, 7},
    {"_74.u_mie_scale_h",    UType::Float, 8},  {"_74.u_mie_g",          UType::Float, 9},
    {"_74.u_planet_radius",  UType::Float, 10}, {"_74.u_atmosphere_height", UType::Float, 11},
    {"_74.u_sun_intensity_r", UType::Float, 12}, {"_74.u_sun_intensity_g", UType::Float, 13},
    {"_74.u_sun_intensity_b", UType::Float, 14}, {"_74.u_sun_disk_angle", UType::Float, 15},
    {"_74.u_near",           UType::Float, 16}, {"_74.u_far",            UType::Float, 17},
    {"_74.u_tan_fov_y",      UType::Float, 18}, {"_74.u_aspect",         UType::Float, 19},
    {"_74.u_right_x",        UType::Float, 20}, {"_74.u_right_y",        UType::Float, 21},
    {"_74.u_right_z",        UType::Float, 22}, {"_74.u_up_x",           UType::Float, 23},
    {"_74.u_up_y",           UType::Float, 24}, {"_74.u_up_z",           UType::Float, 25},
    {"_74.u_fwd_x",          UType::Float, 26}, {"_74.u_fwd_y",          UType::Float, 27},
    {"_74.u_fwd_z",          UType::Float, 28}, {"_74.u_ozone_r",        UType::Float, 29},
    {"_74.u_ozone_g",        UType::Float, 30}, {"_74.u_ozone_b",        UType::Float, 31},
    {"_74.u_ozone_center_h", UType::Float, 32}, {"_74.u_ozone_width",    UType::Float, 33},
    {"_74.u_sky_view_steps", UType::Float, 34}, {"_74.u_reserved",       UType::Float, 35},
};
static constexpr PPUniformEntry kLumAdaptBindings[] = {
    {"_34.u_dt",             UType::Float, 0},
    {"_34.u_speed_up",       UType::Float, 1},
    {"_34.u_speed_down",     UType::Float, 2},
    {"_34.u_min_exposure",   UType::Float, 3},
    {"_34.u_max_exposure",   UType::Float, 4},
    {"_34.u_compensation",   UType::Float, 5},
};
static constexpr PPUniformEntry kDecalBindings[] = {
    {"_35.m00", UType::Float, 0},  {"_35.m01", UType::Float, 1},
    {"_35.m02", UType::Float, 2},  {"_35.m03", UType::Float, 3},
    {"_35.m10", UType::Float, 4},  {"_35.m11", UType::Float, 5},
    {"_35.m12", UType::Float, 6},  {"_35.m13", UType::Float, 7},
    {"_35.m20", UType::Float, 8},  {"_35.m21", UType::Float, 9},
    {"_35.m22", UType::Float, 10}, {"_35.m23", UType::Float, 11},
    {"_35.m30", UType::Float, 12}, {"_35.m31", UType::Float, 13},
    {"_35.m32", UType::Float, 14}, {"_35.m33", UType::Float, 15},
    {"_35.u_color_r",    UType::Float, 16}, {"_35.u_color_g",    UType::Float, 17},
    {"_35.u_color_b",    UType::Float, 18}, {"_35.u_color_a",    UType::Float, 19},
    {"_35.u_angle_fade", UType::Float, 20},
    {"_35.u_decal_up_x", UType::Float, 21}, {"_35.u_decal_up_y", UType::Float, 22},
    {"_35.u_decal_up_z", UType::Float, 23},
};
static constexpr PPUniformEntry kWaterBindings[] = {
    {"_29.u_water_level",  UType::Float, 0},
    {"_29.u_deep_r",       UType::Float, 1},  {"_29.u_deep_g",       UType::Float, 2},
    {"_29.u_deep_b",       UType::Float, 3},  {"_29.u_shallow_r",    UType::Float, 4},
    {"_29.u_shallow_g",    UType::Float, 5},  {"_29.u_shallow_b",    UType::Float, 6},
    {"_29.u_max_depth",    UType::Float, 7},  {"_29.u_transparency", UType::Float, 8},
    {"_29.u_wave_amplitude", UType::Float, 9}, {"_29.u_wave_frequency", UType::Float, 10},
    {"_29.u_wave_speed",   UType::Float, 11}, {"_29.u_wave_dir_x",   UType::Float, 12},
    {"_29.u_wave_dir_y",   UType::Float, 13}, {"_29.u_refraction_strength", UType::Float, 14},
    {"_29.u_specular_power", UType::Float, 15}, {"_29.u_reflection_strength", UType::Float, 16},
    {"_29.u_time",         UType::Float, 17},
    {"_29.u_sun_dir_x",   UType::Float, 18}, {"_29.u_sun_dir_y",   UType::Float, 19},
    {"_29.u_sun_dir_z",   UType::Float, 20},
    {"_29.u_cam_pos_x",   UType::Float, 21}, {"_29.u_cam_pos_y",   UType::Float, 22},
    {"_29.u_cam_pos_z",   UType::Float, 23},
    {"_29.u_near",         UType::Float, 24}, {"_29.u_far",          UType::Float, 25},
    {"_29.u_fwd_x",       UType::Float, 26}, {"_29.u_fwd_y",       UType::Float, 27},
    {"_29.u_fwd_z",       UType::Float, 28}, {"_29.u_tan_fov_y",   UType::Float, 29},
    {"_29.u_aspect",      UType::Float, 30},
    {"_29.u_caustic_intensity", UType::Float, 31}, {"_29.u_caustic_scale", UType::Float, 32},
    {"_29.u_foam_intensity", UType::Float, 33}, {"_29.u_foam_depth_threshold", UType::Float, 34},
    {"_29.u_uw_fog_density", UType::Float, 35},
    {"_29.u_uw_fog_r",    UType::Float, 36}, {"_29.u_uw_fog_g",    UType::Float, 37},
    {"_29.u_uw_fog_b",    UType::Float, 38},
};

// ============================================================
// 特殊处理函数（有条件纹理绑定、UBO 上传等非平凡效果）
// ============================================================

using SpecialBindFn = void(*)(unsigned int prog,
                              const PostProcessRequest& req,
                              const std::vector<float>& params,
                              unsigned int& pp_param_ubo);

static void BindBloomUBO(unsigned int /*prog*/,
                          const PostProcessRequest& /*req*/,
                          const std::vector<float>& params,
                          unsigned int& pp_param_ubo) {
    if (pp_param_ubo == 0) {
        glGenBuffers(1, &pp_param_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, pp_param_ubo);
        glBufferData(GL_UNIFORM_BUFFER, 256, nullptr, GL_DYNAMIC_DRAW);
    }
    float ubo[4] = { params[0], params.size() > 1 ? params[1] : 0.f, 0.f, 0.f };
    glBindBuffer(GL_UNIFORM_BUFFER, pp_param_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 16, ubo);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, pp_param_ubo);
}

static void BindTonemapping(unsigned int prog,
                             const PostProcessRequest& req,
                             const std::vector<float>& params,
                             unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_68.u_manual_exposure"), params[0]);
    const bool has_ae  = req.FindTex(2) != 0;
    const bool has_lut = req.FindTex(5) != 0;
    glUniform1i(glGetUniformLocation(prog, "_68.u_auto_exposure_enabled"), has_ae ? 1 : 0);
    glUniform1i(glGetUniformLocation(prog, "_68.u_lut_enabled"), has_lut ? 1 : 0);
    if (has_lut && params.size() >= 2)
        glUniform1f(glGetUniformLocation(prog, "_68.u_lut_intensity"), params[1]);
}

static void BindColorGrading(unsigned int prog,
                              const PostProcessRequest& /*req*/,
                              const std::vector<float>& params,
                              unsigned int& /*ubo*/) {
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_3D, static_cast<unsigned int>(params[0]));
    glUniform1f(glGetUniformLocation(prog, "_40.u_lut_intensity"), params[1]);
}

static void BindBloomComposite(unsigned int prog,
                                const PostProcessRequest& /*req*/,
                                const std::vector<float>& params,
                                unsigned int& /*ubo*/) {
    const CompositeParamsView cv(params);
    const auto bcp = PrepareBloomCompositeParams(cv);
    glUniform1f(glGetUniformLocation(prog, "_90.exposure"), bcp.exposure);
    glUniform1f(glGetUniformLocation(prog, "_90.bloomIntensity"), bcp.bloom_intensity);
    glUniform1i(glGetUniformLocation(prog, "_90.bloomEnabled"), bcp.bloom_enabled);
    glUniform1i(glGetUniformLocation(prog, "_90.ssaoEnabled"), bcp.ssao_enabled);
    glUniform1i(glGetUniformLocation(prog, "_90.autoExposureEnabled"), bcp.ae_enabled);
    glUniform1i(glGetUniformLocation(prog, "_90.lutEnabled"), bcp.lut_enabled);
    glUniform1f(glGetUniformLocation(prog, "_90.lutIntensity"), bcp.lut_intensity);
    glUniform1i(glGetUniformLocation(prog, "_90.csEnabled"), bcp.cs_enabled);
    glUniform1f(glGetUniformLocation(prog, "_90.csStrength"), bcp.cs_strength);
    glUniform1i(glGetUniformLocation(prog, "_90.vignetteEnabled"), bcp.vignette_enabled);
    glUniform1f(glGetUniformLocation(prog, "_90.vignetteIntensity"), bcp.vignette_intensity);
    glUniform1f(glGetUniformLocation(prog, "_90.vignetteRadius"), bcp.vignette_radius);
    glUniform1f(glGetUniformLocation(prog, "_90.vignetteSoftness"), bcp.vignette_softness);
    glUniform1i(glGetUniformLocation(prog, "_90.filmGrainEnabled"), bcp.film_grain_enabled);
    glUniform1f(glGetUniformLocation(prog, "_90.filmGrainIntensity"), bcp.film_grain_intensity);
    glUniform1f(glGetUniformLocation(prog, "_90.filmGrainTime"), bcp.film_grain_time);
    if (bcp.bloom_enabled) { glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kBloomTex)); }
    if (bcp.ssao_enabled)  { glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kSsaoTex)); }
    if (bcp.ae_enabled)    { glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kAutoExposureTex)); }
    if (bcp.lut_enabled)   { glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_3D, cv.Texture(CompositeParamsView::kLutTex)); }
    if (bcp.cs_enabled)    { glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kContactShadowTex)); }
}

static void BindSsaoApply(unsigned int prog,
                           const PostProcessRequest& req,
                           const std::vector<float>& params,
                           unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_79.exposure"), params[0]);
    const bool has_ae  = req.FindTex(3) != 0;
    const bool has_lut = req.FindTex(5) != 0;
    glUniform1i(glGetUniformLocation(prog, "_79.autoExposureEnabled"), has_ae ? 1 : 0);
    glUniform1i(glGetUniformLocation(prog, "_79.lutEnabled"), has_lut ? 1 : 0);
    if (has_lut && params.size() >= 2)
        glUniform1f(glGetUniformLocation(prog, "_79.lutIntensity"), params[1]);
}

static void BindLightShaftExtra(unsigned int prog,
                                 const PostProcessRequest& req,
                                 const std::vector<float>& /*params*/,
                                 unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_25.u_depth_handle"), static_cast<float>(req.FindTex(2)));
}

static void BindVolumetricFogExtra(unsigned int prog,
                                    const PostProcessRequest& req,
                                    const std::vector<float>& /*params*/,
                                    unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_20.u_depth_handle"), static_cast<float>(req.FindTex(2)));
}

static void BindVolumetricCloudExtra(unsigned int prog,
                                      const PostProcessRequest& req,
                                      const std::vector<float>& /*params*/,
                                      unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_286.u_depth_handle"), static_cast<float>(req.FindTex(2)));
}

static void BindWboitComposite(unsigned int prog,
                                const PostProcessRequest& req,
                                const std::vector<float>& /*params*/,
                                unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_61.u_reveal_handle"), static_cast<float>(req.FindTex(2)));
}

static void BindDecalExtra(unsigned int prog,
                            const PostProcessRequest& req,
                            const std::vector<float>& /*params*/,
                            unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_35.u_depth_handle"), static_cast<float>(req.FindTex(2)));
    glUniform1f(glGetUniformLocation(prog, "_35.u_decal_handle"), static_cast<float>(req.FindTex(3)));
}

static void BindWaterExtra(unsigned int prog,
                            const PostProcessRequest& req,
                            const std::vector<float>& /*params*/,
                            unsigned int& /*ubo*/) {
    glUniform1f(glGetUniformLocation(prog, "_29.u_depth_handle"), static_cast<float>(req.FindTex(2)));
}

// ============================================================
// 效果注册表
// ============================================================

struct PPEffectEntry {
    const PPUniformEntry* bindings;
    int                   binding_count;
    int                   min_params;       // params.size() 最小要求
    SpecialBindFn         special_fn;       // 非空时调用特殊处理（可与 bindings 叠加）
    bool                  needs_blend;      // 需要开启 alpha blend
};

static const std::unordered_map<std::string, PPEffectEntry>& GetEffectTable() {
    static const std::unordered_map<std::string, PPEffectEntry> table = {
        // --- 简单表驱动效果 ---
        {"fxaa",              {kFxaaBindings,         (int)std::size(kFxaaBindings), 2, nullptr, false}},
        {"edge_detect",       {kEdgeDetectBindings,  (int)std::size(kEdgeDetectBindings), 10, nullptr, false}},
        {"ssao",              {kSsaoBindings,         (int)std::size(kSsaoBindings), 6, nullptr, false}},
        {"contact_shadow",    {kContactShadowBindings, (int)std::size(kContactShadowBindings), 10, nullptr, false}},
        {"dof",              {kDofBindings,           (int)std::size(kDofBindings), 7, nullptr, false}},
        {"motion_vector",    {kMotionVectorBindings,  (int)std::size(kMotionVectorBindings), 18, nullptr, false}},
        {"motion_blur",      {kMotionBlurBindings,    (int)std::size(kMotionBlurBindings), 4, nullptr, false}},
        {"ssr",              {kSsrBindings,           (int)std::size(kSsrBindings), 8, nullptr, false}},
        {"taa_resolve",      {kTaaBindings,           (int)std::size(kTaaBindings), 6, nullptr, false}},
        {"deferred_lighting", {kDeferredLightBindings, (int)std::size(kDeferredLightBindings), 8, nullptr, false}},
        {"lum_adapt",        {kLumAdaptBindings,      (int)std::size(kLumAdaptBindings), 6, nullptr, false}},
        // --- UBO 效果 ---
        {"bloom_extract",    {nullptr, 0, 1, BindBloomUBO, false}},
        {"bloom_downsample", {nullptr, 0, 2, BindBloomUBO, false}},
        {"bloom_upsample",   {nullptr, 0, 1, BindBloomUBO, false}},
        // --- 特殊绑定效果 ---
        {"tonemapping",      {nullptr, 0, 1, BindTonemapping, false}},
        {"color_grading",    {nullptr, 0, 2, BindColorGrading, false}},
        {"bloom_composite",  {nullptr, 0, 0, BindBloomComposite, false}},
        {"ssao_apply",       {nullptr, 0, 1, BindSsaoApply, false}},
        // --- 带额外纹理绑定的效果 ---
        {"light_shaft",      {kLightShaftBindings, (int)std::size(kLightShaftBindings), 11, BindLightShaftExtra, false}},
        {"volumetric_fog",   {kVolumetricFogBindings, (int)std::size(kVolumetricFogBindings), 29, BindVolumetricFogExtra, false}},
        {"volumetric_cloud", {kVolumetricCloudBindings, (int)std::size(kVolumetricCloudBindings), 30, BindVolumetricCloudExtra, false}},
        {"wboit_composite",  {nullptr, 0, 0, BindWboitComposite, true}},
        {"decal",            {kDecalBindings, (int)std::size(kDecalBindings), 24, BindDecalExtra, true}},
        {"water",            {kWaterBindings, (int)std::size(kWaterBindings), 39, BindWaterExtra, true}},
        // --- 无参数效果 ---
        {"ui_overlay",               {nullptr, 0, 0, nullptr, true}},
        {"postprocess_passthrough", {nullptr, 0, 0, nullptr, false}},
        {"ssao_blur",               {nullptr, 0, 0, nullptr, false}},
        {"lum_compute",             {nullptr, 0, 0, nullptr, false}},
        {"bloom_blur_h",            {nullptr, 0, 0, nullptr, false}},
        {"bloom_blur_v",            {nullptr, 0, 0, nullptr, false}},
        // --- 大气天空 ---
        {"atmosphere_transmittance_lut", {kAtmTransmittanceLutBindings, (int)std::size(kAtmTransmittanceLutBindings), 8, nullptr, false}},
        {"atmosphere_sky",   {kAtmSkyBindings, (int)std::size(kAtmSkyBindings), 36, nullptr, false}},
    };
    return table;
}

// ============================================================
// DrawPostProcess 主入口
// ============================================================

void GLDrawExecutor::DrawPostProcess(const dse::render::PostProcessRequest& request,
                                       GLShaderManager& shader_mgr) {
    const unsigned int source_texture = request.source_texture;
    const std::string& effect_name = request.effect_name;
    const std::vector<float>& params = request.params;

    // 后处理全屏四边形 VAO/VBO 懒初始化
    if (!pp_vao_handle_) {
        float quadVertices[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        if (create_vao_fn_ && create_buffer_fn_) {
            pp_vao_handle_ = create_vao_fn_();
            pp_vbo_handle_ = create_buffer_fn_(sizeof(quadVertices), &quadVertices, false, false);
        } else {
            unsigned int pp_v = 0; glGenVertexArrays(1, &pp_v); pp_vao_handle_ = VertexArrayHandle{pp_v};
            glGenBuffers(1, &pp_vbo_handle_);
        }
        glBindVertexArray(pp_vao_handle_.raw());
        glBindBuffer(GL_ARRAY_BUFFER, pp_vbo_handle_);
        if (!create_buffer_fn_) {
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        }
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    // 获取 gen.h shader
    unsigned int gen_shader = shader_mgr.GetOrCreateGenPPShader(effect_name);
    if (gen_shader == 0) {
        DEBUG_LOG_WARN("PostProcess effect '{}' has no gen.h shader — skipping", effect_name.c_str());
        return;
    }
    glUseProgram(gen_shader);

    // 绑定 source texture
    glActiveTexture(GL_TEXTURE0 + request.source_binding);
    glBindTexture(GL_TEXTURE_2D, source_texture);

    // 绑定 request.textures[] 中声明的额外纹理
    for (const auto& tb : request.textures) {
        if (tb.handle == 0) break;
        glActiveTexture(GL_TEXTURE0 + tb.slot);
        glBindTexture(tb.is_3d ? GL_TEXTURE_3D : GL_TEXTURE_2D, tb.handle);
    }

    // 查表绑定参数
    const auto& table = GetEffectTable();
    auto it = table.find(effect_name);
    if (it != table.end()) {
        const auto& entry = it->second;
        if (static_cast<int>(params.size()) >= entry.min_params) {
            if (entry.special_fn)
                entry.special_fn(gen_shader, request, params, pp_param_ubo_);
            if (entry.bindings && entry.binding_count > 0)
                ApplyBindings(gen_shader, entry.bindings, entry.binding_count, params);
        }
        // 绘制
        glDisable(GL_DEPTH_TEST);
        if (entry.needs_blend) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }
    } else {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    glBindVertexArray(pp_vao_handle_.raw());
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
} // namespace render
} // namespace dse
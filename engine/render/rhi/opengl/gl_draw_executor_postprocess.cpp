/**
 * @file gl_draw_executor_postprocess.cpp
 * @brief GLDrawExecutor - post-process drawing (split from gl_draw_executor.cpp)
 */

#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/opengl/gl_loader.h"
#include "engine/base/debug.h"

#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace dse {
namespace render {
void GLDrawExecutor::DrawPostProcess(const dse::render::PostProcessRequest& request,
                                       GLShaderManager& shader_mgr) {
    const unsigned int source_texture = request.source_texture;
    const std::string& effect_name = request.effect_name;
    const std::vector<float>& params = request.params;
    // 后处理全屏四边形 VAO/VBO
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

    // ====== gen.h GLSL 430 统一路径（Phase 3）======
    // gen.h shader 是完整独立的 GLSL 430 源，不需要 header 拼接。
    // sampler 使用 layout(binding=N) 自动绑定纹理单元。参数通过 std140 uniform block (binding=2) 或 plain struct uniform 传递。
    {
        unsigned int gen_shader = shader_mgr.GetOrCreateGenPPShader(effect_name);
        if (gen_shader != 0) {
            glUseProgram(gen_shader);

            // gen.h: screenTexture — binding=source_binding (light_shaft=0, others=1)
            glActiveTexture(GL_TEXTURE0 + request.source_binding);
            glBindTexture(GL_TEXTURE_2D, source_texture);

            // 绑定 request.textures[] 中声明的额外纹理
            for (const auto& tb : request.textures) {
                if (tb.handle == 0) break;
                glActiveTexture(GL_TEXTURE0 + tb.slot);
                glBindTexture(tb.is_3d ? GL_TEXTURE_3D : GL_TEXTURE_2D, tb.handle);
            }

            // 参数 UBO 懒创建（用于 std140 uniform block 类 shader）
            auto ensure_param_ubo = [&]() {
                if (pp_param_ubo_ == 0) {
                    glGenBuffers(1, &pp_param_ubo_);
                    glBindBuffer(GL_UNIFORM_BUFFER, pp_param_ubo_);
                    glBufferData(GL_UNIFORM_BUFFER, 256, nullptr, GL_DYNAMIC_DRAW);
                }
            };
            auto upload_ubo = [&](const void* data, size_t size) {
                ensure_param_ubo();
                glBindBuffer(GL_UNIFORM_BUFFER, pp_param_ubo_);
                glBufferSubData(GL_UNIFORM_BUFFER, 0, size, data);
                glBindBufferBase(GL_UNIFORM_BUFFER, 2, pp_param_ubo_);
            };

            if (effect_name == "fxaa" && params.size() >= 2) {
                // plain struct uniform: uniform FxaaParams _27; → _27.u_resolution
                glUniform2f(glGetUniformLocation(gen_shader, "_27.u_resolution"),
                            params[0], params[1]);
            } else if (effect_name == "bloom_extract" && params.size() >= 1) {
                // layout(binding=2,std140) uniform BloomParams { float threshold; }
                float ubo[4] = { params[0], 0.f, 0.f, 0.f };
                upload_ubo(ubo, 16);
            } else if (effect_name == "bloom_downsample" && params.size() >= 2) {
                // layout(binding=2,std140) uniform BloomParams { vec2 srcResolution; }
                float ubo[4] = { params[0], params[1], 0.f, 0.f };
                upload_ubo(ubo, 16);
            } else if (effect_name == "bloom_upsample" && params.size() >= 1) {
                // layout(binding=2,std140) uniform BloomParams { float filterRadius; }
                float ubo[4] = { params[0], 0.f, 0.f, 0.f };
                upload_ubo(ubo, 16);
            } else if (effect_name == "tonemapping" && params.size() >= 1) {
                // plain struct TonemapParams _68
                glUniform1f(glGetUniformLocation(gen_shader, "_68.u_manual_exposure"), params[0]);
                const bool has_ae  = request.FindTex(2) != 0;
                const bool has_lut = request.FindTex(5) != 0;
                glUniform1i(glGetUniformLocation(gen_shader, "_68.u_auto_exposure_enabled"), has_ae ? 1 : 0);
                glUniform1i(glGetUniformLocation(gen_shader, "_68.u_lut_enabled"), has_lut ? 1 : 0);
                if (has_lut && params.size() >= 2) {
                    glUniform1f(glGetUniformLocation(gen_shader, "_68.u_lut_intensity"), params[1]);
                }
            } else if (effect_name == "color_grading" && params.size() >= 2) {
                // plain struct ColorGradingParams _40
                glActiveTexture(GL_TEXTURE5);
                glBindTexture(GL_TEXTURE_3D, static_cast<unsigned int>(params[0]));
                glUniform1f(glGetUniformLocation(gen_shader, "_40.u_lut_intensity"), params[1]);
            } else if (effect_name == "edge_detect" && params.size() >= 10) {
                // plain struct EdgeDetectParams _28
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_thickness"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_depth_threshold"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_normal_threshold"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_outline_r"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_outline_g"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_outline_b"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_near"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_far"), params[7]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_screen_w"), params[8]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.u_screen_h"), params[9]);
            } else if (effect_name == "postprocess_passthrough") {
                // no params — screenTexture already bound at unit 1
            } else if (effect_name == "bloom_composite") {
                // gen.h: bloom_composite_ssao_ae, plain struct BloomCompositeAeParams _90
                const CompositeParamsView cv(params);
                const auto bcp = PrepareBloomCompositeParams(cv);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.exposure"), bcp.exposure);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.bloomIntensity"), bcp.bloom_intensity);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.bloomEnabled"), bcp.bloom_enabled);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.ssaoEnabled"), bcp.ssao_enabled);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.autoExposureEnabled"), bcp.ae_enabled);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.lutEnabled"), bcp.lut_enabled);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.lutIntensity"), bcp.lut_intensity);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.csEnabled"), bcp.cs_enabled);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.csStrength"), bcp.cs_strength);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.vignetteEnabled"), bcp.vignette_enabled);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.vignetteIntensity"), bcp.vignette_intensity);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.vignetteRadius"), bcp.vignette_radius);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.vignetteSoftness"), bcp.vignette_softness);
                glUniform1i(glGetUniformLocation(gen_shader, "_90.filmGrainEnabled"), bcp.film_grain_enabled);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.filmGrainIntensity"), bcp.film_grain_intensity);
                glUniform1f(glGetUniformLocation(gen_shader, "_90.filmGrainTime"), bcp.film_grain_time);
                if (bcp.bloom_enabled) {
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kBloomTex));
                }
                if (bcp.ssao_enabled) {
                    glActiveTexture(GL_TEXTURE3);
                    glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kSsaoTex));
                }
                if (bcp.ae_enabled) {
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kAutoExposureTex));
                }
                if (bcp.lut_enabled) {
                    glActiveTexture(GL_TEXTURE5);
                    glBindTexture(GL_TEXTURE_3D, cv.Texture(CompositeParamsView::kLutTex));
                }
                if (bcp.cs_enabled) {
                    glActiveTexture(GL_TEXTURE6);
                    glBindTexture(GL_TEXTURE_2D, cv.Texture(CompositeParamsView::kContactShadowTex));
                }
            } else if (effect_name == "ssao_apply" && params.size() >= 1) {
                // plain struct SsaoApplyParams _79
                // samplers: screenTexture(1), ssaoTexture(2), autoExposureTex(3), lutTexture(5)
                glUniform1f(glGetUniformLocation(gen_shader, "_79.exposure"), params[0]);
                const bool has_ae  = request.FindTex(3) != 0;
                const bool has_lut = request.FindTex(5) != 0;
                glUniform1i(glGetUniformLocation(gen_shader, "_79.autoExposureEnabled"), has_ae ? 1 : 0);
                glUniform1i(glGetUniformLocation(gen_shader, "_79.lutEnabled"), has_lut ? 1 : 0);
                if (has_lut && params.size() >= 2) {
                    glUniform1f(glGetUniformLocation(gen_shader, "_79.lutIntensity"), params[1]);
                }
            } else if (effect_name == "ssao" && params.size() >= 6) {
                // plain struct SsaoParams _27, sampler: screenTexture(1)
                glUniform1f(glGetUniformLocation(gen_shader, "_27.u_radius"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_27.u_bias"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_27.u_near"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_27.u_far"), params[3]);
                glUniform2f(glGetUniformLocation(gen_shader, "_27.u_screen_size"), params[4], params[5]);
            } else if (effect_name == "ssao_blur") {
                // no params — screenTexture(1) only
            } else if (effect_name == "contact_shadow" && params.size() >= 10) {
                // plain struct ContactShadowParams _23, sampler: screenTexture(1)
                glUniform3f(glGetUniformLocation(gen_shader, "_23.u_light_dir"), params[0], params[1], params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_23.u_near"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_23.u_far"), params[4]);
                glUniform2f(glGetUniformLocation(gen_shader, "_23.u_screen_size"), params[5], params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_23.u_strength"), params[7]);
                glUniform1i(glGetUniformLocation(gen_shader, "_23.u_num_steps"), static_cast<int>(params[8]));
                glUniform1f(glGetUniformLocation(gen_shader, "_23.u_step_size"), params[9]);
            } else if (effect_name == "dof" && params.size() >= 7) {
                // plain struct DofParams _20, samplers: screenTexture(1), u_color_texture(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_20.focus_distance"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.focus_range"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.bokeh_radius"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.near_plane"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.far_plane"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.screen_w"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.screen_h"), params[6]);
            } else if (effect_name == "motion_vector" && params.size() >= 18) {
                // plain struct MvParams _46, sampler: screenTexture(1)
                glUniform1f(glGetUniformLocation(gen_shader, "_46.screen_w"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_46.screen_h"), params[1]);
                glUniformMatrix4fv(glGetUniformLocation(gen_shader, "_46.reproj"), 1, GL_FALSE, &params[2]);
            } else if (effect_name == "motion_blur" && params.size() >= 4) {
                // plain struct MotionBlurParams _23, samplers: screenTexture(1), u_color_texture(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_23.intensity"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_23.num_samples"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_23.screen_w"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_23.screen_h"), params[3]);
            } else if (effect_name == "ssr" && params.size() >= 8) {
                // plain struct SsrParams _28, samplers: screenTexture(1), u_color_texture(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_28.max_distance"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.thickness"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.step_size"), params[2]);
                glUniform1i(glGetUniformLocation(gen_shader, "_28.max_steps"), static_cast<int>(params[3]));
                glUniform1f(glGetUniformLocation(gen_shader, "_28.near_plane"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.far_plane"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.screen_w"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_28.screen_h"), params[7]);
            } else if (effect_name == "taa_resolve" && params.size() >= 6) {
                // plain struct TaaParams _36
                // samplers: screenTexture(1), u_motion_vector(2), u_history(5)
                glUniform1f(glGetUniformLocation(gen_shader, "_36.u_blend_factor"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_36.u_jitter_x"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_36.u_jitter_y"), params[2]);
                glUniform1i(glGetUniformLocation(gen_shader, "_36.u_frame_index"), static_cast<int>(params[3]));
                glUniform1f(glGetUniformLocation(gen_shader, "_36.u_screen_w"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_36.u_screen_h"), params[5]);
            } else if (effect_name == "deferred_lighting" && params.size() >= 8) {
                // plain struct DeferredLightParams _58
                // samplers: screenTexture(1), u_gbuf_normal(2), u_gbuf_position(3)
                glUniform3f(glGetUniformLocation(gen_shader, "_58.u_light_dir"), params[0], params[1], params[2]);
                glUniform3f(glGetUniformLocation(gen_shader, "_58.u_light_color"), params[3], params[4], params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_58.u_light_intensity"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_58.u_ambient"), params[7]);
            } else if (effect_name == "light_shaft" && params.size() >= 11) {
                // plain struct LightShaftParams _25
                // samplers: screenTexture(0), u_depth_tex(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_depth_handle"), static_cast<float>(request.FindTex(2)));
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_sun_x"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_sun_y"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_light_r"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_light_g"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_light_b"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_density"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_weight"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_decay"), params[7]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_exposure"), params[8]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_num_samples"), params[9]);
                glUniform1f(glGetUniformLocation(gen_shader, "_25.u_intensity"), params[10]);
            } else if (effect_name == "volumetric_fog" && params.size() >= 29) {
                // plain struct VolumetricFogParams _20
                // samplers: screenTexture(1), u_depth_tex(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_depth_handle"), static_cast<float>(request.FindTex(2)));
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_r"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_g"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_b"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_density"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_height_falloff"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_height_offset"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_start"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_end"), params[7]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fog_steps"), params[8]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_sun_scatter"), params[9]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_sun_dir_x"), params[10]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_sun_dir_y"), params[11]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_sun_dir_z"), params[12]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_cam_pos_x"), params[13]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_cam_pos_y"), params[14]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_cam_pos_z"), params[15]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_near"), params[16]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_far"), params[17]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_right_x"), params[18]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_right_y"), params[19]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_right_z"), params[20]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_up_x"), params[21]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_up_y"), params[22]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_up_z"), params[23]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fwd_x"), params[24]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fwd_y"), params[25]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_fwd_z"), params[26]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_tan_fov_y"), params[27]);
                glUniform1f(glGetUniformLocation(gen_shader, "_20.u_aspect"), params[28]);
            } else if (effect_name == "lum_compute") {
                // no params — screenTexture(1) only
            } else if (effect_name == "lum_adapt" && params.size() >= 6) {
                // plain struct LumAdaptParams _34
                // samplers: screenTexture(1), prevAdaptedTex(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_34.u_dt"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_34.u_speed_up"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_34.u_speed_down"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_34.u_min_exposure"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_34.u_max_exposure"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_34.u_compensation"), params[5]);
            } else if (effect_name == "wboit_composite") {
                // plain struct WboitParams _61, samplers: screenTexture(1), u_reveal_tex(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_61.u_reveal_handle"), static_cast<float>(request.FindTex(2)));
            } else if (effect_name == "decal" && params.size() >= 24) {
                // plain struct DecalParams _35
                // samplers: screenTexture(1), u_depth_tex(2), u_decal_tex(3)
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_depth_handle"), static_cast<float>(request.FindTex(2)));
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_decal_handle"), static_cast<float>(request.FindTex(3)));
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m00"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m01"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m02"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m03"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m10"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m11"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m12"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m13"), params[7]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m20"), params[8]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m21"), params[9]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m22"), params[10]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m23"), params[11]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m30"), params[12]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m31"), params[13]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m32"), params[14]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.m33"), params[15]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_color_r"), params[16]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_color_g"), params[17]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_color_b"), params[18]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_color_a"), params[19]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_angle_fade"), params[20]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_decal_up_x"), params[21]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_decal_up_y"), params[22]);
                glUniform1f(glGetUniformLocation(gen_shader, "_35.u_decal_up_z"), params[23]);
            } else if (effect_name == "water" && params.size() >= 39) {
                // plain struct WaterParams _29
                // samplers: screenTexture(1), u_depth_tex(2)
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_depth_handle"), static_cast<float>(request.FindTex(2)));
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_water_level"), params[0]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_deep_r"), params[1]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_deep_g"), params[2]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_deep_b"), params[3]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_shallow_r"), params[4]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_shallow_g"), params[5]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_shallow_b"), params[6]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_max_depth"), params[7]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_transparency"), params[8]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_wave_amplitude"), params[9]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_wave_frequency"), params[10]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_wave_speed"), params[11]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_wave_dir_x"), params[12]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_wave_dir_y"), params[13]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_refraction_strength"), params[14]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_specular_power"), params[15]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_reflection_strength"), params[16]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_time"), params[17]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_sun_dir_x"), params[18]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_sun_dir_y"), params[19]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_sun_dir_z"), params[20]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_cam_pos_x"), params[21]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_cam_pos_y"), params[22]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_cam_pos_z"), params[23]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_near"), params[24]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_far"), params[25]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_fwd_x"), params[26]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_fwd_y"), params[27]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_fwd_z"), params[28]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_tan_fov_y"), params[29]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_aspect"), params[30]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_caustic_intensity"), params[31]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_caustic_scale"), params[32]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_foam_intensity"), params[33]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_foam_depth_threshold"), params[34]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_uw_fog_density"), params[35]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_uw_fog_r"), params[36]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_uw_fog_g"), params[37]);
                glUniform1f(glGetUniformLocation(gen_shader, "_29.u_uw_fog_b"), params[38]);
            } else if (effect_name == "bloom_blur_h" || effect_name == "bloom_blur_v") {
                // no params — screenTexture(1) only
            }

            // 绘制全屏四边形
            glDisable(GL_DEPTH_TEST);
            if (effect_name == "decal" || effect_name == "wboit_composite" || effect_name == "water") {
                glEnable(GL_BLEND);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            } else {
                glDisable(GL_BLEND);
            }
            glBindVertexArray(pp_vao_handle_.raw());
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            return;
        }
    }
    // gen.h 路径已覆盖所有后处理效果，不应到达此处
    DEBUG_LOG_WARN("PostProcess effect '{}' has no gen.h shader — skipping", effect_name.c_str());
}
} // namespace render
} // namespace dse
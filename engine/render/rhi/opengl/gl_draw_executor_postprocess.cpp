/**
 * @file gl_draw_executor_postprocess.cpp
 * @brief GLDrawExecutor - post-process drawing (split from gl_draw_executor.cpp)
 */

#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/opengl/gl_loader.h"

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
    if (pp_vao_handle_ == 0) {
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
            glGenVertexArrays(1, &pp_vao_handle_);
            glGenBuffers(1, &pp_vbo_handle_);
        }
        glBindVertexArray(pp_vao_handle_);
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
            glBindVertexArray(pp_vao_handle_);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            return;
        }
    }
    // ====== 旧路径：动态拼接 GLSL 330 ======

    // 构建后处理片段着色器
    const char* vs_src = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;
        out vec2 TexCoords;
        void main() {
            TexCoords = aTexCoords;
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
        }
    )";

    std::string fs_src = "#version 330 core\nout vec4 FragColor;\nin vec2 TexCoords;\nuniform sampler2D screenTexture;\n";

    if (effect_name == "bloom_downsample") {
        fs_src += R"(
            uniform vec2 srcResolution;
            void main() {
                vec2 srcTexelSize = 1.0 / srcResolution;
                float x = srcTexelSize.x;
                float y = srcTexelSize.y;
                vec3 a = texture(screenTexture, vec2(TexCoords.x - 2*x, TexCoords.y + 2*y)).rgb;
                vec3 b = texture(screenTexture, vec2(TexCoords.x,       TexCoords.y + 2*y)).rgb;
                vec3 c = texture(screenTexture, vec2(TexCoords.x + 2*x, TexCoords.y + 2*y)).rgb;
                vec3 d = texture(screenTexture, vec2(TexCoords.x - 2*x, TexCoords.y)).rgb;
                vec3 e = texture(screenTexture, vec2(TexCoords.x,       TexCoords.y)).rgb;
                vec3 f = texture(screenTexture, vec2(TexCoords.x + 2*x, TexCoords.y)).rgb;
                vec3 g = texture(screenTexture, vec2(TexCoords.x - 2*x, TexCoords.y - 2*y)).rgb;
                vec3 h = texture(screenTexture, vec2(TexCoords.x,       TexCoords.y - 2*y)).rgb;
                vec3 i = texture(screenTexture, vec2(TexCoords.x + 2*x, TexCoords.y - 2*y)).rgb;
                vec3 j = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
                vec3 k = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
                vec3 l = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
                vec3 m = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;
                vec3 downsample = e*0.125;
                downsample += (a+c+g+i)*0.03125;
                downsample += (b+d+f+h)*0.0625;
                downsample += (j+k+l+m)*0.125;
                FragColor = vec4(downsample, 1.0);
            }
        )";
    } else if (effect_name == "bloom_upsample") {
        fs_src += R"(
            uniform float filterRadius;
            void main() {
                float x = filterRadius;
                float y = filterRadius;
                vec3 a = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y + y)).rgb;
                vec3 b = texture(screenTexture, vec2(TexCoords.x,     TexCoords.y + y)).rgb;
                vec3 c = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y + y)).rgb;
                vec3 d = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y)).rgb;
                vec3 e = texture(screenTexture, vec2(TexCoords.x,     TexCoords.y)).rgb;
                vec3 f = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y)).rgb;
                vec3 g = texture(screenTexture, vec2(TexCoords.x - x, TexCoords.y - y)).rgb;
                vec3 h = texture(screenTexture, vec2(TexCoords.x,     TexCoords.y - y)).rgb;
                vec3 i = texture(screenTexture, vec2(TexCoords.x + x, TexCoords.y - y)).rgb;
                vec3 upsample = e*4.0;
                upsample += (b+d+f+h)*2.0;
                upsample += (a+c+g+i);
                upsample *= 1.0 / 16.0;
                FragColor = vec4(upsample, 1.0);
            }
        )";
    } else if (effect_name == "bloom_extract") {
        fs_src += R"(
            uniform float threshold;
            void main() {
                vec3 color = texture(screenTexture, TexCoords).rgb;
                float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
                if(brightness > threshold)
                    FragColor = vec4(color, 1.0);
                else
                    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "bloom_blur_h") {
        fs_src += R"(
            uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
            void main() {
                vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
                vec3 result = texture(screenTexture, TexCoords).rgb * weight[0];
                for(int i = 1; i < 5; ++i) {
                    result += texture(screenTexture, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                    result += texture(screenTexture, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                }
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "bloom_blur_v") {
        fs_src += R"(
            uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
            void main() {
                vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
                vec3 result = texture(screenTexture, TexCoords).rgb * weight[0];
                for(int i = 1; i < 5; ++i) {
                    result += texture(screenTexture, TexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                    result += texture(screenTexture, TexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                }
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "lum_compute") {
        fs_src += R"(
            void main() {
                float logSum = 0.0;
                const int N = 64;
                for (int i = 0; i < 8; i++) {
                    for (int j = 0; j < 8; j++) {
                        vec2 uv = (vec2(float(i), float(j)) + 0.5) / 8.0;
                        vec3 c = texture(screenTexture, uv).rgb;
                        float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
                        logSum += log(max(lum, 0.0001));
                    }
                }
                float avgLogLum = logSum / float(N);
                FragColor = vec4(avgLogLum, 0.0, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "lum_adapt") {
        fs_src += R"(
            uniform sampler2D prevAdaptedTex;
            uniform float u_dt;
            uniform float u_speed_up;
            uniform float u_speed_down;
            uniform float u_min_exposure;
            uniform float u_max_exposure;
            uniform float u_compensation;
            void main() {
                float avgLogLum = texture(screenTexture, vec2(0.5, 0.5)).r;
                float avgLum = exp(avgLogLum);
                float targetExposure = 0.18 / max(avgLum, 0.001);
                targetExposure = clamp(targetExposure * exp2(u_compensation), u_min_exposure, u_max_exposure);
                float prevExposure = texture(prevAdaptedTex, vec2(0.5, 0.5)).r;
                if (prevExposure <= 0.0) prevExposure = targetExposure;
                float speed = (targetExposure > prevExposure) ? u_speed_up : u_speed_down;
                float adapted = prevExposure + (targetExposure - prevExposure) * (1.0 - exp(-u_dt * speed));
                FragColor = vec4(adapted, 0.0, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "tonemapping") {
        fs_src += R"(
            uniform sampler2D autoExposureTex;
            uniform sampler3D u_lut;
            uniform float u_manual_exposure;
            uniform float u_lut_intensity;
            uniform int u_auto_exposure_enabled;
            uniform int u_lut_enabled;
            vec3 AcesFilmic(vec3 x) {
                float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
                return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
            }
            void main() {
                vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
                float finalExposure = u_manual_exposure;
                if (u_auto_exposure_enabled != 0) {
                    finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
                }
                vec3 result = AcesFilmic(hdrColor * finalExposure);
                result = pow(result, vec3(1.0 / 2.2));
                if (u_lut_enabled != 0) {
                    vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
                    result = mix(result, lutColor, u_lut_intensity);
                }
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                result += (ign - 0.5) / 255.0;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "bloom_composite") {
        fs_src += R"(
            uniform sampler2D bloomBlur;
            uniform sampler2D ssaoTexture;
            uniform sampler2D contactShadowTex;
            uniform sampler2D autoExposureTex;
            uniform sampler3D u_lut;
            uniform float exposure;
            uniform float bloomIntensity;
            uniform float u_lut_intensity;
            uniform float u_vignette_intensity;
            uniform float u_vignette_radius;
            uniform float u_vignette_softness;
            uniform float u_film_grain_intensity;
            uniform float u_film_grain_time;
            uniform int u_bloom_enabled;
            uniform int u_ssao_enabled;
            uniform int u_contact_shadow_enabled;
            uniform float u_contact_shadow_strength;
            uniform int u_auto_exposure_enabled;
            uniform int u_lut_enabled;
            uniform int u_vignette_enabled;
            uniform int u_film_grain_enabled;
            vec3 AcesFilmic(vec3 x) {
                float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
                return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
            }
            float GrainNoise(vec2 uv, float time_seed) {
                return fract(sin(dot(uv + vec2(time_seed, time_seed * 0.37), vec2(12.9898, 78.233))) * 43758.5453);
            }
            void main() {
                vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
                if (u_ssao_enabled != 0) {
                    float ao = texture(ssaoTexture, TexCoords).r;
                    hdrColor *= ao;
                }
                if (u_bloom_enabled != 0) {
                    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;
                    hdrColor += bloomColor * bloomIntensity;
                }
                if (u_contact_shadow_enabled != 0) {
                    float cs = texture(contactShadowTex, TexCoords).r;
                    hdrColor *= (1.0 - (1.0 - cs) * u_contact_shadow_strength);
                }
                float finalExposure = exposure;
                if (u_auto_exposure_enabled != 0) {
                    finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
                }
                vec3 result = AcesFilmic(hdrColor * finalExposure);
                result = pow(result, vec3(1.0 / 2.2));
                if (u_lut_enabled != 0) {
                    vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
                    result = mix(result, lutColor, u_lut_intensity);
                }
                if (u_vignette_enabled != 0) {
                    float dist = length(TexCoords - vec2(0.5));
                    float radius = clamp(u_vignette_radius, 0.001, 1.5);
                    float softness = max(u_vignette_softness, 0.0001);
                    float vignette = 1.0 - smoothstep(radius, radius + softness, dist);
                    result *= mix(1.0, vignette, clamp(u_vignette_intensity, 0.0, 1.0));
                }
                if (u_film_grain_enabled != 0) {
                    float grain = GrainNoise(TexCoords * vec2(1280.0, 720.0), u_film_grain_time) - 0.5;
                    result = clamp(result + grain * u_film_grain_intensity, 0.0, 1.0);
                }
                // Anti color-banding dithering (IGN, +-0.5/255)
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                result += (ign - 0.5) / 255.0;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "ssao") {
        fs_src += R"(
            uniform float u_radius;
            uniform float u_bias;
            uniform float u_near;
            uniform float u_far;
            uniform vec2 u_screen_size;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            vec3 reconstructNormal(vec2 uv) {
                vec2 texel = 1.0 / u_screen_size;
                float dc = linearizeDepth(texture(screenTexture, uv).r);
                float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
                float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
                float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
                float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
                vec3 n = normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
                return n;
            }
            const vec3 kernel[16] = vec3[](
                vec3( 0.5381, 0.1856,-0.4319), vec3( 0.1379, 0.2486, 0.4430),
                vec3( 0.3371, 0.5679,-0.0057), vec3(-0.6999,-0.0451,-0.0019),
                vec3( 0.0689,-0.1598,-0.8547), vec3( 0.0560, 0.0069,-0.1843),
                vec3(-0.0146, 0.1402, 0.0762), vec3( 0.0100,-0.1924,-0.0344),
                vec3(-0.3577,-0.5301,-0.4358), vec3(-0.3169, 0.1063, 0.0158),
                vec3( 0.0103,-0.5869, 0.0046), vec3(-0.0897,-0.4940, 0.3287),
                vec3( 0.7119,-0.0154,-0.0918), vec3(-0.0533, 0.0596,-0.5411),
                vec3( 0.0352,-0.0631, 0.5460), vec3(-0.4776, 0.2847,-0.0271)
            );
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                if (depth >= 1.0) { FragColor = vec4(1.0); return; }
                float linDepth = linearizeDepth(depth);
                vec3 normal = reconstructNormal(TexCoords);
                float occlusion = 0.0;
                float rScale = u_radius / linDepth;
                for (int i = 0; i < 16; ++i) {
                    vec3 sampleDir = kernel[i];
                    if (dot(sampleDir, normal) < 0.0) sampleDir = -sampleDir;
                    vec2 sampleUV = TexCoords + sampleDir.xy * rScale * (1.0 / u_screen_size);
                    float sampleDepth = linearizeDepth(texture(screenTexture, sampleUV).r);
                    float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(linDepth - sampleDepth));
                    if (sampleDepth < linDepth - u_bias) occlusion += rangeCheck;
                }
                occlusion = 1.0 - (occlusion / 16.0);
                FragColor = vec4(vec3(occlusion), 1.0);
            }
        )";
    } else if (effect_name == "ssao_blur") {
        fs_src += R"(
            void main() {
                vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));
                float result = 0.0;
                for (int x = -2; x <= 2; ++x) {
                    for (int y = -2; y <= 2; ++y) {
                        vec2 offset = vec2(float(x), float(y)) * texelSize;
                        result += texture(screenTexture, TexCoords + offset).r;
                    }
                }
                FragColor = vec4(vec3(result / 25.0), 1.0);
            }
        )";
    } else if (effect_name == "ssao_apply") {
        fs_src += R"(
            uniform sampler2D ssaoTexture;
            uniform sampler2D autoExposureTex;
            uniform sampler3D u_lut;
            uniform float exposure;
            uniform float u_lut_intensity;
            uniform int u_auto_exposure_enabled;
            uniform int u_lut_enabled;
            vec3 AcesFilmic(vec3 x) {
                float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
                return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
            }
            void main() {
                vec3 hdrColor = texture(screenTexture, TexCoords).rgb;
                float ao = texture(ssaoTexture, TexCoords).r;
                hdrColor *= ao;
                float finalExposure = exposure;
                if (u_auto_exposure_enabled != 0) {
                    finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
                }
                vec3 result = AcesFilmic(hdrColor * finalExposure);
                result = pow(result, vec3(1.0 / 2.2));
                if (u_lut_enabled != 0) {
                    vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
                    result = mix(result, lutColor, u_lut_intensity);
                }
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                result += (ign - 0.5) / 255.0;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "contact_shadow") {
        fs_src += R"(
            uniform vec3 u_light_dir;
            uniform float u_near;
            uniform float u_far;
            uniform vec2 u_screen_size;
            uniform float u_strength;
            uniform float u_step_size;
            uniform int u_num_steps;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                if (depth >= 1.0) { FragColor = vec4(1.0); return; }
                float linDepth = linearizeDepth(depth);
                vec3 lightDir = normalize(u_light_dir);
                vec2 texelSize = 1.0 / u_screen_size;
                float occlusion = 0.0;
                int validSteps = 0;
                for (int i = 1; i <= u_num_steps; ++i) {
                    float dist = u_step_size * float(i);
                    vec2 sampleUV = TexCoords + lightDir.xy * texelSize * dist * 50.0;
                    if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0) break;
                    float sampleDepth = texture(screenTexture, sampleUV).r;
                    if (sampleDepth >= 1.0) continue;
                    float sampleLin = linearizeDepth(sampleDepth);
                    float diff = sampleLin - linDepth;
                    if (diff > 0.0 && diff < u_step_size) {
                        float k = 1.0 - (diff / u_step_size);
                        occlusion += k * k;
                    }
                    ++validSteps;
                }
                float shadow = validSteps > 0 ? 1.0 - clamp(occlusion / float(validSteps) * u_strength, 0.0, 1.0) : 1.0;
                FragColor = vec4(vec3(shadow), 1.0);
            }
        )";
    } else if (effect_name == "color_grading") {
        fs_src += R"(
            uniform sampler3D u_lut;
            uniform float u_lut_intensity;
            void main() {
                vec3 color = texture(screenTexture, TexCoords).rgb;
                vec3 lutColor = texture(u_lut, clamp(color, 0.0, 1.0)).rgb;
                color = mix(color, lutColor, u_lut_intensity);
                float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
                color += (ign - 0.5) / 255.0;
                FragColor = vec4(color, 1.0);
            }
        )";
    } else if (effect_name == "fxaa") {
        fs_src += R"(
            uniform vec2 u_resolution;
            float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
            void main() {
                vec2 texel = 1.0 / u_resolution;
                float lumaM  = luma(texture(screenTexture, TexCoords).rgb);
                float lumaNW = luma(texture(screenTexture, TexCoords + vec2(-1.0,-1.0) * texel).rgb);
                float lumaNE = luma(texture(screenTexture, TexCoords + vec2( 1.0,-1.0) * texel).rgb);
                float lumaSW = luma(texture(screenTexture, TexCoords + vec2(-1.0, 1.0) * texel).rgb);
                float lumaSE = luma(texture(screenTexture, TexCoords + vec2( 1.0, 1.0) * texel).rgb);
                float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
                float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
                float lumaRange = lumaMax - lumaMin;
                if (lumaRange < max(0.0312, lumaMax * 0.125)) {
                    FragColor = texture(screenTexture, TexCoords);
                    return;
                }
                vec2 dir;
                dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
                dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
                float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.25, 1.0/128.0);
                float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
                dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * texel;
                vec3 rgbA = 0.5 * (
                    texture(screenTexture, TexCoords + dir * (1.0/3.0 - 0.5)).rgb +
                    texture(screenTexture, TexCoords + dir * (2.0/3.0 - 0.5)).rgb);
                vec3 rgbB = rgbA * 0.5 + 0.25 * (
                    texture(screenTexture, TexCoords + dir * -0.5).rgb +
                    texture(screenTexture, TexCoords + dir *  0.5).rgb);
                float lumaB = luma(rgbB);
                if (lumaB < lumaMin || lumaB > lumaMax)
                    FragColor = vec4(rgbA, 1.0);
                else
                    FragColor = vec4(rgbB, 1.0);
            }
        )";
    } else if (effect_name == "dof") {
        fs_src += R"(
            uniform sampler2D u_color_texture;
            uniform float u_focus_distance;
            uniform float u_focus_range;
            uniform float u_bokeh_radius;
            uniform float u_near;
            uniform float u_far;
            uniform float u_screen_w;
            uniform float u_screen_h;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                float lin_depth = linearizeDepth(depth);
                float coc = clamp(abs(lin_depth - u_focus_distance) / u_focus_range, 0.0, 1.0);
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                float radius = coc * u_bokeh_radius;
                vec3 color = vec3(0.0);
                float total_weight = 0.0;
                const int SAMPLES = 16;
                const float GOLDEN_ANGLE = 2.39996323;
                for (int i = 0; i < SAMPLES; ++i) {
                    float r = sqrt(float(i) / float(SAMPLES)) * radius;
                    float theta = float(i) * GOLDEN_ANGLE;
                    vec2 offset = vec2(cos(theta), sin(theta)) * r * texel;
                    float sd = linearizeDepth(texture(screenTexture, TexCoords + offset).r);
                    float sc = clamp(abs(sd - u_focus_distance) / u_focus_range, 0.0, 1.0);
                    float w = max(sc, coc);
                    color += texture(u_color_texture, TexCoords + offset).rgb * w;
                    total_weight += w;
                }
                if (total_weight > 0.0) color /= total_weight;
                else color = texture(u_color_texture, TexCoords).rgb;
                FragColor = vec4(color, 1.0);
            }
        )";
    } else if (effect_name == "motion_vector") {
        fs_src += R"(
            uniform float u_screen_w;
            uniform float u_screen_h;
            uniform mat4 u_reproj;
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                vec2 ndc = TexCoords * 2.0 - 1.0;
                float z_ndc = depth * 2.0 - 1.0;
                vec4 clip_pos = vec4(ndc, z_ndc, 1.0);
                vec4 prev_clip = u_reproj * clip_pos;
                prev_clip.xy /= prev_clip.w;
                vec2 prev_uv = prev_clip.xy * 0.5 + 0.5;
                vec2 velocity = TexCoords - prev_uv;
                FragColor = vec4(velocity, 0.0, 1.0);
            }
        )";
    } else if (effect_name == "motion_blur") {
        fs_src += R"(
            uniform sampler2D u_color_texture;
            uniform float u_intensity;
            uniform int u_samples;
            uniform float u_screen_w;
            uniform float u_screen_h;
            void main() {
                // screenTexture = motion_vector RT (rg = velocity)
                vec2 velocity = texture(screenTexture, TexCoords).rg * u_intensity;
                vec3 color = texture(u_color_texture, TexCoords).rgb;
                float total = 1.0;
                for (int i = 1; i < u_samples; ++i) {
                    float t = float(i) / float(u_samples);
                    vec2 sample_uv = TexCoords + velocity * t;
                    if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && sample_uv.y >= 0.0 && sample_uv.y <= 1.0) {
                        color += texture(u_color_texture, sample_uv).rgb;
                        total += 1.0;
                    }
                }
                FragColor = vec4(color / total, 1.0);
            }
        )";
    } else if (effect_name == "ssr") {
        fs_src += R"(
            uniform sampler2D u_color_texture;
            uniform float u_max_distance;
            uniform float u_thickness;
            uniform float u_step_size;
            uniform int u_max_steps;
            uniform float u_near;
            uniform float u_far;
            uniform float u_screen_w;
            uniform float u_screen_h;
            float linearizeDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }
            vec3 reconstructNormal(vec2 uv) {
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                float dc = linearizeDepth(texture(screenTexture, uv).r);
                float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
                float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
                float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
                float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
                return normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
            }
            void main() {
                float depth = texture(screenTexture, TexCoords).r;
                if (depth >= 1.0) { FragColor = vec4(0.0); return; }
                float lin_depth = linearizeDepth(depth);
                vec3 normal = reconstructNormal(TexCoords);
                vec3 view_dir = normalize(vec3(TexCoords * 2.0 - 1.0, 1.0));
                vec3 reflect_dir = reflect(view_dir, normal);
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                vec2 ray_uv = TexCoords;
                float ray_depth = lin_depth;
                for (int i = 0; i < u_max_steps; ++i) {
                    ray_uv += reflect_dir.xy * texel * u_step_size;
                    if (ray_uv.x < 0.0 || ray_uv.x > 1.0 || ray_uv.y < 0.0 || ray_uv.y > 1.0) break;
                    float sd = linearizeDepth(texture(screenTexture, ray_uv).r);
                    ray_depth += reflect_dir.z * u_step_size;
                    float dd = ray_depth - sd;
                    if (dd > 0.0 && dd < u_thickness) {
                        float fade = 1.0 - float(i) / float(u_max_steps);
                        vec3 hit_color = texture(u_color_texture, ray_uv).rgb;
                        FragColor = vec4(hit_color * fade, fade);
                        return;
                    }
                }
                FragColor = vec4(0.0);
            }
        )";
    } else if (effect_name == "taa_resolve") {
        fs_src += R"(
            uniform sampler2D u_history;
            uniform sampler2D u_motion_vector;
            uniform float u_blend_factor;
            uniform float u_jitter_x;
            uniform float u_jitter_y;
            uniform int u_frame_index;
            uniform float u_screen_w;
            uniform float u_screen_h;
            void main() {
                vec3 current = texture(screenTexture, TexCoords).rgb;
                vec2 mv = texture(u_motion_vector, TexCoords).rg;
                vec2 history_uv = TexCoords - mv - vec2(u_jitter_x, u_jitter_y);
                history_uv = clamp(history_uv, vec2(0.0), vec2(1.0));
                vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
                vec3 m1 = vec3(0.0), m2 = vec3(0.0);
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        vec3 s = texture(screenTexture, TexCoords + vec2(dx, dy) * texel).rgb;
                        m1 += s; m2 += s * s;
                    }
                }
                m1 /= 9.0;
                vec3 sigma = sqrt(max(m2 / 9.0 - m1 * m1, vec3(0.0)));
                vec3 history = texture(u_history, history_uv).rgb;
                history = clamp(history, m1 - 1.25 * sigma, m1 + 1.25 * sigma);
                float velocity_len = length(mv * vec2(u_screen_w, u_screen_h));
                float vel_weight = clamp(velocity_len * 0.5, 0.0, 0.5);
                float alpha = (u_frame_index < 2) ? 1.0 : clamp(u_blend_factor + vel_weight, u_blend_factor, 1.0);
                FragColor = vec4(mix(history, current, alpha), 1.0);
            }
        )";
    } else if (effect_name == "deferred_lighting") {
        fs_src += R"(
            uniform sampler2D u_gbuf_normal;
            uniform sampler2D u_gbuf_position;
            uniform vec3 u_light_dir;
            uniform vec3 u_light_color;
            uniform float u_light_intensity;
            uniform float u_ambient;
            void main() {
                vec3 albedo   = texture(screenTexture, TexCoords).rgb;
                vec3 normal   = texture(u_gbuf_normal, TexCoords).rgb * 2.0 - 1.0;
                vec3 position = texture(u_gbuf_position, TexCoords).rgb;
                if (length(normal) < 0.01) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
                normal = normalize(normal);
                float NdotL = max(dot(normal, -normalize(u_light_dir)), 0.0);
                vec3 diffuse = albedo * u_light_color * u_light_intensity * NdotL;
                vec3 ambient = albedo * u_ambient;
                FragColor = vec4(diffuse + ambient, 1.0);
            }
        )";
    } else if (effect_name == "edge_detect") {
        fs_src += R"(
            uniform float u_thickness;
            uniform float u_depth_threshold;
            uniform float u_normal_threshold;
            uniform vec3 u_outline_color;
            uniform float u_near;
            uniform float u_far;
            uniform float u_screen_w;
            uniform float u_screen_h;

            float linearize_depth(float d) {
                float ndc = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - ndc * (u_far - u_near));
            }

            vec3 reconstruct_normal(vec2 uv, vec2 texel_size) {
                float dc = linearize_depth(texture(screenTexture, uv).r);
                float dl = linearize_depth(texture(screenTexture, uv - vec2(texel_size.x, 0.0)).r);
                float dr = linearize_depth(texture(screenTexture, uv + vec2(texel_size.x, 0.0)).r);
                float db = linearize_depth(texture(screenTexture, uv - vec2(0.0, texel_size.y)).r);
                float dt = linearize_depth(texture(screenTexture, uv + vec2(0.0, texel_size.y)).r);
                return normalize(vec3(dl - dr, db - dt, 2.0 * texel_size.x * dc));
            }

            void main() {
                vec2 base_texel = vec2(1.0 / u_screen_w, 1.0 / u_screen_h);
                vec2 texel = base_texel * u_thickness;

                float d_c = linearize_depth(texture(screenTexture, TexCoords).r);
                float d_l = linearize_depth(texture(screenTexture, TexCoords + vec2(-texel.x, 0.0)).r);
                float d_r = linearize_depth(texture(screenTexture, TexCoords + vec2( texel.x, 0.0)).r);
                float d_t = linearize_depth(texture(screenTexture, TexCoords + vec2(0.0,  texel.y)).r);
                float d_b = linearize_depth(texture(screenTexture, TexCoords + vec2(0.0, -texel.y)).r);

                float depth_diff = abs(d_l - d_r) + abs(d_t - d_b);
                float depth_edge = smoothstep(0.0, u_depth_threshold * d_c, depth_diff);

                vec3 n_c = reconstruct_normal(TexCoords, base_texel);
                vec3 n_l = reconstruct_normal(TexCoords + vec2(-texel.x, 0.0), base_texel);
                vec3 n_r = reconstruct_normal(TexCoords + vec2( texel.x, 0.0), base_texel);
                vec3 n_t = reconstruct_normal(TexCoords + vec2(0.0,  texel.y), base_texel);
                vec3 n_b = reconstruct_normal(TexCoords + vec2(0.0, -texel.y), base_texel);
                float normal_diff = length(n_l - n_r) + length(n_t - n_b);
                float normal_edge = smoothstep(0.0, u_normal_threshold, normal_diff);

                float edge = clamp(max(depth_edge, normal_edge), 0.0, 1.0);
                FragColor = vec4(u_outline_color, edge);
            }
        )";
    } else if (effect_name == "light_shaft") {
        fs_src += R"(
            uniform sampler2D u_depth_tex;
            uniform vec2 u_sun_screen;
            uniform vec3 u_light_color;
            uniform float u_density;
            uniform float u_weight;
            uniform float u_decay;
            uniform float u_exposure;
            uniform float u_num_samples;
            uniform float u_intensity;

            void main() {
                vec4 scene = texture(screenTexture, TexCoords);
                int samples = int(u_num_samples);
                vec2 delta_uv = (u_sun_screen - TexCoords) * u_density / float(samples);

                vec2 uv = TexCoords;
                float illum_decay = 1.0;
                vec3 accumulated = vec3(0.0);

                for (int i = 0; i < samples; i++) {
                    uv += delta_uv;
                    vec2 suv = clamp(uv, 0.001, 0.999);
                    float d = texture(u_depth_tex, suv).r;
                    vec3 s = texture(screenTexture, suv).rgb;
                    float sky = step(0.9999, d);
                    float lum = dot(s, vec3(0.2126, 0.7152, 0.0722));
                    float bright = smoothstep(0.8, 1.2, lum);
                    float mask = max(sky, bright);
                    accumulated += s * mask * illum_decay * u_weight;
                    illum_decay *= u_decay;
                    if (illum_decay < 0.003) break;
                }

                vec3 result = scene.rgb + accumulated * u_exposure * u_light_color * u_intensity;
                FragColor = vec4(result, 1.0);
            }
        )";
    } else if (effect_name == "ui_overlay") {
        fs_src += R"(
            void main() {
                FragColor = texture(screenTexture, TexCoords);
            }
        )";
    } else if (effect_name == "volumetric_fog") {
        fs_src += R"(
            uniform sampler2D u_depth_tex;
            uniform vec3 u_fog_color;
            uniform float u_fog_density;
            uniform float u_height_falloff;
            uniform float u_height_offset;
            uniform float u_fog_start;
            uniform float u_fog_end;
            uniform float u_fog_steps;
            uniform float u_sun_scatter;
            uniform vec3 u_sun_dir;
            uniform vec3 u_camera_pos;
            uniform float u_near;
            uniform float u_far;
            uniform vec3 u_cam_right;
            uniform vec3 u_cam_up;
            uniform vec3 u_cam_fwd;
            uniform float u_tan_fov_y;
            uniform float u_aspect;

            float VFogLinearDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }

            void main() {
                vec4 scene = texture(screenTexture, TexCoords);
                float depth = texture(u_depth_tex, TexCoords).r;
                if (depth >= 0.9999) { FragColor = scene; return; }

                float viewZ = VFogLinearDepth(depth);
                vec2 ndc = TexCoords * 2.0 - 1.0;
                vec3 viewDir = normalize(u_cam_fwd
                    + ndc.x * u_cam_right * u_tan_fov_y * u_aspect
                    + ndc.y * u_cam_up    * u_tan_fov_y);
                float cosAngle = max(dot(viewDir, u_cam_fwd), 0.0001);
                float rayLen   = viewZ / cosAngle;

                float marchStart = u_fog_start;
                float marchEnd   = min(rayLen, u_fog_end);
                float steps = max(u_fog_steps, 1.0);
                if (marchEnd <= marchStart) { FragColor = scene; return; }

                float stepLen  = (marchEnd - marchStart) / steps;
                float cosTheta = dot(viewDir, -u_sun_dir);
                float g = 0.76; float g2 = g * g;
                float mie = (1.0 - g2) / (4.0 * 3.14159265 *
                    pow(max(1.0 + g2 - 2.0 * g * cosTheta, 0.001), 1.5));

                float transmittance = 1.0;
                vec3 inscatter = vec3(0.0);
                for (float i = 0.0; i < steps; i += 1.0) {
                    float t   = marchStart + (i + 0.5) * stepLen;
                    vec3 pos  = u_camera_pos + viewDir * t;
                    float h   = max(pos.y - u_height_offset, 0.0);
                    float den = u_fog_density * exp(-u_height_falloff * h);
                    float sT  = exp(-den * stepLen);
                    inscatter += transmittance * (1.0 - sT) *
                        (u_fog_color + mie * u_sun_scatter * vec3(1.0));
                    transmittance *= sT;
                    if (transmittance < 0.001) break;
                }
                FragColor = vec4(scene.rgb * transmittance + inscatter, scene.a);
            }
        )";
    } else if (effect_name == "wboit_composite") {
        fs_src += R"(
            uniform sampler2D u_reveal_tex;

            void main() {
                vec4 accum = texture(screenTexture, TexCoords);
                float revealage = texture(u_reveal_tex, TexCoords).r;

                // No transparent fragments: early out
                if (accum.a < 1e-4) discard;

                vec3 avg_color = accum.rgb / max(accum.a, 1e-5);
                FragColor = vec4(avg_color, 1.0 - revealage);
            }
        )";
    } else if (effect_name == "water") {
        fs_src += R"(
            uniform sampler2D u_depth_tex;
            uniform float u_water_level;
            uniform vec3  u_deep_color;
            uniform vec3  u_shallow_color;
            uniform float u_max_depth;
            uniform float u_transparency;
            uniform float u_wave_amplitude;
            uniform float u_wave_frequency;
            uniform float u_wave_speed;
            uniform vec2  u_wave_dir;
            uniform float u_refraction_strength;
            uniform float u_specular_power;
            uniform float u_reflection_strength;
            uniform float u_time;
            uniform vec3  u_sun_dir;
            uniform vec3  u_camera_pos;
            uniform float u_near;
            uniform float u_far;
            uniform vec3  u_cam_fwd;
            uniform float u_tan_fov_y;
            uniform float u_aspect;
            uniform float u_caustic_intensity;
            uniform float u_caustic_scale;
            uniform float u_foam_intensity;
            uniform float u_foam_depth_threshold;
            uniform float u_uw_fog_density;
            uniform vec3  u_uw_fog_color;

            float WaterLinearDepth(float d) {
                float z = d * 2.0 - 1.0;
                return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
            }

            vec3 GerstnerNormal(vec2 xz, float t) {
                float k = u_wave_frequency;
                float a = u_wave_amplitude;
                float sp = u_wave_speed;
                vec2 d1 = u_wave_dir;
                vec2 d2 = vec2(-d1.y, d1.x);
                float p1 = dot(d1, xz) * k - t * sp;
                float p2 = dot(d2, xz) * k * 1.3 - t * sp * 0.7;
                float dx = -k * a * (d1.x * cos(p1) + d2.x * 0.5 * cos(p2));
                float dz = -k * a * (d1.y * cos(p1) + d2.y * 0.5 * cos(p2));
                return normalize(vec3(-dx, 1.0, -dz));
            }

            void main() {
                vec4 scene = texture(screenTexture, TexCoords);
                float depth_raw = texture(u_depth_tex, TexCoords).r;

                vec3 worldUp = vec3(0.0, 1.0, 0.0);
                vec3 camRight = normalize(cross(worldUp, u_cam_fwd));
                vec3 camUp = cross(u_cam_fwd, camRight);
                vec2 ndc = TexCoords * 2.0 - 1.0;
                vec3 rayDir = normalize(u_cam_fwd
                    + ndc.x * camRight * u_tan_fov_y * u_aspect
                    + ndc.y * camUp    * u_tan_fov_y);

                // ray-plane intersection: y = water_level
                float denom = rayDir.y;
                if (abs(denom) < 1e-6) { FragColor = scene; return; }
                float t_hit = (u_water_level - u_camera_pos.y) / denom;
                if (t_hit < 0.0) { FragColor = scene; return; }

                // scene depth in world units
                float scene_linear = (depth_raw < 0.9999)
                    ? WaterLinearDepth(depth_raw) / max(dot(rayDir, u_cam_fwd), 0.0001)
                    : 1e6;

                if (t_hit > scene_linear) { FragColor = scene; return; }

                vec3 water_world = u_camera_pos + rayDir * t_hit;

                // underwater depth for coloring
                float underwater_depth = max(scene_linear - t_hit, 0.0);
                float depth_factor = clamp(underwater_depth / max(u_max_depth, 0.01), 0.0, 1.0);
                vec3 water_color = mix(u_shallow_color, u_deep_color, depth_factor);

                // refraction: offset UV by wave normal
                vec3 wave_normal = GerstnerNormal(water_world.xz, u_time);
                vec2 refract_offset = wave_normal.xz * u_refraction_strength;
                vec2 refract_uv = clamp(TexCoords + refract_offset, 0.0, 1.0);
                vec3 refracted = texture(screenTexture, refract_uv).rgb;

                // Fresnel (Schlick approximation)
                float cos_theta = max(dot(-rayDir, wave_normal), 0.0);
                float fresnel = u_reflection_strength + (1.0 - u_reflection_strength) * pow(1.0 - cos_theta, 5.0);

                // simple sky reflection
                vec3 reflected_dir = reflect(rayDir, wave_normal);
                float sky_grad = clamp(reflected_dir.y * 0.5 + 0.5, 0.0, 1.0);
                vec3 sky_color = mix(vec3(0.3, 0.4, 0.5), vec3(0.6, 0.75, 1.0), sky_grad);

                // specular highlight
                vec3 half_vec = normalize(-rayDir + (-u_sun_dir));
                float spec = pow(max(dot(wave_normal, half_vec), 0.0), u_specular_power);
                vec3 specular = vec3(1.0) * spec;

                // caustics: dual-layer Voronoi noise projected on underwater surfaces
                vec3 caustic = vec3(0.0);
                if (u_caustic_intensity > 0.001) {
                    vec2 cUV = water_world.xz / u_caustic_scale;
                    float v1 = 1.0, v2 = 1.0;
                    for (int ci = 0; ci < 2; ci++) {
                        float speed = (ci == 0) ? 0.4 : -0.3;
                        vec2 uvc = cUV + vec2(u_time * speed, u_time * speed * 0.7);
                        vec2 cell = floor(uvc);
                        vec2 frac_uv = fract(uvc);
                        float minD = 1.0;
                        for (int y = -1; y <= 1; y++) {
                            for (int x = -1; x <= 1; x++) {
                                vec2 nb = vec2(float(x), float(y));
                                vec2 h = fract(sin(vec2(
                                    dot(cell + nb, vec2(127.1, 311.7)),
                                    dot(cell + nb, vec2(269.5, 183.3))
                                )) * 43758.5453);
                                vec2 diff = nb + h - frac_uv;
                                minD = min(minD, dot(diff, diff));
                            }
                        }
                        if (ci == 0) v1 = minD; else v2 = minD;
                    }
                    float pattern = clamp(pow(min(v1, v2), 0.5) * 2.0, 0.0, 1.0);
                    pattern = 1.0 - pattern;
                    pattern = pow(pattern, 2.5);
                    caustic = vec3(pattern) * u_caustic_intensity * (1.0 - depth_factor);
                }

                // foam: white fringe at shallow depths
                float foam = 0.0;
                if (u_foam_intensity > 0.001) {
                    foam = (1.0 - smoothstep(0.0, u_foam_depth_threshold, underwater_depth)) * u_foam_intensity;
                    float foam_noise = fract(sin(dot(water_world.xz * 5.0 + u_time * 0.3, vec2(12.9898, 78.233))) * 43758.5453);
                    foam *= (0.6 + 0.4 * foam_noise);
                }

                // combine
                vec3 underwater = mix(refracted, water_color, depth_factor * u_transparency) + caustic;
                vec3 surface = mix(underwater, sky_color, fresnel) + specular + vec3(foam);

                // underwater fog: when camera is below water level
                if (u_camera_pos.y < u_water_level && u_uw_fog_density > 0.001) {
                    float fog_dist = length(water_world - u_camera_pos);
                    float fog_factor = 1.0 - exp(-u_uw_fog_density * fog_dist);
                    surface = mix(surface, u_uw_fog_color, clamp(fog_factor, 0.0, 1.0));
                }

                float edge_fade = smoothstep(0.0, 0.5, underwater_depth);
                float alpha = u_transparency * edge_fade;
                FragColor = vec4(mix(scene.rgb, surface, alpha), scene.a);
            }
        )";
    } else if (effect_name == "decal") {
        fs_src += R"(
            uniform sampler2D u_depth_tex;
            uniform sampler2D u_decal_tex;
            uniform mat4 u_inv_model_vp;
            uniform vec4 u_color;
            uniform float u_angle_fade;
            uniform vec3 u_decal_up;

            void main() {
                float depth = texture(u_depth_tex, TexCoords).r;
                if (depth >= 0.9999) discard;

                vec4 clip = vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
                vec4 local4 = u_inv_model_vp * clip;
                vec3 local = local4.xyz / local4.w;

                if (abs(local.x) > 0.5 || abs(local.y) > 0.5 || abs(local.z) > 0.5) discard;

                vec2 decal_uv = local.xz + 0.5;
                vec4 decal = texture(u_decal_tex, decal_uv) * u_color;

                float angle_factor = 1.0;
                if (u_angle_fade > 0.0) {
                    vec2 texel = 1.0 / textureSize(u_depth_tex, 0);
                    float dl = texture(u_depth_tex, TexCoords + vec2(-texel.x, 0.0)).r;
                    float dr = texture(u_depth_tex, TexCoords + vec2( texel.x, 0.0)).r;
                    float dt = texture(u_depth_tex, TexCoords + vec2(0.0,  texel.y)).r;
                    float db = texture(u_depth_tex, TexCoords + vec2(0.0, -texel.y)).r;
                    vec3 normal = normalize(vec3(dl - dr, dt - db, 2.0 * texel.x));
                    float facing = abs(dot(normal, u_decal_up));
                    angle_factor = smoothstep(0.0, 1.0 - u_angle_fade, facing);
                }
                FragColor = vec4(decal.rgb, decal.a * angle_factor);
            }
        )";
    } else {
        fs_src += R"(
            void main() {
                FragColor = texture(screenTexture, TexCoords);
            }
        )";
    }

    unsigned int shader = shader_mgr.GetOrCreatePostProcessShader(effect_name, vs_src, fs_src);
    glUseProgram(shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_texture);
    glUniform1i(glGetUniformLocation(shader, "screenTexture"), 0);

    if (effect_name == "bloom_extract" && params.size() >= 1) {
        glUniform1f(glGetUniformLocation(shader, "threshold"), params[0]);
    } else if (effect_name == "bloom_downsample" && params.size() >= 2) {
        glUniform2f(glGetUniformLocation(shader, "srcResolution"), params[0], params[1]);
    } else if (effect_name == "bloom_upsample" && params.size() >= 1) {
        glUniform1f(glGetUniformLocation(shader, "filterRadius"), params[0]);
    } else if (effect_name == "lum_adapt" && params.size() >= 6) {
        const unsigned int prev_tex = request.FindTex(2);
        if (prev_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, prev_tex);
            glUniform1i(glGetUniformLocation(shader, "prevAdaptedTex"), 1);
        }
        glUniform1f(glGetUniformLocation(shader, "u_dt"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_speed_up"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_speed_down"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_min_exposure"), params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_max_exposure"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_compensation"), params[5]);
    } else if (effect_name == "tonemapping" && params.size() >= 1) {
        glUniform1f(glGetUniformLocation(shader, "u_manual_exposure"), params[0]);
        const unsigned int ae_tex_id = request.FindTex(2);
        if (ae_tex_id != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ae_tex_id);
            glUniform1i(glGetUniformLocation(shader, "autoExposureTex"), 1);
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 0);
        }
        const unsigned int lut_tex_id = request.FindTex(5);
        if (lut_tex_id != 0) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_3D, lut_tex_id);
            glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
            if (params.size() >= 2) glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"), params[1]);
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 0);
        }
    } else if (effect_name == "color_grading" && params.size() >= 2) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_3D, static_cast<unsigned int>(params[0]));
        glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
        glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"), params[1]);
    } else if (effect_name == "bloom_composite") {
        const CompositeParamsView composite(params);
        const bool bloom_enabled = composite.Flag(CompositeParamsView::kBloomEnabled) &&
                                   composite.Texture(CompositeParamsView::kBloomTex) != 0;
        if (bloom_enabled) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, composite.Texture(CompositeParamsView::kBloomTex));
            glUniform1i(glGetUniformLocation(shader, "bloomBlur"), 1);
            glUniform1i(glGetUniformLocation(shader, "u_bloom_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_bloom_enabled"), 0);
        }
        glUniform1f(glGetUniformLocation(shader, "exposure"),
                    composite.Float(CompositeParamsView::kExposure, 1.0f));
        glUniform1f(glGetUniformLocation(shader, "bloomIntensity"),
                    composite.Float(CompositeParamsView::kBloomIntensity, 0.5f));
        const unsigned int ssao_tex = composite.Texture(CompositeParamsView::kSsaoTex);
        if (ssao_tex != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ssao_tex);
            glUniform1i(glGetUniformLocation(shader, "ssaoTexture"), 2);
            glUniform1i(glGetUniformLocation(shader, "u_ssao_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_ssao_enabled"), 0);
        }
        const unsigned int auto_exposure_tex = composite.Texture(CompositeParamsView::kAutoExposureTex);
        if (auto_exposure_tex != 0) {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, auto_exposure_tex);
            glUniform1i(glGetUniformLocation(shader, "autoExposureTex"), 3);
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 0);
        }
        const unsigned int lut_tex = composite.Texture(CompositeParamsView::kLutTex);
        if (lut_tex != 0) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_3D, lut_tex);
            glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
            glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"),
                        composite.Float(CompositeParamsView::kLutIntensity, 0.0f));
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 0);
        }
        const unsigned int contact_shadow_tex = composite.Texture(CompositeParamsView::kContactShadowTex);
        if (contact_shadow_tex != 0) {
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, contact_shadow_tex);
            glUniform1i(glGetUniformLocation(shader, "contactShadowTex"), 5);
            glUniform1i(glGetUniformLocation(shader, "u_contact_shadow_enabled"), 1);
            glUniform1f(glGetUniformLocation(shader, "u_contact_shadow_strength"),
                        composite.Float(CompositeParamsView::kContactShadowStrength, 0.0f));
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_contact_shadow_enabled"), 0);
        }
        const bool vignette_enabled = composite.Flag(CompositeParamsView::kVignetteEnabled);
        glUniform1i(glGetUniformLocation(shader, "u_vignette_enabled"), vignette_enabled ? 1 : 0);
        if (vignette_enabled) {
            glUniform1f(glGetUniformLocation(shader, "u_vignette_intensity"),
                        composite.Float(CompositeParamsView::kVignetteIntensity, 0.0f));
            glUniform1f(glGetUniformLocation(shader, "u_vignette_radius"),
                        composite.Float(CompositeParamsView::kVignetteRadius, 0.75f));
            glUniform1f(glGetUniformLocation(shader, "u_vignette_softness"),
                        composite.Float(CompositeParamsView::kVignetteSoftness, 0.35f));
        }
        const bool film_grain_enabled = composite.Flag(CompositeParamsView::kFilmGrainEnabled);
        glUniform1i(glGetUniformLocation(shader, "u_film_grain_enabled"), film_grain_enabled ? 1 : 0);
        if (film_grain_enabled) {
            glUniform1f(glGetUniformLocation(shader, "u_film_grain_intensity"),
                        composite.Float(CompositeParamsView::kFilmGrainIntensity, 0.0f));
            glUniform1f(glGetUniformLocation(shader, "u_film_grain_time"),
                        composite.Float(CompositeParamsView::kFilmGrainTime, 0.0f));
        }
    } else if (effect_name == "ssao" && params.size() >= 6) {
        glUniform1f(glGetUniformLocation(shader, "u_radius"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_bias"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[3]);
        glUniform2f(glGetUniformLocation(shader, "u_screen_size"), params[4], params[5]);
    } else if (effect_name == "contact_shadow" && params.size() >= 10) {
        glUniform3f(glGetUniformLocation(shader, "u_light_dir"), params[0], params[1], params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[4]);
        glUniform2f(glGetUniformLocation(shader, "u_screen_size"), params[5], params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_strength"), params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_step_size"), params[9]);
        glUniform1i(glGetUniformLocation(shader, "u_num_steps"), static_cast<int>(params[8]));
    } else if (effect_name == "ssao_apply" && params.size() >= 1) {
        const unsigned int ssao_tex = request.FindTex(2);
        if (ssao_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, ssao_tex);
            glUniform1i(glGetUniformLocation(shader, "ssaoTexture"), 1);
        }
        glUniform1f(glGetUniformLocation(shader, "exposure"), params[0]);
        const unsigned int ae_tex_id = request.FindTex(3);
        if (ae_tex_id != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, ae_tex_id);
            glUniform1i(glGetUniformLocation(shader, "autoExposureTex"), 2);
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_auto_exposure_enabled"), 0);
        }
        const unsigned int lut_tex_id = request.FindTex(5);
        if (lut_tex_id != 0) {
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_3D, lut_tex_id);
            glUniform1i(glGetUniformLocation(shader, "u_lut"), 4);
            if (params.size() >= 2) glUniform1f(glGetUniformLocation(shader, "u_lut_intensity"), params[1]);
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 1);
        } else {
            glUniform1i(glGetUniformLocation(shader, "u_lut_enabled"), 0);
        }
    } else if (effect_name == "dof" && params.size() >= 7) {
        glUniform1f(glGetUniformLocation(shader, "u_focus_distance"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_focus_range"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_bokeh_radius"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[6]);
        const unsigned int color_tex = request.FindTex(2);
        if (color_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, color_tex);
            glUniform1i(glGetUniformLocation(shader, "u_color_texture"), 1);
        }
    } else if (effect_name == "motion_vector" && params.size() >= 18) {
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[1]);
        glUniformMatrix4fv(glGetUniformLocation(shader, "u_reproj"), 1, GL_FALSE, &params[2]);
    } else if (effect_name == "motion_blur" && params.size() >= 4) {
        glUniform1f(glGetUniformLocation(shader, "u_intensity"), params[0]);
        glUniform1i(glGetUniformLocation(shader, "u_samples"), static_cast<int>(params[1]));
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[3]);
        const unsigned int color_tex = request.FindTex(2);
        if (color_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, color_tex);
            glUniform1i(glGetUniformLocation(shader, "u_color_texture"), 1);
        }
    } else if (effect_name == "ssr" && params.size() >= 8) {
        glUniform1f(glGetUniformLocation(shader, "u_max_distance"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_thickness"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_step_size"), params[2]);
        glUniform1i(glGetUniformLocation(shader, "u_max_steps"), static_cast<int>(params[3]));
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[7]);
        const unsigned int color_tex = request.FindTex(2);
        if (color_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, color_tex);
            glUniform1i(glGetUniformLocation(shader, "u_color_texture"), 1);
        }
    } else if (effect_name == "fxaa" && params.size() >= 2) {
        glUniform2f(glGetUniformLocation(shader, "u_resolution"), params[0], params[1]);
    } else if (effect_name == "taa_resolve" && params.size() >= 6) {
        // params: [blend_factor, jitter_x, jitter_y, frame_index, screen_w, screen_h]
        const unsigned int history_tex = request.FindTex(5);
        const unsigned int mv_tex = request.FindTex(2);
        if (history_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, history_tex);
            glUniform1i(glGetUniformLocation(shader, "u_history"), 1);
        }
        if (mv_tex != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mv_tex);
            glUniform1i(glGetUniformLocation(shader, "u_motion_vector"), 2);
        }
        glUniform1f(glGetUniformLocation(shader, "u_blend_factor"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_jitter_x"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_jitter_y"), params[2]);
        glUniform1i(glGetUniformLocation(shader, "u_frame_index"), static_cast<int>(params[3]));
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[5]);
    } else if (effect_name == "deferred_lighting" && params.size() >= 8) {
        // params: [light_dir.xyz, light_color.xyz, intensity, ambient]
        const unsigned int normal_tex = request.FindTex(2);
        const unsigned int pos_tex = request.FindTex(3);
        if (normal_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, normal_tex);
            glUniform1i(glGetUniformLocation(shader, "u_gbuf_normal"), 1);
        }
        if (pos_tex != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, pos_tex);
            glUniform1i(glGetUniformLocation(shader, "u_gbuf_position"), 2);
        }
        glUniform3f(glGetUniformLocation(shader, "u_light_dir"), params[0], params[1], params[2]);
        glUniform3f(glGetUniformLocation(shader, "u_light_color"), params[3], params[4], params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_light_intensity"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_ambient"), params[7]);
    } else if (effect_name == "light_shaft" && params.size() >= 11) {
        const unsigned int depth_tex = request.FindTex(2);
        if (depth_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depth_tex);
            glUniform1i(glGetUniformLocation(shader, "u_depth_tex"), 1);
        }
        glUniform2f(glGetUniformLocation(shader, "u_sun_screen"), params[0], params[1]);
        glUniform3f(glGetUniformLocation(shader, "u_light_color"), params[2], params[3], params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_density"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_weight"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_decay"), params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_exposure"), params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_num_samples"), params[9]);
        glUniform1f(glGetUniformLocation(shader, "u_intensity"), params[10]);
    } else if (effect_name == "edge_detect" && params.size() >= 10) {
        glUniform1f(glGetUniformLocation(shader, "u_thickness"), params[0]);
        glUniform1f(glGetUniformLocation(shader, "u_depth_threshold"), params[1]);
        glUniform1f(glGetUniformLocation(shader, "u_normal_threshold"), params[2]);
        glUniform3f(glGetUniformLocation(shader, "u_outline_color"), params[3], params[4], params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_w"), params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_screen_h"), params[9]);
    } else if (effect_name == "volumetric_fog" && params.size() >= 29) {
        const unsigned int depth_tex = request.FindTex(2);
        if (depth_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depth_tex);
            glUniform1i(glGetUniformLocation(shader, "u_depth_tex"), 1);
        }
        glUniform3f(glGetUniformLocation(shader, "u_fog_color"),   params[0], params[1], params[2]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_density"),   params[3]);
        glUniform1f(glGetUniformLocation(shader, "u_height_falloff"),params[4]);
        glUniform1f(glGetUniformLocation(shader, "u_height_offset"), params[5]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_start"),     params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_end"),       params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_fog_steps"),     params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_sun_scatter"),   params[9]);
        glUniform3f(glGetUniformLocation(shader, "u_sun_dir"),    params[10], params[11], params[12]);
        glUniform3f(glGetUniformLocation(shader, "u_camera_pos"), params[13], params[14], params[15]);
        glUniform1f(glGetUniformLocation(shader, "u_near"),  params[16]);
        glUniform1f(glGetUniformLocation(shader, "u_far"),   params[17]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_right"), params[18], params[19], params[20]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_up"),    params[21], params[22], params[23]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_fwd"),   params[24], params[25], params[26]);
        glUniform1f(glGetUniformLocation(shader, "u_tan_fov_y"), params[27]);
        glUniform1f(glGetUniformLocation(shader, "u_aspect"),    params[28]);
    } else if (effect_name == "wboit_composite") {
        const unsigned int reveal_tex = request.FindTex(2);
        if (reveal_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, reveal_tex);
            glUniform1i(glGetUniformLocation(shader, "u_reveal_tex"), 1);
        }
    } else if (effect_name == "water" && params.size() >= 39) {
        const unsigned int depth_tex = request.FindTex(2);
        if (depth_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depth_tex);
            glUniform1i(glGetUniformLocation(shader, "u_depth_tex"), 1);
        }
        glUniform1f(glGetUniformLocation(shader, "u_water_level"), params[0]);
        glUniform3f(glGetUniformLocation(shader, "u_deep_color"), params[1], params[2], params[3]);
        glUniform3f(glGetUniformLocation(shader, "u_shallow_color"), params[4], params[5], params[6]);
        glUniform1f(glGetUniformLocation(shader, "u_max_depth"), params[7]);
        glUniform1f(glGetUniformLocation(shader, "u_transparency"), params[8]);
        glUniform1f(glGetUniformLocation(shader, "u_wave_amplitude"), params[9]);
        glUniform1f(glGetUniformLocation(shader, "u_wave_frequency"), params[10]);
        glUniform1f(glGetUniformLocation(shader, "u_wave_speed"), params[11]);
        glUniform2f(glGetUniformLocation(shader, "u_wave_dir"), params[12], params[13]);
        glUniform1f(glGetUniformLocation(shader, "u_refraction_strength"), params[14]);
        glUniform1f(glGetUniformLocation(shader, "u_specular_power"), params[15]);
        glUniform1f(glGetUniformLocation(shader, "u_reflection_strength"), params[16]);
        glUniform1f(glGetUniformLocation(shader, "u_time"), params[17]);
        glUniform3f(glGetUniformLocation(shader, "u_sun_dir"), params[18], params[19], params[20]);
        glUniform3f(glGetUniformLocation(shader, "u_camera_pos"), params[21], params[22], params[23]);
        glUniform1f(glGetUniformLocation(shader, "u_near"), params[24]);
        glUniform1f(glGetUniformLocation(shader, "u_far"), params[25]);
        glUniform3f(glGetUniformLocation(shader, "u_cam_fwd"), params[26], params[27], params[28]);
        glUniform1f(glGetUniformLocation(shader, "u_tan_fov_y"), params[29]);
        glUniform1f(glGetUniformLocation(shader, "u_aspect"), params[30]);
        glUniform1f(glGetUniformLocation(shader, "u_caustic_intensity"), params[31]);
        glUniform1f(glGetUniformLocation(shader, "u_caustic_scale"), params[32]);
        glUniform1f(glGetUniformLocation(shader, "u_foam_intensity"), params[33]);
        glUniform1f(glGetUniformLocation(shader, "u_foam_depth_threshold"), params[34]);
        glUniform1f(glGetUniformLocation(shader, "u_uw_fog_density"), params[35]);
        glUniform3f(glGetUniformLocation(shader, "u_uw_fog_color"), params[36], params[37], params[38]);
    } else if (effect_name == "decal" && params.size() >= 24) {
        const unsigned int depth_tex = request.FindTex(2);
        const unsigned int decal_tex = request.FindTex(3);
        if (depth_tex != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, depth_tex);
            glUniform1i(glGetUniformLocation(shader, "u_depth_tex"), 1);
        }
        if (decal_tex != 0) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, decal_tex);
            glUniform1i(glGetUniformLocation(shader, "u_decal_tex"), 2);
        }
        glUniformMatrix4fv(glGetUniformLocation(shader, "u_inv_model_vp"), 1, GL_FALSE, &params[0]);
        glUniform4f(glGetUniformLocation(shader, "u_color"), params[16], params[17], params[18], params[19]);
        glUniform1f(glGetUniformLocation(shader, "u_angle_fade"), params[20]);
        glUniform3f(glGetUniformLocation(shader, "u_decal_up"), params[21], params[22], params[23]);
    }

    glDisable(GL_DEPTH_TEST);
    if (effect_name == "ui_overlay" || effect_name == "decal" || effect_name == "wboit_composite" || effect_name == "water") {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    glBindVertexArray(pp_vao_handle_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
} // namespace render
} // namespace dse
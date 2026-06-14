/**
 * @file gl_shader_manager.cpp
 * @brief GLShaderManager 瀹炵幇 - 鐫€鑹插櫒绠＄悊鍣?
 */

#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/ubo_types.h"
#include "engine/base/debug.h"
#include "engine/render/rhi/opengl/gl_loader.h"
#include <cstdio>
#include <cstring>
#include <string>

// 鍐呭祵鐨?GLSL 330 鐫€鑹插櫒婧愮爜
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
#include "embed/volumetric_cloud_frag.gen.h"
#include "embed/decal_frag.gen.h"
#include "embed/water_frag.gen.h"
#include "embed/wboit_composite_frag.gen.h"
#include "embed/atmosphere_transmittance_lut_frag.gen.h"
#include "embed/atmosphere_sky_frag.gen.h"
#include "embed/sss_blur_frag.gen.h"
#include "embed/weather_particle_frag.gen.h"
#include "embed/eye_frag.gen.h"
#include "embed/lum_compute_frag.gen.h"
#include "embed/lum_adapt_frag.gen.h"
#include "embed/gbuffer_frag.gen.h"
#include "embed/sprite_vert.gen.h"
#include "embed/sprite_frag.gen.h"
#include "embed/sprite2d_vert.gen.h"
#include "embed/sprite2d_frag.gen.h"
#include "embed/shadow_vert.gen.h"
#include "embed/shadow_frag.gen.h"
#include "embed/text_sdf_frag.gen.h"

// GPU-Driven PBR 变体（预编译，无运行时 string patch）
#include "embed/pbr_gpu_driven_vert.gen.h"
#include "embed/pbr_gpu_driven_vert_reflect.gen.h"
#include "embed/pbr_gpu_driven_frag.gen.h"
#include "embed/pbr_gpu_driven_frag_reflect.gen.h"

// GPU-Driven Shadow 变体（预编译）
#include "embed/shadow_gpu_driven_vert.gen.h"
#include "embed/shadow_gpu_driven_vert_reflect.gen.h"

// Reflection metadata for automated binding
#include "embed/pbr_vert_reflect.gen.h"
#include "embed/pbr_frag_reflect.gen.h"
#include "embed/shadow_vert_reflect.gen.h"
#include "embed/shadow_frag_reflect.gen.h"
#include "embed/sprite_vert_reflect.gen.h"
#include "embed/sprite_frag_reflect.gen.h"
#include "embed/sprite2d_vert_reflect.gen.h"
#include "embed/gbuffer_frag_reflect.gen.h"
#include "engine/render/shader_reflection.h"

// ---------------------------------------------------------------------------
// GLSL variant selection by active GL profile (capability-driven; see gl_loader.h).
//   Desktop GL 4.x      -> *_glsl430
//   GLES 3.1 (Android)  -> *_essl310  (compute / SSBO available)
//   GLES 3.0 (WebGL2)   -> *_essl300  (no compute / SSBO)
// Token-paste over the embedded constant base name, e.g. DSE_SL(ksprite_vert).
// ---------------------------------------------------------------------------
#if DSE_GL_ES_RUNTIME
#  if defined(__EMSCRIPTEN__)
#    define DSE_SL(base) base##_essl300
#  else
#    define DSE_SL(base) base##_essl310
#  endif
#else
#  define DSE_SL(base) base##_glsl430
#endif

namespace dse {
namespace render {

// ============================================================
// 鐫€鑹插櫒缂栬瘧鍜岀鐞?
// ============================================================

unsigned int GLShaderManager::CompileProgram(const char* vertex_src, const char* fragment_src) {
    // A variant may be empty when the active GL profile cannot express it (e.g.
    // SSBO/compute shaders have no ESSL 300 / WebGL2 lowering). Skip cleanly.
    if (vertex_src == nullptr || vertex_src[0] == '\0' ||
        fragment_src == nullptr || fragment_src[0] == '\0') {
        return 0;
    }
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
// GL 3.3 UBO Fallback锛氫粠 SSBO GLSL 430 鐢熸垚 GLSL 330 UBO 鍙樹綋
//
// 鎵€鏈夋搷浣滃潎鎸?block/struct 鍚嶇О瀹氫綅锛屼笉渚濊禆 spirv-cross 鑷姩鍒嗛厤鐨?
// OpVariable ID锛堝 _1904銆乢1996锛夛紝鍥犳 shader 鏀瑰姩鍚?ID 婕傜Щ鏃朵粛绋冲畾銆?
// ============================================================

// 鎸夊悕绉扮Щ闄や竴涓?SSBO block 澹版槑锛圛D 鏃犲叧锛?
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
    auto end = src.find('\n', close + 2); // 璺宠繃 "} _NNN;"
    if (end == std::string::npos) end = src.size();
    else end++;                            // 鍖呭惈鎹㈣
    while (end < src.size() && src[end] == '\n') end++; // 娑堣€楀熬閮ㄧ┖琛?
    src.erase(layout_start, end - layout_start);
    return true;
}

// 灏?SSBO 鍧楀０鏄庡師鍦拌浆鎹负鍥哄畾澶у皬 UBO 鍧楋紙ID 鏃犲叧锛?
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

// Lower the skinning/instancing SSBOs of a PBR/shadow vertex shader to bounded
// UBOs so the program links on capability-limited contexts without SSBO support
// (WebGL2 / GLES 3.0, desktop GL < 4.3). ES 3.0 (WebGL2) rejects
// `buffer`/`std430`/`readonly` outright, so this is mandatory there.
//   - BoneMatricesSSBO -> BoneMatrices UBO (binding 6, bound explicitly later),
//     sized to the engine's BoneMatricesUBO (kMaxBones).
//   - SkinnedInstBuf (GPU instancing) / ComputeSkinBuf (compute skinning) are
//     unreachable without SSBO/compute (u_skinned is never 2/3 there); they are
//     bounded minimally and never read, so they stay at the default binding.
static std::string LowerVertexSSBOToUBO(const char* vert_src) {
    if (vert_src == nullptr || vert_src[0] == '\0') return std::string();
    std::string src = vert_src;
    TransformSSBOToUBO(src, "BoneMatricesSSBO", "BoneMatrices",   "u_bone_matrices",   kMaxBones);
    TransformSSBOToUBO(src, "SkinnedInstBuf",   "SkinnedInstUBO", "skinned_instances", 1);
    TransformSSBOToUBO(src, "ComputeSkinBuf",   "ComputeSkinUBO", "compute_skin_verts", 1);
    return src;
}

// 鍔ㄦ€佹彁鍙?UBO 鍧楃殑 spirv-cross 瀹炰緥鍚嶏紙濡?"_2008"锛夛紝闅?shader 鍙樺姩鑰屽彉鍔?
static std::string ExtractInstanceName(const std::string& src, const char* ubo_name) {
    const std::string marker = std::string("uniform ") + ubo_name + "\n";
    const auto blk = src.find(marker);
    if (blk == std::string::npos) return {};
    const auto close = src.find("\n} ", blk); // "\n} _NNN;"
    if (close == std::string::npos) return {};
    const auto name_start = close + 3;        // 璺宠繃 "\n} "
    const auto semi = src.find(';', name_start);
    if (semi == std::string::npos) return {};
    return src.substr(name_start, semi - name_start);
}

// 灏?Clustered Forward+ 鐐瑰厜婧愬惊鐜浛鎹负鏆村姏閬嶅巻锛圛D 鏃犲叧锛?
// 瀹氫綅渚濇嵁锛氱粨鏋勬爣璁?"int cl_tx" 鍜?"for (uint ci = 0u;"锛屼笉渚濊禆浠讳綍 _NNN ID
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
    const auto body = src.find("    {\n", for_kw); // for 寰幆浣撳紑濮?"    {\n"
    if (body == std::string::npos) {
        fprintf(stderr, "[GenerateUBOGLSL] point loop body { not found\n");
        return false;
    }
    // 寰幆浣撳唴绗竴涓?"        }\n" 鏄?if-guard 鐨勯棴鍚堟嫭鍙?
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

// 灏嗚仛鍏夌伅寰幆澶存浛鎹负鏆村姏閬嶅巻锛圛D 鏃犲叧锛?
// 瀹氫綅渚濇嵁锛氱粨鏋勬爣璁?"for (uint si = 0u;"锛屼笉渚濊禆浠讳綍 _NNN ID
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
    std::string src = DSE_SL(kpbr_frag);
    if (src.empty()) return src;  // no ES3.0 (WebGL2) PBR variant; caller skips

    // 1. 保持 #version 430 不降级。NVIDIA 在 GL 3.3 context 中也接受 #version 430
    //    （gbuffer shader 已验证）；降到 330 会导致 layout(binding) 编译失败。
    {
        const auto pos = src.find("#version 430");
        if (pos == std::string::npos)
            fprintf(stderr, "[GenerateUBOGLSL] #version 430 not found\n");
    }

    // 2. 绉婚櫎 ClusterInfoEntry 缁撴瀯浣擄紙鎸夊悕绉板畾浣嶏紝ID 鏃犲叧锛?
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

    // 3 & 4. 绉婚櫎 ClusterInfoSSBO + LightIndexSSBO锛堟寜鍚嶇О瀹氫綅锛孖D 鏃犲叧锛?
    RemoveSSBOBlock(src, "ClusterInfoSSBO");
    RemoveSSBOBlock(src, "LightIndexSSBO");

    // 5 & 6. PointLightSSBO + SpotLightSSBO 鈫?鍥哄畾澶у皬 UBO锛堟寜鍚嶇О瀹氫綅锛孖D 鏃犲叧锛?
    TransformSSBOToUBO(src, "PointLightSSBO", "PointLightUBO", "u_point_lights", kMaxUBOLights);
    TransformSSBOToUBO(src, "SpotLightSSBO",  "SpotLightUBO",  "u_spot_lights",  kMaxUBOLights);

    // 7. 鍔ㄦ€佹彁鍙栧疄渚嬪悕锛堥殢 shader 鍙樺姩鑷姩閫傚簲锛屼笉鍐嶇‖缂栫爜 _2008/_2190锛?
    const std::string point_inst = ExtractInstanceName(src, "PointLightUBO");
    const std::string spot_inst  = ExtractInstanceName(src, "SpotLightUBO");
    if (point_inst.empty())
        fprintf(stderr, "[GenerateUBOGLSL] PointLightUBO instance name not found\n");
    if (spot_inst.empty())
        fprintf(stderr, "[GenerateUBOGLSL] SpotLightUBO instance name not found\n");

    // 8 & 9. 鏇挎崲 Clustered Forward+ 寰幆涓烘毚鍔涢亶鍘嗭紙鎸夌粨鏋勬爣璁板畾浣嶏紝ID 鏃犲叧锛?
    ReplacePointLoopCluster(src, point_inst);
    ReplaceSpotLoopHeader(src, spot_inst);

    return src;
}

// ============================================================
// 鍐呯疆 PBR 鐫€鑹插櫒
// ============================================================

void GLShaderManager::InitBuiltinPBRShader() {
    using namespace dse::render::generated_shaders;
    // On capability-limited contexts without SSBO (WebGL2 / GLES 3.0, desktop
    // GL < 4.3) both the vertex and fragment shaders must be lowered from SSBO
    // to bounded UBO; the eye shader reuses the same (lowered) vertex.
    const std::string pbr_vert = supports_ssbo_
        ? std::string(DSE_SL(kpbr_vert))
        : LowerVertexSSBOToUBO(DSE_SL(kpbr_vert));
    const std::string pbr_frag = supports_ssbo_
        ? std::string(DSE_SL(kpbr_frag))
        : GenerateUBOGLSL();
    pbr_shader_handle_ = CompileProgram(pbr_vert.c_str(), pbr_frag.c_str());
    programs_created_ += 1;
    CachePBRLocations();

    eye_shader_handle_ = CompileProgram(pbr_vert.c_str(), DSE_SL(keye_frag));
    if (eye_shader_handle_) {
        programs_created_ += 1;
        DEBUG_LOG_INFO("[GL] Eye shader created: {}", eye_shader_handle_);
    }

    if (supports_ssbo_) {
        InitGPUDrivenPBRShader();
        InitGPUDrivenShadowShader();
    }
}

// UBO name 鈫?engine UBOBindingPoint 鏄犲皠琛紙鐢ㄤ簬 reflection 鑷姩缁戝畾锛?
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
    return static_cast<UBOBindingPoint>(0xFF); // unknown 鈥?skipped
}

// 浠?reflection 鏁版嵁鑷姩缁戝畾鍗曚釜 stage 鐨勬墍鏈?UBO block
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

    // --- UBO block 缁戝畾锛坮eflection 椹卞姩锛?--
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(h, kpbr_vert_reflection);
    BindUBOsFromReflection(h, kpbr_frag_reflection);

    // 缂撳瓨 block index 鍒?locations struct锛堝悜鍚庡吋瀹癸級
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

    // The PointLightUBO/SpotLightUBO/BoneMatrices blocks are lowered from SSBOs
    // at runtime, so the reflection metadata (which still lists them as storage
    // buffers) cannot bind them. Without explicit binding they default to 0 and
    // alias PerFrame -> garbage light counts / a block larger than the bound
    // buffer. Bind them to their engine UBO binding points here.
    if (!supports_ssbo_) {
        const auto bind_block = [h](unsigned int idx, UBOBindingPoint bp) {
            if (idx != GL_INVALID_INDEX)
                glUniformBlockBinding(h, idx, static_cast<unsigned int>(bp));
        };
        bind_block(loc.point_lights_block_index, UBOBindingPoint::PointLights);
        bind_block(loc.spot_lights_block_index,  UBOBindingPoint::SpotLights);
        bind_block(loc.bone_matrices_block_index, UBOBindingPoint::BoneMatrices);
    }

    // --- 绾圭悊 unit 鑷姩鍒嗛厤锛坮eflection 椹卞姩锛屼竴娆℃€х粦瀹氾級---
    {
        std::vector<gl_reflect::TextureUnitEntry> tex_entries;
        gl_reflect::ComputeFlatTextureUnits(kpbr_frag_reflection, tex_entries);

        // glUseProgram 蹇呴』鍦?BindSamplersOnce 涔嬪墠璋冪敤
        glUseProgram(h);
        gl_reflect::BindSamplersOnce(h, tex_entries, glGetUniformLocation, glUniform1i);

        // 浠庤绠楃粨鏋滃～鍏?PBRTextureSlots锛坉raw executor 浣跨敤锛?
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
        // Debug 鏍￠獙锛氱汗鐞?slot 鏃犻噸鍙?
        shader_reflect_debug::ValidateTextureSlotOverlaps(tex_entries);
        shader_reflect_debug::ValidateUBOBindings(kpbr_frag_reflection, "PBR.frag");
        shader_reflect_debug::ValidateVertexInputs(kpbr_vert_reflection);
#endif
    }

    // --- 缂撳瓨 sampler location锛堝悜鍚庡吋瀹?draw executor锛?--
    loc.texture = glGetUniformLocation(h, "u_texture");
    loc.normal_map = glGetUniformLocation(h, "u_normal_map");
    loc.metallic_roughness_map = glGetUniformLocation(h, "u_metallic_roughness_map");
    loc.emissive_map = glGetUniformLocation(h, "u_emissive_map");
    loc.occlusion_map = glGetUniformLocation(h, "u_occlusion_map");
    loc.shadow_atlas = glGetUniformLocation(h, "u_shadow_atlas");
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

    // --- Snow cover ---
    loc.snow_coverage = glGetUniformLocation(h, "u_snow_coverage");
    loc.snow_normal_threshold = glGetUniformLocation(h, "u_snow_normal_threshold");
    loc.snow_edge_sharpness = glGetUniformLocation(h, "u_snow_edge_sharpness");
    loc.snow_params = glGetUniformLocation(h, "u_snow_params");

    // --- DDGI ---
    loc.ddgi_enabled = glGetUniformLocation(h, "u_ddgi_enabled");
    loc.ddgi_grid_origin = glGetUniformLocation(h, "u_ddgi_grid_origin");
    loc.ddgi_grid_spacing = glGetUniformLocation(h, "u_ddgi_grid_spacing");
    loc.ddgi_grid_resolution = glGetUniformLocation(h, "u_ddgi_grid_resolution");
    loc.ddgi_irradiance_texels = glGetUniformLocation(h, "u_ddgi_irradiance_texels");
    loc.ddgi_gi_intensity = glGetUniformLocation(h, "u_ddgi_gi_intensity");
    loc.ddgi_normal_bias = glGetUniformLocation(h, "u_ddgi_normal_bias");
    loc.ddgi_irradiance_atlas = glGetUniformLocation(h, "u_ddgi_irradiance_atlas");

    // --- 閫愬璞?uniform锛堜粠 push constants 灞曞钩锛?----
    loc.model = glGetUniformLocation(h, "u_model");
    loc.skinned = glGetUniformLocation(h, "u_skinned");
    loc.morph_enabled = glGetUniformLocation(h, "u_morph_enabled");
    loc.bone_offset = glGetUniformLocation(h, "u_bone_offset");
    loc.foliage = glGetUniformLocation(h, "u_foliage");
    loc.use_instancing = glGetUniformLocation(h, "u_use_instancing");
}

// NOTE: Old hand-written PBR shader (~500 lines) removed.
// Now uses offline-compiled GLSL 330 strings (embed/*.gen.h).
// See git history for the old version.

// ============================================================
// 澶╃┖鐩掔潃鑹插櫒
// ============================================================

void GLShaderManager::InitSkyboxShader() {
    if (skybox_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    skybox_shader_handle_ = CompileProgram(DSE_SL(kskybox_vert), DSE_SL(kskybox_frag));
    programs_created_ += 1;

    skybox_locations_.vp = glGetUniformLocation(skybox_shader_handle_, "u_vp");
    skybox_locations_.tex = glGetUniformLocation(skybox_shader_handle_, "skybox");
}

// ============================================================
// 绮掑瓙鐫€鑹插櫒
// ============================================================

void GLShaderManager::InitParticleShader() {
    if (particle_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    particle_shader_handle_ = CompileProgram(DSE_SL(kparticle_vert), DSE_SL(kparticle_frag));
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
// GBuffer 鐫€鑹插櫒锛堝欢杩熸覆鏌撳嚑浣曢€氶亾锛?
// ============================================================

void GLShaderManager::InitGBufferShader() {
    if (gbuffer_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;

    gbuffer_shader_handle_ = CompileProgram(DSE_SL(kpbr_vert), DSE_SL(kgbuffer_frag));
    programs_created_ += 1;

    // GBuffer UBO 缁戝畾锛坮eflection 椹卞嫊锛?
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(gbuffer_shader_handle_, kpbr_vert_reflection);
    BindUBOsFromReflection(gbuffer_shader_handle_, kgbuffer_frag_reflection);

    // GBuffer 绾圭悊 sampler 涓€娆℃€х粦瀹?
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
// 绮剧伒鐫€鑹插櫒
// ============================================================

void GLShaderManager::InitSpriteShader() {
    if (sprite_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    sprite_shader_handle_ = CompileProgram(DSE_SL(ksprite_vert), DSE_SL(ksprite_frag));
    programs_created_ += 1;

    // Sprite UBO 缁戝畾锛坮eflection 椹卞姩锛?
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(sprite_shader_handle_, ksprite_vert_reflection);
    BindUBOsFromReflection(sprite_shader_handle_, ksprite_frag_reflection);
}

// ============================================================
// 2D batch shader (ES3.0-capable; used on Web in place of PBR)
// ============================================================

void GLShaderManager::InitSprite2DShader() {
    if (sprite2d_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    sprite2d_shader_handle_ = CompileProgram(DSE_SL(ksprite2d_vert), DSE_SL(ksprite2d_frag));
    if (sprite2d_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("GLShaderManager: 2D batch shader compile failed");
        return;
    }
    programs_created_ += 1;
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(sprite2d_shader_handle_, ksprite2d_vert_reflection);
    glUseProgram(sprite2d_shader_handle_);
    sprite2d_locations_.texture = glGetUniformLocation(sprite2d_shader_handle_, "u_texture");
    glUseProgram(0);
}

// ============================================================
// SDF 文本着色器
// ============================================================

void GLShaderManager::InitTextSdfShader() {
    if (text_sdf_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    text_sdf_shader_handle_ = CompileProgram(DSE_SL(ksprite_vert), DSE_SL(ktext_sdf_frag));
    if (text_sdf_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("GLShaderManager: SDF text shader compile failed");
        return;
    }
    programs_created_ += 1;

    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(text_sdf_shader_handle_, ksprite_vert_reflection);

    text_sdf_locations_.texture = glGetUniformLocation(text_sdf_shader_handle_, "u_texture");
    text_sdf_locations_.sdf_threshold = glGetUniformLocation(text_sdf_shader_handle_, "u_sdf_threshold");
    text_sdf_locations_.sdf_smoothing = glGetUniformLocation(text_sdf_shader_handle_, "u_sdf_smoothing");
    text_sdf_locations_.outline_width = glGetUniformLocation(text_sdf_shader_handle_, "u_outline_width");
    text_sdf_locations_.shadow_softness = glGetUniformLocation(text_sdf_shader_handle_, "u_shadow_softness");
}

// ============================================================
// UI 视觉效果着色器
// ============================================================

void GLShaderManager::InitUIEffectsShader() {
    if (ui_effects_shader_handle_ != 0) return;
    unsigned int prog = GetOrCreateGenPPShader("ui_effects");
    if (prog == 0) {
        using namespace dse::render::generated_shaders;
        const char* vert = DSE_SL(ksprite_vert);
        const char* frag = nullptr;
        (void)vert;
        (void)frag;
        DEBUG_LOG_WARN("GLShaderManager: ui_effects gen.h shader not available — visual effects disabled");
        return;
    }
    ui_effects_shader_handle_ = prog;
    ui_effects_locations_.texture = glGetUniformLocation(prog, "u_texture");
    ui_effects_locations_.gradient_start = glGetUniformLocation(prog, "u_gradient_start");
    ui_effects_locations_.gradient_end = glGetUniformLocation(prog, "u_gradient_end");
    ui_effects_locations_.rect_size_and_radius = glGetUniformLocation(prog, "u_rect_size_and_radius");
    ui_effects_locations_.blur_params = glGetUniformLocation(prog, "u_blur_params");
}

// ============================================================
// 闃村奖娣卞害鐫€鑹插櫒
// ============================================================

void GLShaderManager::InitShadowShader() {
    if (shadow_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    // Same SSBO->UBO lowering as the PBR vertex shader for SSBO-less contexts.
    const std::string shadow_vert = supports_ssbo_
        ? std::string(DSE_SL(kshadow_vert))
        : LowerVertexSSBOToUBO(DSE_SL(kshadow_vert));
    shadow_shader_handle_ = CompileProgram(shadow_vert.c_str(), DSE_SL(kshadow_frag));
    programs_created_ += 1;

    // Shadow UBO 缁戝畾锛坮eflection 椹卞姩锛?
    using namespace dse::render::generated_shaders::reflect;
    BindUBOsFromReflection(shadow_shader_handle_, kshadow_vert_reflection);
    BindUBOsFromReflection(shadow_shader_handle_, kshadow_frag_reflection);
    // BoneMatrices is lowered from an SSBO, so reflection cannot bind it.
    if (!supports_ssbo_) {
        unsigned int bm = glGetUniformBlockIndex(shadow_shader_handle_, "BoneMatrices");
        if (bm != GL_INVALID_INDEX)
            glUniformBlockBinding(shadow_shader_handle_, bm,
                                  static_cast<unsigned int>(UBOBindingPoint::BoneMatrices));
    }

    shadow_locations_.model        = glGetUniformLocation(shadow_shader_handle_, "u_model");
    shadow_locations_.skinned      = glGetUniformLocation(shadow_shader_handle_, "u_skinned");
    shadow_locations_.morph_enabled = glGetUniformLocation(shadow_shader_handle_, "u_morph_enabled");
    shadow_locations_.bone_offset   = glGetUniformLocation(shadow_shader_handle_, "u_bone_offset");
    shadow_locations_.foliage         = glGetUniformLocation(shadow_shader_handle_, "u_foliage");
}

// ============================================================
// Post-process shader cache
// ============================================================

unsigned int GLShaderManager::GetOrCreateGenPPShader(const std::string& effect_name) {
    std::string key = "gen_" + effect_name;
    auto it = pp_shaders_.find(key);
    if (it != pp_shaders_.end()) return it->second;

    using namespace dse::render::generated_shaders;
    const char* fs = nullptr;
    if      (effect_name == "fxaa")             fs = DSE_SL(kfxaa_frag);
    else if (effect_name == "bloom_extract")     fs = DSE_SL(kbloom_extract_frag);
    else if (effect_name == "bloom_downsample")  fs = DSE_SL(kbloom_downsample_frag);
    else if (effect_name == "bloom_upsample")    fs = DSE_SL(kbloom_upsample_frag);
    else if (effect_name == "tonemapping")         fs = DSE_SL(ktonemapping_frag);
    else if (effect_name == "color_grading")       fs = DSE_SL(kcolor_grading_frag);
    else if (effect_name == "edge_detect")          fs = DSE_SL(kedge_detect_frag);
    else if (effect_name == "postprocess_passthrough") fs = DSE_SL(kpostprocess_passthrough_frag);
    else if (effect_name == "bloom_composite")     fs = DSE_SL(kbloom_composite_ssao_ae_frag);
    else if (effect_name == "ssao_apply")           fs = DSE_SL(kssao_apply_frag);
    else if (effect_name == "ssao")                 fs = DSE_SL(kssao_frag);
    else if (effect_name == "ssao_blur")             fs = DSE_SL(kssao_blur_frag);
    else if (effect_name == "contact_shadow")        fs = DSE_SL(kcontact_shadow_frag);
    else if (effect_name == "dof")                   fs = DSE_SL(kdof_frag);
    else if (effect_name == "motion_vector")         fs = DSE_SL(kmotion_vector_frag);
    else if (effect_name == "motion_blur")           fs = DSE_SL(kmotion_blur_frag);
    else if (effect_name == "ssr")                   fs = DSE_SL(kssr_frag);
    else if (effect_name == "taa_resolve")           fs = DSE_SL(ktaa_resolve_frag);
    else if (effect_name == "ui_overlay")             fs = DSE_SL(kpostprocess_passthrough_frag);
    else if (effect_name == "deferred_lighting")     fs = DSE_SL(kdeferred_lighting_frag);
    else if (effect_name == "light_shaft")           fs = DSE_SL(klight_shaft_frag);
    else if (effect_name == "volumetric_fog")        fs = DSE_SL(kvolumetric_fog_frag);
    else if (effect_name == "volumetric_cloud")     fs = DSE_SL(kvolumetric_cloud_frag);
    else if (effect_name == "decal")                 fs = DSE_SL(kdecal_frag);
    else if (effect_name == "water")                 fs = DSE_SL(kwater_frag);
    else if (effect_name == "wboit_composite")       fs = DSE_SL(kwboit_composite_frag);
    else if (effect_name == "lum_compute")           fs = DSE_SL(klum_compute_frag);
    else if (effect_name == "lum_adapt")             fs = DSE_SL(klum_adapt_frag);
    else if (effect_name == "bloom_blur_h")          fs = DSE_SL(kbloom_blur_h_frag);
    else if (effect_name == "bloom_blur_v")          fs = DSE_SL(kbloom_blur_v_frag);
    else if (effect_name == "copy")                  fs = DSE_SL(kpostprocess_passthrough_frag);
    else if (effect_name == "atmosphere_transmittance_lut") fs = DSE_SL(katmosphere_transmittance_lut_frag);
    else if (effect_name == "atmosphere_sky")        fs = DSE_SL(katmosphere_sky_frag);
    else if (effect_name == "sss_blur")               fs = DSE_SL(ksss_blur_frag);
    else if (effect_name == "weather_particle")        fs = DSE_SL(kweather_particle_frag);
    else return 0;

    if (fs == nullptr || fs[0] == '\0') return 0;  // no variant for this GL profile
    unsigned int shader = CompileProgram(DSE_SL(kpostprocess_vert), fs);
    if (shader == 0) {
        DEBUG_LOG_ERROR("Failed to compile gen.h PP shader: {}", effect_name);
        return 0;
    }
    programs_created_ += 1;
    pp_shaders_[key] = shader;
    return shader;
}

// ============================================================
// Shader Warmup — 预编译所有后处理着色器消除首帧 stall
// ============================================================

void GLShaderManager::WarmupAllPostProcessShaders() {
    static const char* kAllPPEffects[] = {
        "fxaa", "bloom_extract", "bloom_downsample", "bloom_upsample",
        "tonemapping", "color_grading", "edge_detect", "postprocess_passthrough",
        "bloom_composite", "ssao_apply", "ssao", "ssao_blur", "contact_shadow",
        "dof", "motion_vector", "motion_blur", "ssr", "taa_resolve",
        "ui_overlay", "deferred_lighting", "light_shaft",
        "volumetric_fog", "volumetric_cloud",
        "decal", "water", "wboit_composite", "lum_compute", "lum_adapt",
        "bloom_blur_h", "bloom_blur_v", "copy",
        "atmosphere_transmittance_lut", "atmosphere_sky",
        "sss_blur",
        "weather_particle",
    };
    for (const char* name : kAllPPEffects) {
        GetOrCreateGenPPShader(name);
    }
    DEBUG_LOG_INFO("[GLShaderManager] Warmed up {} post-process shaders", programs_created_);
}

// ============================================================
// GPU-Driven PBR Shader Variant
// ============================================================

void GLShaderManager::InitGPUDrivenPBRShader() {
    using namespace dse::render::generated_shaders;
    using namespace dse::render::generated_shaders::reflect;

    // SPIRV-Cross 为 DSEInstBuf 分配了 binding=0，但 CPU 侧绑定到 kSSBOBindingInstances(=5)
    // 编译前 patch 源码将 instance SSBO binding 修正为 5
    std::string vert_patched = DSE_SL(kpbr_gpu_driven_vert);
    {
        const std::string old_binding = "layout(binding = 0, std430) readonly buffer DSEInstBuf";
        const std::string new_binding = "layout(binding = 5, std430) readonly buffer DSEInstBuf";
        auto pos = vert_patched.find(old_binding);
        if (pos != std::string::npos) {
            vert_patched.replace(pos, old_binding.size(), new_binding);
        } else {
            fprintf(stderr, "[GLShaderManager] GPU-driven PBR: DSEInstBuf binding=0 not found for patch\n");
        }
    }
    const char* frag_src = DSE_SL(kpbr_gpu_driven_frag);

    gpu_driven_pbr_shader_handle_ = CompileProgram(vert_patched.c_str(), frag_src);
    if (gpu_driven_pbr_shader_handle_ == 0) {
        fprintf(stderr, "[GLShaderManager] GPU-driven PBR shader compilation failed\n");
        return;
    }
    programs_created_ += 1;

    // UBO block 绑定（使用 GPU_DRIVEN 变体的 reflection 数据）
    BindUBOsFromReflection(gpu_driven_pbr_shader_handle_, kpbr_gpu_driven_vert_reflection);
    BindUBOsFromReflection(gpu_driven_pbr_shader_handle_, kpbr_gpu_driven_frag_reflection);

    // Sampler 一次性绑定
    {
        using namespace dse::render::gl_reflect;
        std::vector<TextureUnitEntry> tex_entries;
        ComputeFlatTextureUnits(kpbr_gpu_driven_frag_reflection, tex_entries);
        glUseProgram(gpu_driven_pbr_shader_handle_);
        BindSamplersOnce(gpu_driven_pbr_shader_handle_, tex_entries, glGetUniformLocation, glUniform1i);
        glUseProgram(0);
    }

    // GPU_DRIVEN 变体无 u_skinned / u_morph_enabled uniform；glGetUniformLocation 返回 -1
    // SetupGPUDrivenPBRShader 中的 glUniform1i(-1, x) 是 OpenGL 规范的 no-op
    gpu_driven_pbr_skinned_loc_ = glGetUniformLocation(gpu_driven_pbr_shader_handle_, "u_skinned");
    gpu_driven_pbr_morph_loc_   = glGetUniformLocation(gpu_driven_pbr_shader_handle_, "u_morph_enabled");
}

// ============================================================
// GPU-Driven Shadow Shader Variant
// ============================================================

void GLShaderManager::InitGPUDrivenShadowShader() {
    if (gpu_driven_shadow_shader_handle_ != 0) return;
    using namespace dse::render::generated_shaders;
    using namespace dse::render::generated_shaders::reflect;

    // 使用预编译的 GPU-driven shadow 变体，仅需 patch SSBO binding 0→5
    std::string vert_src = DSE_SL(kshadow_gpu_driven_vert);
    {
        const std::string old_binding = "layout(binding = 0, std430) readonly buffer DSEInstBuf";
        const std::string new_binding = "layout(binding = 5, std430) readonly buffer DSEInstBuf";
        auto pos = vert_src.find(old_binding);
        if (pos != std::string::npos) {
            vert_src.replace(pos, old_binding.size(), new_binding);
        } else {
            fprintf(stderr, "[GLShaderManager] GPU-driven Shadow: DSEInstBuf binding=0 not found for patch\n");
        }
    }

    gpu_driven_shadow_shader_handle_ = CompileProgram(vert_src.c_str(), DSE_SL(kshadow_frag));
    if (gpu_driven_shadow_shader_handle_ == 0) {
        fprintf(stderr, "[GLShaderManager] GPU-driven Shadow shader compilation failed\n");
        return;
    }
    programs_created_ += 1;

    BindUBOsFromReflection(gpu_driven_shadow_shader_handle_, kshadow_gpu_driven_vert_reflection);
    BindUBOsFromReflection(gpu_driven_shadow_shader_handle_, kshadow_frag_reflection);

    gpu_driven_shadow_skinned_loc_ = glGetUniformLocation(gpu_driven_shadow_shader_handle_, "u_skinned");
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
    if (gpu_driven_pbr_shader_handle_ != 0) {
        glDeleteProgram(gpu_driven_pbr_shader_handle_);
        programs_destroyed_ += 1;
        gpu_driven_pbr_shader_handle_ = 0;
    }
    if (gpu_driven_shadow_shader_handle_ != 0) {
        glDeleteProgram(gpu_driven_shadow_shader_handle_);
        programs_destroyed_ += 1;
        gpu_driven_shadow_shader_handle_ = 0;
    }
    if (gbuffer_shader_handle_ != 0) {
        glDeleteProgram(gbuffer_shader_handle_);
        programs_destroyed_ += 1;
        gbuffer_shader_handle_ = 0;
    }
    if (sprite_shader_handle_ != 0) {
        glDeleteProgram(sprite_shader_handle_);
        programs_destroyed_ += 1;
        sprite_shader_handle_ = 0;
    }
    if (shadow_shader_handle_ != 0) {
        glDeleteProgram(shadow_shader_handle_);
        programs_destroyed_ += 1;
        shadow_shader_handle_ = 0;
    }
    if (eye_shader_handle_ != 0) {
        glDeleteProgram(eye_shader_handle_);
        programs_destroyed_ += 1;
        eye_shader_handle_ = 0;
    }
    if (text_sdf_shader_handle_ != 0) {
        glDeleteProgram(text_sdf_shader_handle_);
        programs_destroyed_ += 1;
        text_sdf_shader_handle_ = 0;
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

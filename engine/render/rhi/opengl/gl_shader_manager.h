/**
 * @file gl_shader_manager.h
 * @brief OpenGL 着色器管理器 - 负责着色器编译/链接和 uniform location 缓存
 *
 * 从 OpenGLRhiDevice 中提取的第三个子系统：
 * - 内置 PBR 着色器的编译与 uniform 缓存
 * - 天空盒/粒子着色器管理
 * - 后处理着色器缓存
 * - 通用着色器编译辅助函数
 */

#ifndef DSE_RENDER_GL_SHADER_MANAGER_H
#define DSE_RENDER_GL_SHADER_MANAGER_H

#include <unordered_map>
#include <string>
#include <vector>
#include "engine/render/rhi/shader_manager_base.h"

namespace dse {
namespace render {

/// PBR 主着色器的 uniform location 缓存
/// UBO 化后：PerFrame/PerScene/PerMaterial 中的 uniform 不再需要独立 location
struct PBRShaderLocations {
    // --- UBO block indices（用于 glUniformBlockBinding） ---
    unsigned int per_frame_block_index = 0;
    unsigned int per_scene_block_index = 0;
    unsigned int per_material_block_index = 0;
    unsigned int point_lights_block_index = 0;
    unsigned int spot_lights_block_index = 0;
    unsigned int spot_light_data_block_index = 0;
    unsigned int bone_matrices_block_index = 0;
    unsigned int morph_weights_block_index = 0;
    unsigned int light_probe_data_block_index = 0;

    // --- 纹理采样器 uniform location ---
    int texture = -1;
    int normal_map = -1;
    int metallic_roughness_map = -1;
    int emissive_map = -1;
    int occlusion_map = -1;
    int shadow_map[3] = {-1, -1, -1};
    int spot_shadow_map[4] = {-1, -1, -1, -1};
    int point_shadow_map[4] = {-1, -1, -1, -1};

    // --- Terrain splatmap ---
    int splat_weight_map = -1;
    int splat_layer[4] = {-1, -1, -1, -1};
    int splat_enabled = -1;
    int splat_tiling = -1;

    // --- Snow cover ---
    int snow_coverage = -1;
    int snow_normal_threshold = -1;
    int snow_edge_sharpness = -1;
    int snow_params = -1;

    // --- DDGI uniform location ---
    int ddgi_enabled = -1;
    int ddgi_grid_origin = -1;
    int ddgi_grid_spacing = -1;
    int ddgi_grid_resolution = -1;
    int ddgi_irradiance_texels = -1;
    int ddgi_gi_intensity = -1;
    int ddgi_normal_bias = -1;
    int ddgi_irradiance_atlas = -1;

    // --- 逐对象 uniform（从 push constants 展平） ---
    int model = -1;
    int skinned = -1;
    int morph_enabled = -1;
    int bone_offset = -1;
    int foliage = -1;
    int use_instancing = -1;
};

/// PBR 纹理 slot 映射（reflection 驱动，初始化时一次性计算）
/// 每个字段 = glActiveTexture(GL_TEXTURE0 + slot) 使用的 texture unit
struct PBRTextureSlots {
    int albedo = 0;                ///< u_texture
    int normal = 1;                ///< u_normal_map
    int metallic_roughness = 2;    ///< u_metallic_roughness_map
    int emissive = 3;              ///< u_emissive_map
    int occlusion = 4;             ///< u_occlusion_map
    int shadow_base = 5;           ///< u_shadow_maps[0..2]
    int spot_shadow_base = 8;      ///< u_spot_shadow_maps[0..3]
    int reflection_cubemap = 12;   ///< u_reflection_cubemap
    int brdf_lut = 13;             ///< u_brdf_lut
    int splat_weight = 14;         ///< u_splat_weight_map
    int splat_layer_base = 15;     ///< u_splat_layer0..3
    int point_shadow_base = 19;    ///< u_point_shadow_maps[0..3]
    int ddgi_atlas = 23;           ///< u_ddgi_irradiance_atlas（预留）
};

/// 天空盒着色器 uniform location 缓存
struct SkyboxShaderLocations {
    int vp = -1;     ///< 合并的 view-projection 矩阵
    int tex = -1;
};

/// Shadow 着色器 uniform location 缓存
struct ShadowShaderLocations {
    int model = -1;
    int skinned = -1;
    int morph_enabled = -1;
    int bone_offset = -1;
    int foliage = -1;
};

/// 粒子着色器 uniform location 缓存
struct ParticleShaderLocations {
    unsigned int per_frame_block_index = 0;  ///< PerFrame UBO
    int texture = -1;
};

/**
 * @class GLShaderManager
 * @brief OpenGL 着色器管理器
 *
 * 职责：
 * 1. 通用着色器编译/链接
 * 2. 内置 PBR 着色器的初始化和 uniform location 缓存
 * 3. 天空盒/粒子着色器管理
 * 4. 后处理着色器效果缓存
 */
class GLShaderManager : public ShaderManagerBase {
public:
    GLShaderManager() { next_handle_ = 100000; }
    ~GLShaderManager() = default;

    /// 编译并链接着色器程序（通用接口）
    static unsigned int CompileProgram(const char* vertex_src, const char* fragment_src);

    /// 销毁着色器程序
    void DeleteProgram(unsigned int handle);

    /// 设置 SSBO 支持标志（必须在 InitBuiltinPBRShader 前调用）
    void set_supports_ssbo(bool v) { supports_ssbo_ = v; }
    bool supports_ssbo() const { return supports_ssbo_; }

    /// 初始化内置 PBR 着色器，编译并缓存所有 uniform location
    void InitBuiltinPBRShader();

    /// 清理所有着色器资源
    void Shutdown();

    // --- PBR 主着色器 ---
    const PBRShaderLocations& pbr_locations() const { return pbr_locations_; }
    const PBRTextureSlots& pbr_texture_slots() const { return pbr_texture_slots_; }

    /// GPU-Driven PBR uniform locations (GL only)
    int gpu_driven_pbr_skinned_loc() const { return gpu_driven_pbr_skinned_loc_; }
    int gpu_driven_pbr_morph_loc()    const { return gpu_driven_pbr_morph_loc_; }

    /// GPU-Driven Shadow uniform locations (GL only)
    int gpu_driven_shadow_skinned_loc() const { return gpu_driven_shadow_skinned_loc_; }

    /// Per-item Shadow shader locations
    const ShadowShaderLocations& shadow_locations() const { return shadow_locations_; }

    // --- 天空盒着色器 ---
    const SkyboxShaderLocations& skybox_locations() const { return skybox_locations_; }
    void InitSkyboxShader();
    void set_skybox_shader_handle(unsigned int h) { skybox_shader_handle_ = h; }

    // --- 粒子着色器 ---
    const ParticleShaderLocations& particle_locations() const { return particle_locations_; }
    void InitParticleShader();
    void set_particle_shader_handle(unsigned int h) { particle_shader_handle_ = h; }

    // --- 精灵着色器 ---
    void InitSpriteShader();

    // --- SDF 文本着色器 ---
    struct TextSdfLocations {
        int texture = -1;
        int sdf_threshold = -1;
        int sdf_smoothing = -1;
        int outline_width = -1;
        int shadow_softness = -1;
    };
    void InitTextSdfShader();
    const TextSdfLocations& text_sdf_locations() const { return text_sdf_locations_; }

    // --- UI 视觉效果着色器 ---
    struct UIEffectsLocations {
        int texture = -1;
        int gradient_start = -1;
        int gradient_end = -1;
        int rect_size_and_radius = -1;
        int blur_params = -1;
    };
    void InitUIEffectsShader();
    unsigned int ui_effects_shader_handle() const { return ui_effects_shader_handle_; }
    const UIEffectsLocations& ui_effects_locations() const { return ui_effects_locations_; }

    // --- 阴影深度着色器 ---
    void InitShadowShader();

    // --- GBuffer 着色器（延迟渲染几何通道） ---
    void InitGBufferShader();

    // --- 后处理着色器缓存 ---
    /// gen.h 统一后处理着色器：使用 GLSL 430 预编译源（VS + FS 均来自 gen.h）
    unsigned int GetOrCreateGenPPShader(const std::string& effect_name);

    /// 预编译所有后处理着色器，消除首帧 stall
    void WarmupAllPostProcessShaders();

    // 着色器句柄访问器、计数器继承自 ShaderManagerBase

private:
    /// 缓存 PBR 着色器的所有 uniform location
    void CachePBRLocations();

    /// 编译 GPU-Driven PBR 着色器变体（运行时字符串补丁）
    void InitGPUDrivenPBRShader();

    /// 编译 GPU-Driven Shadow 着色器变体（运行时字符串补丁）
    void InitGPUDrivenShadowShader();

    PBRShaderLocations pbr_locations_;
    PBRTextureSlots pbr_texture_slots_;
    SkyboxShaderLocations skybox_locations_;
    ParticleShaderLocations particle_locations_;

    /// 后处理着色器缓存：effect_name → shader_program_handle
    std::unordered_map<std::string, unsigned int> pp_shaders_;

    /// GL 3.3 UBO fallback 模式：从 SSBO GLSL 430 运行时高效生成 GLSL 330 UBO 变体
    static std::string GenerateUBOGLSL();

    bool supports_ssbo_ = true;

    int gpu_driven_pbr_skinned_loc_ = -1;
    int gpu_driven_pbr_morph_loc_   = -1;
    int gpu_driven_shadow_skinned_loc_ = -1;
    ShadowShaderLocations shadow_locations_;
    TextSdfLocations text_sdf_locations_;
    unsigned int ui_effects_shader_handle_ = 0;
    UIEffectsLocations ui_effects_locations_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_SHADER_MANAGER_H

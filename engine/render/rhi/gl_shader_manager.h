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

namespace dse {
namespace render {

/// PBR 主着色器的 uniform location 缓存
/// UBO 化后：PerFrame/PerScene/PerMaterial 中的 uniform 不再需要独立 location
struct PBRShaderLocations {
    // --- UBO block indices（用于 glUniformBlockBinding） ---
    unsigned int per_frame_block_index = 0;     ///< PerFrame UBO block index
    unsigned int per_scene_block_index = 0;     ///< PerScene UBO block index
    unsigned int per_material_block_index = 0;  ///< PerMaterial UBO block index

    // --- 独立 uniform location（纹理采样器/逐对象数据） ---
    int texture = -1;
    int normal_map = -1;
    int metallic_roughness_map = -1;
    int emissive_map = -1;
    int occlusion_map = -1;
    int shadow_map[3] = {-1, -1, -1};
    int spot_shadow_map[4] = {-1, -1, -1, -1};
    int spot_light_space_matrix[4] = {-1, -1, -1, -1};
    int model = -1;                    ///< 模型矩阵（逐对象）
    int skinned = -1;
    int bone_matrices = -1;
    int morph_enabled = -1;
    int morph_weights = -1;
    int point_light_count = -1;

    struct PointLightLoc {
        int color = -1;
        int position = -1;
        int intensity = -1;
        int radius = -1;
        int cast_shadow = -1;
        int shadow_index = -1;
    } point_lights[4];

    int spot_light_count = -1;

    struct SpotLightLoc {
        int color = -1;
        int position = -1;
        int direction = -1;
        int intensity = -1;
        int radius = -1;
        int inner_cone = -1;
        int outer_cone = -1;
        int cast_shadow = -1;
        int shadow_index = -1;
    } spot_lights[4];
};

/// 天空盒着色器 uniform location 缓存
struct SkyboxShaderLocations {
    int view = -1;
    int projection = -1;
    int tex = -1;
};

/// 粒子着色器 uniform location 缓存
struct ParticleShaderLocations {
    int vp = -1;
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
class GLShaderManager {
public:
    GLShaderManager() = default;
    ~GLShaderManager() = default;

    /// 编译并链接着色器程序（通用接口）
    static unsigned int CompileProgram(const char* vertex_src, const char* fragment_src);

    /// 销毁着色器程序
    void DeleteProgram(unsigned int handle);

    /// 初始化内置 PBR 着色器，编译并缓存所有 uniform location
    void InitBuiltinPBRShader();

    /// 清理所有着色器资源
    void Shutdown();

    // --- PBR 主着色器 ---
    unsigned int pbr_shader_handle() const { return pbr_shader_handle_; }
    const PBRShaderLocations& pbr_locations() const { return pbr_locations_; }

    // --- 天空盒着色器 ---
    unsigned int skybox_shader_handle() const { return skybox_shader_handle_; }
    const SkyboxShaderLocations& skybox_locations() const { return skybox_locations_; }
    void InitSkyboxShader();
    void set_skybox_shader_handle(unsigned int h) { skybox_shader_handle_ = h; }

    // --- 粒子着色器 ---
    unsigned int particle_shader_handle() const { return particle_shader_handle_; }
    const ParticleShaderLocations& particle_locations() const { return particle_locations_; }
    void InitParticleShader();
    void set_particle_shader_handle(unsigned int h) { particle_shader_handle_ = h; }

    // --- 后处理着色器缓存 ---
    unsigned int GetOrCreatePostProcessShader(const std::string& effect_name,
                                               const char* vs_src,
                                               const std::string& fs_src);
    bool HasPostProcessShader(const std::string& effect_name) const;

    /// 着色器程序计数（用于资源账本）
    std::size_t programs_created() const { return programs_created_; }
    std::size_t programs_destroyed() const { return programs_destroyed_; }

private:
    /// 缓存 PBR 着色器的所有 uniform location
    void CachePBRLocations();

    unsigned int pbr_shader_handle_ = 0;
    PBRShaderLocations pbr_locations_;

    unsigned int skybox_shader_handle_ = 0;
    SkyboxShaderLocations skybox_locations_;

    unsigned int particle_shader_handle_ = 0;
    ParticleShaderLocations particle_locations_;

    /// 后处理着色器缓存：effect_name → shader_program_handle
    std::unordered_map<std::string, unsigned int> pp_shaders_;

    /// 着色器程序创建/销毁计数
    std::size_t programs_created_ = 0;
    std::size_t programs_destroyed_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_SHADER_MANAGER_H

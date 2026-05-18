/**
 * @file dx11_shader_manager.h
 * @brief D3D11 着色器管理器 — HLSL 运行时编译与着色器程序管理
 *
 * 职责：
 * 1. HLSL 源码通过 D3DCompile 编译为字节码
 * 2. 创建 ID3D11VertexShader + ID3D11PixelShader
 * 3. 保存编译后的 VS 字节码用于 InputLayout 创建
 * 4. 内置着色器初始化（PBR/天空盒/粒子/精灵/后处理）
 */

#ifndef DSE_RENDER_DX11_SHADER_MANAGER_H
#define DSE_RENDER_DX11_SHADER_MANAGER_H

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

class DX11Context;

/// D3D11 着色器程序封装
struct DX11ShaderProgram {
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3DBlob> vs_blob;   ///< VS 编译字节码，用于创建 InputLayout
    ComPtr<ID3DBlob> ps_blob;
};

/// D3D11 Compute Shader 程序封装
struct DX11ComputeProgram {
    ComPtr<ID3D11ComputeShader> cs;
    ComPtr<ID3D11Buffer> params_cb;   ///< BloomParams CB（float2 src + float2 dst）
};

/**
 * @class DX11ShaderManager
 * @brief D3D11 着色器管理器
 */
class DX11ShaderManager {
public:
    DX11ShaderManager() = default;
    ~DX11ShaderManager() = default;

    /// 初始化
    void Init(DX11Context* context);

    /// 清理所有着色器资源
    void Shutdown();

    /// 从 HLSL 源码创建着色器程序，返回句柄（0 = 失败）
    unsigned int CreateProgram(const std::string& vert_src, const std::string& frag_src);

    /// 从 HLSL 源码创建着色器程序（自定义入口点），用于 spirv-cross 生成的 HLSL
    unsigned int CreateProgram(const std::string& vert_src, const std::string& frag_src,
                               const std::string& vs_entry, const std::string& ps_entry);

    /// 销毁着色器程序
    void DeleteProgram(unsigned int handle);

    /// 查询着色器程序
    const DX11ShaderProgram* GetProgram(unsigned int handle) const;

    /// 从 HLSL 源码创建 Compute Shader，返回句柄（0 = 失败）
    unsigned int CreateComputeProgram(const std::string& cs_src);

    /// 从 HLSL 源码创建 Compute Shader（自定义入口点），用于 spirv-cross 生成的 HLSL
    unsigned int CreateComputeProgram(const std::string& cs_src, const std::string& cs_entry);

    /// 查询 Compute Shader 程序
    const DX11ComputeProgram* GetComputeProgram(unsigned int handle) const;

    /// 初始化内置着色器
    void InitBuiltinShaders(std::function<void()> keep_alive = nullptr);

    /// 获取着色器对应的 InputLayout（由 InitBuiltinShaders 创建）
    ID3D11InputLayout* GetInputLayout(unsigned int shader_handle) const;

    // --- 内置着色器访问器 ---
    unsigned int pbr_shader_handle() const { return pbr_shader_handle_; }
    unsigned int skybox_shader_handle() const { return skybox_shader_handle_; }
    unsigned int particle_shader_handle() const { return particle_shader_handle_; }
    unsigned int sprite_shader_handle() const { return sprite_shader_handle_; }
    unsigned int postprocess_shader_handle() const { return postprocess_shader_handle_; }
    unsigned int shadow_shader_handle() const { return shadow_shader_handle_; }
    unsigned int bloom_extract_shader_handle() const { return bloom_extract_shader_handle_; }
    unsigned int bloom_downsample_cs_handle() const { return bloom_downsample_cs_handle_; }
    unsigned int bloom_upsample_cs_handle() const { return bloom_upsample_cs_handle_; }
    unsigned int bloom_composite_shader_handle() const { return bloom_composite_shader_handle_; }
    unsigned int bloom_composite_ssao_shader_handle() const { return bloom_composite_ssao_shader_handle_; }
    unsigned int fxaa_shader_handle() const { return fxaa_shader_handle_; }
    unsigned int ssao_shader_handle() const { return ssao_shader_handle_; }
    unsigned int ssao_blur_shader_handle() const { return ssao_blur_shader_handle_; }
    unsigned int ssao_apply_shader_handle() const { return ssao_apply_shader_handle_; }
    unsigned int contact_shadow_shader_handle() const { return contact_shadow_shader_handle_; }
    unsigned int lum_compute_shader_handle() const { return lum_compute_shader_handle_; }
    unsigned int lum_adapt_shader_handle() const { return lum_adapt_shader_handle_; }
    unsigned int tonemapping_shader_handle() const { return tonemapping_shader_handle_; }
    unsigned int bloom_composite_ssao_ae_shader_handle() const { return bloom_composite_ssao_ae_shader_handle_; }
    unsigned int color_grading_shader_handle() const { return color_grading_shader_handle_; }
    unsigned int taa_resolve_shader_handle() const { return taa_resolve_shader_handle_; }
    unsigned int dof_shader_handle() const { return dof_shader_handle_; }
    unsigned int motion_blur_shader_handle() const { return motion_blur_shader_handle_; }
    unsigned int ssr_shader_handle() const { return ssr_shader_handle_; }
    unsigned int motion_vector_shader_handle() const { return motion_vector_shader_handle_; }
    unsigned int gbuffer_shader_handle() const { return gbuffer_shader_handle_; }
    unsigned int deferred_lighting_shader_handle() const { return deferred_lighting_shader_handle_; }
    unsigned int edge_detect_shader_handle() const { return edge_detect_shader_handle_; }
    unsigned int volumetric_fog_shader_handle() const { return volumetric_fog_shader_handle_; }
    unsigned int decal_shader_handle() const { return decal_shader_handle_; }
    unsigned int wboit_composite_shader_handle() const { return wboit_composite_shader_handle_; }
    unsigned int water_shader_handle() const { return water_shader_handle_; }
    unsigned int light_shaft_shader_handle() const { return light_shaft_shader_handle_; }

    std::size_t programs_created() const { return programs_created_; }
    std::size_t programs_destroyed() const { return programs_destroyed_; }

private:
    /// 编译 HLSL 源码
    ComPtr<ID3DBlob> CompileShader(const std::string& source, const std::string& entry_point,
                                    const std::string& target);

    /// 为指定着色器创建 InputLayout
    void CreateInputLayoutForShader(unsigned int handle, const D3D11_INPUT_ELEMENT_DESC* elements, UINT count);

    DX11Context* context_ = nullptr;

    std::unordered_map<unsigned int, DX11ShaderProgram> programs_;
    unsigned int next_handle_ = 840000;

    unsigned int pbr_shader_handle_ = 0;
    unsigned int skybox_shader_handle_ = 0;
    unsigned int particle_shader_handle_ = 0;
    unsigned int sprite_shader_handle_ = 0;
    unsigned int postprocess_shader_handle_ = 0;
    unsigned int shadow_shader_handle_ = 0;
    unsigned int bloom_extract_shader_handle_ = 0;
    unsigned int bloom_downsample_cs_handle_ = 0;
    unsigned int bloom_upsample_cs_handle_ = 0;
    unsigned int bloom_composite_shader_handle_ = 0;
    unsigned int bloom_composite_ssao_shader_handle_ = 0;
    unsigned int fxaa_shader_handle_ = 0;
    unsigned int ssao_shader_handle_ = 0;
    unsigned int ssao_blur_shader_handle_ = 0;
    unsigned int ssao_apply_shader_handle_ = 0;
    unsigned int contact_shadow_shader_handle_ = 0;
    unsigned int lum_compute_shader_handle_ = 0;
    unsigned int lum_adapt_shader_handle_ = 0;
    unsigned int tonemapping_shader_handle_ = 0;
    unsigned int bloom_composite_ssao_ae_shader_handle_ = 0;
    unsigned int color_grading_shader_handle_ = 0;
    unsigned int taa_resolve_shader_handle_ = 0;
    unsigned int dof_shader_handle_ = 0;
    unsigned int motion_blur_shader_handle_ = 0;
    unsigned int ssr_shader_handle_ = 0;
    unsigned int motion_vector_shader_handle_ = 0;
    unsigned int gbuffer_shader_handle_ = 0;
    unsigned int deferred_lighting_shader_handle_ = 0;
    unsigned int edge_detect_shader_handle_ = 0;
    unsigned int volumetric_fog_shader_handle_ = 0;
    unsigned int decal_shader_handle_ = 0;
    unsigned int wboit_composite_shader_handle_ = 0;
    unsigned int water_shader_handle_ = 0;
    unsigned int light_shaft_shader_handle_ = 0;

    std::size_t programs_created_ = 0;
    std::size_t programs_destroyed_ = 0;

    /// 着色器句柄 → InputLayout 映射
    std::unordered_map<unsigned int, ComPtr<ID3D11InputLayout>> input_layouts_;

    /// Compute Shader 程序映射
    std::unordered_map<unsigned int, DX11ComputeProgram> compute_programs_;
    unsigned int next_cs_handle_ = 850000;

};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_SHADER_MANAGER_H

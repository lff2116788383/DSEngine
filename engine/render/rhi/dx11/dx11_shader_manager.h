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
#include "engine/render/rhi/shader_manager_base.h"

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
class DX11ShaderManager : public ShaderManagerBase {
public:
    DX11ShaderManager() { next_handle_ = 840000; }
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

    // 内置着色器句柄访问器继承自 ShaderManagerBase

private:
    /// 编译 HLSL 源码
    ComPtr<ID3DBlob> CompileShader(const std::string& source, const std::string& entry_point,
                                    const std::string& target);

    /// 为指定着色器创建 InputLayout
    void CreateInputLayoutForShader(unsigned int handle, const D3D11_INPUT_ELEMENT_DESC* elements, UINT count);

    DX11Context* context_ = nullptr;

    std::unordered_map<unsigned int, DX11ShaderProgram> programs_;

    /// 着色器句柄 → InputLayout 映射
    std::unordered_map<unsigned int, ComPtr<ID3D11InputLayout>> input_layouts_;

    /// Compute Shader 程序映射
    std::unordered_map<unsigned int, DX11ComputeProgram> compute_programs_;
    unsigned int next_cs_handle_ = 850000;

};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_SHADER_MANAGER_H

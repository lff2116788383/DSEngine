/**
 * @file shader_graph_codegen.h
 * @brief 后端无关的着色器图代码生成：ShaderGraphAsset → 着色器源码
 *
 * 在共享 .dshadergraph 资产契约（shader_graph_asset.h）之上，把节点图编译为
 * 具体后端的着色器源码。目前支持五个目标：
 *   - GLSL（#version 430，OpenGL；与引擎内建 GL 着色器一致）
 *   - HLSL（Shader Model 5.0，Direct3D 11）
 *   - GLSL_VULKAN（#version 450，Vulkan；显式 layout(location/binding)、UBO 化 u_time）
 *   - GLSL_ES（#version 300 es，WebGL2；precision 限定符、name-matched varying）
 *   - WGSL（WebGPU；@group/@binding 资源、struct I/O、textureSample）
 *
 * 生成过程与后端无关：同一份节点遍历/拓扑排序逻辑，通过 ShaderLang 抽象出
 * 类型名与内建函数差异（vec3/float3、mix/lerp、texture()/Sample() 等）。
 * 无法在目标后端表达的节点会记录 warning 并退化为安全默认值（不静默产错）。
 *
 * 该模块以源码生成为主。当构建链接了 glslang（DSE_HAS_GLSLANG）时，GenerateSpirv()
 * 还能把 Vulkan GLSL 450 输出编译为真实 SPIR-V；DXBC 仍由离线 dse_shader_compiler
 * 在此源码之上完成。
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/dse_export.h"
#include "engine/render/shader_graph/shader_graph_asset.h"

namespace dse {
namespace shadergraph {

enum class ShaderTarget {
    GLSL,         ///< OpenGL, #version 430
    HLSL,         ///< Direct3D 11, Shader Model 5.0
    GLSL_VULKAN,  ///< Vulkan, #version 450 (explicit layout locations/bindings)
    GLSL_ES,      ///< WebGL2, #version 300 es (precision-qualified)
    WGSL,         ///< WebGPU, WGSL text (@group/@binding, struct I/O)
};

struct ShaderCodegenResult {
    bool ok = false;
    std::string vertex;    ///< 顶点着色器源码
    std::string fragment;  ///< 片元/像素着色器源码
    std::vector<std::string> warnings;  ///< 目标后端无法表达的节点等
    std::vector<std::string> errors;    ///< 结构错误（图非法等）
};

DSE_EXPORT const char* ShaderTargetName(ShaderTarget target);

/// 生成顶点 + 片元着色器源码。图非法时 ok=false 并填充 errors。
DSE_EXPORT ShaderCodegenResult GenerateShader(const ShaderGraphAsset& graph,
                                              ShaderTarget target);

/// SPIR-V 编译结果。available=false 表示本次构建未链接 glslang（DSE_HAS_GLSLANG 未定义）。
struct ShaderSpirvResult {
    bool available = false;                 ///< 构建是否含 glslang
    bool ok = false;                        ///< 顶点 + 片元均成功编译为 SPIR-V
    std::vector<uint32_t> vertex_spirv;     ///< 顶点 SPIR-V 字（words）
    std::vector<uint32_t> fragment_spirv;   ///< 片元 SPIR-V 字（words）
    std::vector<std::string> errors;        ///< 图非法或 glslang 编译错误
};

/// 将节点图经 Vulkan GLSL 450 用 glslang 编译为真实 SPIR-V（顶点 + 片元）。
/// 若构建未链接 glslang，返回 available=false（不产错，由调用方决定是否走离线工具）。
DSE_EXPORT ShaderSpirvResult GenerateSpirv(const ShaderGraphAsset& graph);

/// DXBC 编译结果。available=false 表示本次构建未启用 D3D11/d3dcompiler（仅 Windows）。
struct ShaderDxbcResult {
    bool available = false;                 ///< 构建是否含 d3dcompiler（DSE_ENABLE_D3D11 + Windows）
    bool ok = false;                        ///< 顶点 + 片元均成功编译为 DXBC
    std::vector<uint8_t> vertex_dxbc;       ///< 顶点 DXBC 字节码（vs_5_0）
    std::vector<uint8_t> fragment_dxbc;     ///< 片元 DXBC 字节码（ps_5_0）
    std::vector<std::string> errors;        ///< 图非法或 D3DCompile 错误
};

/// 将节点图经 HLSL SM5 用 d3dcompiler 编译为真实 DXBC（VSMain vs_5_0 + PSMain ps_5_0）。
/// D3DCompile 不依赖 GPU，可无头运行；未启用 D3D11/非 Windows 时返回 available=false。
DSE_EXPORT ShaderDxbcResult GenerateDxbc(const ShaderGraphAsset& graph);

}  // namespace shadergraph
}  // namespace dse

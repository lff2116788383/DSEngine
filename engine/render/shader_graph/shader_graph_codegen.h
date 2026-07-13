/**
 * @file shader_graph_codegen.h
 * @brief 后端无关的着色器图代码生成：ShaderGraphAsset → 着色器源码
 *
 * 在共享 .dshadergraph 资产契约（shader_graph_asset.h）之上，把节点图编译为
 * 具体后端的着色器源码。目前支持四个目标：
 *   - GLSL（#version 430，OpenGL；与引擎内建 GL 着色器一致）
 *   - HLSL（Shader Model 5.0，Direct3D 11）
 *   - GLSL_VULKAN（#version 450，Vulkan；显式 layout(location/binding)、UBO 化 u_time）
 *   - GLSL_ES（#version 300 es，WebGL2；precision 限定符、name-matched varying）
 *
 * 生成过程与后端无关：同一份节点遍历/拓扑排序逻辑，通过 ShaderLang 抽象出
 * 类型名与内建函数差异（vec3/float3、mix/lerp、texture()/Sample() 等）。
 * 无法在目标后端表达的节点会记录 warning 并退化为安全默认值（不静默产错）。
 *
 * 该模块只做源码生成，不做编译/链接；SPIR-V/DXBC 由各后端离线或运行时编译器
 * 在此源码之上完成。
 */

#pragma once

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

}  // namespace shadergraph
}  // namespace dse
